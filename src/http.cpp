#include "http.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <limits>
#include <stdexcept>
std::string toLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::string reasonPhrase(int status) {
    switch (status) {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 400: return "Bad Request";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 413: return "Payload Too Large";
        case 431: return "Request Header Fields Too Large";
        case 500: return "Internal Server Error";
        case 503: return "Service Unavailable";
        default: return "Error";
    }
}


ParseResult parseHttpRequest(const std::string& raw, std::size_t maxBodyBytes) {
    ParseResult result;

    const auto headerEnd = raw.find("\r\n\r\n");

    if (headerEnd == std::string::npos) {
        result.errorMessage = "Incomplete headers";
        return result;
    }

    std::istringstream stream(raw.substr(0, headerEnd));

    std::string line;

    if (!std::getline(stream, line)) {
        return result;
    }

    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }

    std::istringstream requestLine(line);

    if (!(requestLine >> result.request.method
                     >> result.request.target
                     >> result.request.version)) {
        return result;
    }

    std::string extra;

    if (requestLine >> extra) {
        return result;
    }

    if (result.request.version != "HTTP/1.1" &&
        result.request.version != "HTTP/1.0") {
        return result;
    }

    if (result.request.target.empty() ||
        result.request.target.front() != '/') {
        return result;
    }

    result.request.path =
        result.request.target.substr(
            0,
            result.request.target.find('?')
        );

    while (std::getline(stream, line)) {

        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        if (line.empty()) continue;

        const auto colon = line.find(':');

        if (colon == std::string::npos || colon == 0) {
            return result;
        }

        std::string key = line.substr(0, colon);

        // Validate header-name characters.
        for (unsigned char c : key) {
            if (!(std::isalnum(c) ||
                  std::string("!#$%&'*+-.^_`|~").find(c)
                      != std::string::npos)) {
                return result;
            }
        }

        key = toLower(key);

        std::string value = line.substr(colon + 1);

        const auto first = value.find_first_not_of(" \t");

        if (first == std::string::npos) {
            value.clear();
        } else {
            value.erase(0, first);
        }

        // Reject control characters in header values.
        for (unsigned char c : value) {
            if ((c < 32 && c != '\t') || c == 127) {
                return result;
            }
        }

        // Reject duplicate Content-Length.
        if (key == "content-length" &&
            result.request.headers.count(key)) {
            return result;
        }

        result.request.headers[key] = value;
    }

    if (result.request.version == "HTTP/1.1" &&
        !result.request.headers.count("host")) {
        return result;
    }

    if (result.request.headers.count("transfer-encoding")) {
        result.errorMessage = "Transfer-Encoding is not supported";
        return result;
    }

    std::size_t contentLength = 0;

    auto it = result.request.headers.find("content-length");

    if (it != result.request.headers.end()) {

        const std::string& value = it->second;

        if (value.empty() ||
            !std::all_of(value.begin(), value.end(),
                [](unsigned char c) {
                    return c >= '0' && c <= '9';
                })) {
            return result;
        }

        try {
            const unsigned long long parsed = std::stoull(value);

            if (parsed > maxBodyBytes ||
                parsed > std::numeric_limits<std::size_t>::max()) {

                result.errorStatus = 413;
                result.errorMessage = "Payload Too Large";
                return result;
            }

            contentLength = static_cast<std::size_t>(parsed);
        }
        catch (...) {
            return result;
        }
    }

    const std::size_t bodyStart = headerEnd + 4;
    const std::size_t available = raw.size() - bodyStart;

    if (available < contentLength) {
        result.errorMessage = "Incomplete request body";
        return result;
    }

    result.request.body = raw.substr(bodyStart, contentLength);

    result.ok = true;

    return result;
}

std::string serializeResponse(const HttpResponse& response, bool headOnly) {
    std::ostringstream out;
    out << "HTTP/1.1 " << response.status << ' ' << reasonPhrase(response.status) << "\r\n";
    out << "Content-Type: " << response.contentType << "\r\n";
    out << "Content-Length: " << response.body.size() << "\r\n";
    out << "Connection: close\r\n";
    out << "X-Content-Type-Options: nosniff\r\n";
    out << "\r\n";
    if (!headOnly) out << response.body;
    return out.str();
}
