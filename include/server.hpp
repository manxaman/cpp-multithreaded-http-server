#pragma once
#include "router.hpp"
#include "thread_pool.hpp"
#include <atomic>
#include <cstddef>
#include <string>
#include <functional>

struct ServerConfig {
    std::string host{"0.0.0.0"};
    int port{8080};
    std::size_t threads{4};
    std::size_t queueCapacity{256};
    std::size_t maxHeaderBytes{16384};
    std::size_t maxBodyBytes{1024 * 1024};
    int ioTimeoutSeconds{5};
    std::string documentRoot{"public"};
};

class HttpServer {
public:
    explicit HttpServer(ServerConfig config);
    ~HttpServer();
    bool run(const std::function<bool()>& stopRequested = {});
    void stop();
private:
    void acceptLoop(const std::function<bool()>& stopRequested);
    void handleClient(int clientFd);
    bool sendAll(int fd, const std::string& data);

    ServerConfig config_;
    ThreadPool pool_;
    ServerStats stats_;
    Router router_;
    std::atomic<bool> stopping_{false};
    int listenFd_{-1};
};
