#pragma once
#include "http.hpp"
#include <atomic>
#include <string>

struct ServerStats {
    std::atomic<unsigned long long> accepted{0};
    std::atomic<unsigned long long> completed{0};
    std::atomic<unsigned long long> rejected{0};
    std::atomic<unsigned long long> bytesSent{0};
};

class Router {
public:
    Router(std::string documentRoot, ServerStats& stats);
    HttpResponse handle(const HttpRequest& request);
private:
    std::string documentRoot_;
    ServerStats& stats_;
    HttpResponse serveFile(const std::string& relativePath);
};
