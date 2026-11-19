#include "router.hpp"
#include <fstream>
#include <sstream>
#include <vector>

namespace {
std::string jsonEscape(const std::string& value) {
    std::string out;
    for (char c : value) {
        if (c == '"' || c == '\\') out.push_back('\\');
        if (c == '\n') { out += "\\n"; continue; }
        if (c == '\r') { out += "\\r"; continue; }
        out.push_back(c);
    }
    return out;
}
std::string contentTypeFor(const std::string& path) {
    const auto dot = path.find_last_of('.');
    const std::string ext = dot == std::string::npos ? "" : path.substr(dot + 1);
    if (ext == "html") return "text/html; charset=utf-8";
    if (ext == "css") return "text/css; charset=utf-8";
    if (ext == "js") return "application/javascript";
    if (ext == "json") return "application/json";
    if (ext == "png") return "image/png";
    if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
    if (ext == "svg") return "image/svg+xml";
    return "text/plain; charset=utf-8";
}
}

Router::Router(std::string documentRoot, ServerStats& stats)
    : documentRoot_(std::move(documentRoot)), stats_(stats) {}

HttpResponse Router::handle(const HttpRequest& request) {
    const bool head = request.method == "HEAD";
    if (request.method != "GET" && !head && request.method != "POST")
        return {405, "text/plain; charset=utf-8", "Method Not Allowed\n"};

    if (request.path == "/" && (request.method == "GET" || head))
        return {200, "text/html; charset=utf-8",
            "<!doctype html><html><head><title>C++ HTTP Server</title></head>"
            "<body><h1>It works!</h1><p>Multithreaded C++ HTTP server.</p>"
            "<p>Try <a href='/health'>/health</a>, <a href='/stats'>/stats</a>.</p>"
            "</body></html>\n"};

    if (request.path == "/health" && (request.method == "GET" || head))
        return {200, "application/json", "{\"status\":\"ok\",\"service\":\"cpp-http-server\"}\n"};

    if (request.path == "/hello" && (request.method == "GET" || head))
        return {200, "text/plain; charset=utf-8", "Hello from a multithreaded C++ server!\n"};

    if (request.path == "/stats" && (request.method == "GET" || head)) {
        std::ostringstream body;
        body << "{\"accepted\":" << stats_.accepted.load()
             << ",\"completed\":" << stats_.completed.load()
             << ",\"rejected\":" << stats_.rejected.load()
             << ",\"bytes_sent\":" << stats_.bytesSent.load() << "}\n";
        return {200, "application/json", body.str()};
    }

    if (request.path == "/echo" && request.method == "POST")
        return {200, "text/plain; charset=utf-8", request.body};

    const std::string prefix = "/files/";
    if ((request.method == "GET" || head) && request.path.rfind(prefix, 0) == 0)
        return serveFile(request.path.substr(prefix.size()));

    return {404, "text/plain; charset=utf-8", "Not Found\n"};
}

HttpResponse Router::serveFile(const std::string& relativePath) {
    if (relativePath.empty() || relativePath.find("..") != std::string::npos ||
        relativePath.front() == '/' || relativePath.find('\\') != std::string::npos)
        return {403, "text/plain; charset=utf-8", "Forbidden\n"};

    const std::string fullPath = documentRoot_ + "/" + relativePath;
    std::ifstream file(fullPath, std::ios::binary);
    if (!file) return {404, "text/plain; charset=utf-8", "Not Found\n"};

    std::ostringstream contents;
    contents << file.rdbuf();
    std::string body = contents.str();
    if (body.size() > 5 * 1024 * 1024)
        return {413, "text/plain; charset=utf-8", "File Too Large\n"};
    return {200, contentTypeFor(relativePath), std::move(body)};
}
