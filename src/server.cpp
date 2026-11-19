#include "server.hpp"
#include "http.hpp"
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

HttpServer::HttpServer(ServerConfig config)
    : config_(std::move(config)),
      pool_(config_.threads, config_.queueCapacity),
      router_(config_.documentRoot, stats_) {}

HttpServer::~HttpServer() { stop(); }

bool HttpServer::run(const std::function<bool()>& stopRequested) {
    listenFd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd_ < 0) {
        std::cerr << "socket failed: " << std::strerror(errno) << '\n';
        return false;
    }
    int yes = 1;
    ::setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(static_cast<uint16_t>(config_.port));
    if (::inet_pton(AF_INET, config_.host.c_str(), &address.sin_addr) != 1) {
        std::cerr << "Invalid IPv4 bind address: " << config_.host << '\n';
        stop();
        return false;
    }
    if (::bind(listenFd_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        std::cerr << "bind failed: " << std::strerror(errno) << '\n';
        stop();
        return false;
    }
    if (::listen(listenFd_, 128) < 0) {
        std::cerr << "listen failed: " << std::strerror(errno) << '\n';
        stop();
        return false;
    }

    std::cout << "Listening on http://" << config_.host << ':' << config_.port
              << " with " << config_.threads << " workers\n";
    acceptLoop(stopRequested);
    stop();
    return true;
}

void HttpServer::acceptLoop(const std::function<bool()>& stopRequested) {
    while (!stopping_.load() &&
           !(stopRequested && stopRequested())) {
        pollfd pfd{listenFd_, POLLIN, 0};
        const int ready = ::poll(&pfd, 1, 500);
        if (ready < 0) {
            if (errno == EINTR) continue;
            std::cerr << "poll failed: " << std::strerror(errno) << '\n';
            break;
        }
        if (ready == 0) continue;
        sockaddr_in clientAddress{};
        socklen_t length = sizeof(clientAddress);
        const int clientFd = ::accept(listenFd_, reinterpret_cast<sockaddr*>(&clientAddress), &length);
        if (clientFd < 0) {
            if (errno == EINTR || errno == EAGAIN) continue;
            if (!stopping_.load()) std::cerr << "accept failed: " << std::strerror(errno) << '\n';
            continue;
        }
        timeval timeout{config_.ioTimeoutSeconds, 0};
        ::setsockopt(clientFd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        ::setsockopt(clientFd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
        stats_.accepted.fetch_add(1);

        if (!pool_.submit([this, clientFd] { handleClient(clientFd); })) {
            stats_.rejected.fetch_add(1);
            const std::string busy = serializeResponse({503, "text/plain", "Server busy\n"});
            sendAll(clientFd, busy);
            ::close(clientFd);
        }
    }
}

bool HttpServer::sendAll(int fd, const std::string& data) {
    std::size_t sent = 0;
    while (sent < data.size()) {
#ifdef MSG_NOSIGNAL
        const ssize_t n = ::send(fd, data.data() + sent, data.size() - sent, MSG_NOSIGNAL);
#else
        const ssize_t n = ::send(fd, data.data() + sent, data.size() - sent, 0);
#endif
        if (n < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        if (n == 0) return false;
        sent += static_cast<std::size_t>(n);
    }
    stats_.bytesSent.fetch_add(sent);
    return true;
}


void HttpServer::handleClient(int clientFd) {
    std::string raw;
    char buffer[4096];

    auto sendError = [&](int status, const std::string& message) {
        sendAll(clientFd, serializeResponse({
            status, "text/plain", message + "\n"
        }));
        ::close(clientFd);
    };

    // STEP 1: Read until the complete HTTP header is received.
    std::size_t headerEnd = std::string::npos;

    while (headerEnd == std::string::npos) {
        const ssize_t n = ::recv(clientFd, buffer, sizeof(buffer), 0);

        if (n < 0) {
            if (errno == EINTR) continue;
            break;
        }

        if (n == 0) break;

        raw.append(buffer, static_cast<std::size_t>(n));

        headerEnd = raw.find("\r\n\r\n");

        if (headerEnd == std::string::npos &&
            raw.size() > config_.maxHeaderBytes) {
            sendError(431, "Request Headers Too Large");
            return;
        }

        if (headerEnd != std::string::npos &&
            headerEnd + 4 > config_.maxHeaderBytes) {
            sendError(431, "Request Headers Too Large");
            return;
        }
    }

    // No complete header was received.
    if (headerEnd == std::string::npos) {
        sendError(400, "Incomplete HTTP Headers");
        return;
    }

    const std::size_t headerBytes = headerEnd + 4;

    // STEP 2: Validate headers before reading the body.
    ParseResult headerResult = parseHttpRequest(
        raw.substr(0, headerBytes),
        config_.maxBodyBytes
    );

    if (!headerResult.ok &&
        headerResult.errorMessage != "Incomplete request body") {

        sendError(
            headerResult.errorStatus,
            headerResult.errorMessage
        );
        return;
    }

    // STEP 3: Obtain the validated Content-Length.
    std::size_t contentLength = 0;

    auto it = headerResult.request.headers.find("content-length");

    if (it != headerResult.request.headers.end()) {
        contentLength = static_cast<std::size_t>(
            std::stoull(it->second)
        );
    }

    // Prevent size_t overflow.
    if (contentLength >
        std::string().max_size() - headerBytes) {
        sendError(413, "Request Too Large");
        return;
    }

    const std::size_t expectedTotal = headerBytes + contentLength;

    // STEP 4: Read until the complete request body arrives.
    while (raw.size() < expectedTotal) {
        const ssize_t n = ::recv(clientFd, buffer, sizeof(buffer), 0);

        if (n < 0) {
            if (errno == EINTR) continue;
            break;
        }

        if (n == 0) break;

        raw.append(buffer, static_cast<std::size_t>(n));
    }

    // STEP 5: Parse the complete request.
    if (raw.size() < expectedTotal) {
        sendError(400, "Incomplete Request Body");
        return;
    }

    ParseResult parsed = parseHttpRequest(
        raw.substr(0, expectedTotal),
        config_.maxBodyBytes
    );

    if (!parsed.ok) {
        sendError(parsed.errorStatus, parsed.errorMessage);
        return;
    }

    // STEP 6: Route the request.
    HttpResponse response = router_.handle(parsed.request);

    const bool head = parsed.request.method == "HEAD";

    const std::string wire = serializeResponse(response, head);

    sendAll(clientFd, wire);

    stats_.completed.fetch_add(1);

    std::cout << parsed.request.method << ' '
              << parsed.request.path
              << " -> " << response.status << '\n';

    ::close(clientFd);
}


void HttpServer::stop() {
    if (stopping_.exchange(true)) return;
    if (listenFd_ >= 0) {
        ::shutdown(listenFd_, SHUT_RDWR);
        ::close(listenFd_);
        listenFd_ = -1;
    }
    pool_.shutdown();
}
