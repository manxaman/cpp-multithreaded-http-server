#pragma once
#include <cstddef>
#include <string>
#include <unordered_map>

struct HttpRequest {
    std::string method;
    std::string target;
    std::string path;
    std::string version;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
};

struct HttpResponse {
    int status{200};
    std::string contentType{"text/plain; charset=utf-8"};
    std::string body;
};

struct ParseResult {
    bool ok{false};
    int errorStatus{400};
    std::string errorMessage{"Bad Request"};
    HttpRequest request;
};

ParseResult parseHttpRequest(const std::string& raw, std::size_t maxBodyBytes);
std::string reasonPhrase(int status);
std::string serializeResponse(const HttpResponse& response, bool headOnly = false);
std::string toLower(std::string value);
