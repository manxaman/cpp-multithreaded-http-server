
#include "server.hpp"

#include <algorithm>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <thread>

namespace {

volatile std::sig_atomic_t g_stopRequested = 0;

void handleSignal(int) {
    g_stopRequested = 1;
}

std::string envOr(const char* key, const std::string& fallback) {
    const char* value = std::getenv(key);
    return value ? value : fallback;
}

std::size_t envSize(const char* key, std::size_t fallback) {
    const char* value = std::getenv(key);

    if (!value) return fallback;

    try {
        return static_cast<std::size_t>(std::stoull(value));
    }
    catch (...) {
        return fallback;
    }
}

}

int main() {

    ServerConfig config;

    config.host = envOr("HTTP_HOST", config.host);
    config.port = static_cast<int>(
        envSize("HTTP_PORT", config.port)
    );

    config.threads = envSize(
        "HTTP_THREADS",
        std::max<std::size_t>(
            2,
            std::thread::hardware_concurrency()
        )
    );

    config.queueCapacity = envSize(
        "HTTP_QUEUE_CAPACITY",
        config.queueCapacity
    );

    config.documentRoot = envOr(
        "HTTP_DOC_ROOT",
        config.documentRoot
    );

    config.maxHeaderBytes = envSize(
        "HTTP_MAX_HEADER_BYTES",
        config.maxHeaderBytes
    );

    config.maxBodyBytes = envSize(
        "HTTP_MAX_BODY_BYTES",
        config.maxBodyBytes
    );

    config.ioTimeoutSeconds = static_cast<int>(
        envSize(
            "HTTP_IO_TIMEOUT_SECONDS",
            config.ioTimeoutSeconds
        )
    );

    if (config.port < 1 ||
        config.port > 65535 ||
        config.threads == 0) {

        std::cerr << "Invalid port or worker count\n";
        return 2;
    }

    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    HttpServer server(config);

    const bool ok = server.run([] {
    return g_stopRequested != 0;
});

    return ok ? 0 : 1;
}
