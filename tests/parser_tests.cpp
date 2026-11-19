#include "http.hpp"
#include <cassert>
#include <iostream>

int main() {
    {
        const std::string raw =
            "GET /health HTTP/1.1\r\nHost: localhost\r\n\r\n";
        auto parsed = parseHttpRequest(raw, 1024);
        assert(parsed.ok);
        assert(parsed.request.method == "GET");
        assert(parsed.request.path == "/health");
        assert(parsed.request.headers.at("host") == "localhost");
    }
    {
        const std::string raw =
            "POST /echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\n\r\nhello";
        auto parsed = parseHttpRequest(raw, 1024);
        assert(parsed.ok);
        assert(parsed.request.body == "hello");
    }
    {
        const std::string raw = "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n";
        auto parsed = parseHttpRequest(raw, 1);
        assert(parsed.ok);
    }
    {
        const std::string raw =
            "POST /echo HTTP/1.1\r\nContent-Length: 9\r\n\r\n123456789";
        auto parsed = parseHttpRequest(raw, 4);
        assert(!parsed.ok);
        assert(parsed.errorStatus == 413);
    }
    {
        const std::string raw = "nonsense\r\n\r\n";
        auto parsed = parseHttpRequest(raw, 1024);
        assert(!parsed.ok);
    }

    // 1. Duplicate Content-Length must be rejected.
    {
        const std::string raw =
            "POST /echo HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "Content-Length: 5\r\n"
            "Content-Length: 10\r\n\r\n"
            "hello";

        auto parsed = parseHttpRequest(raw, 1024);
        assert(!parsed.ok);
    }

    // 2. HTTP/1.1 requires a Host header.
    {
        const std::string raw =
            "GET /health HTTP/1.1\r\n\r\n";

        auto parsed = parseHttpRequest(raw, 1024);
        assert(!parsed.ok);
    }

    // 3. Negative Content-Length must be rejected.
    {
        const std::string raw =
            "POST /echo HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "Content-Length: -5\r\n\r\n";

        auto parsed = parseHttpRequest(raw, 1024);
        assert(!parsed.ok);
    }

    // 4. Non-numeric Content-Length must be rejected.
    {
        const std::string raw =
            "POST /echo HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "Content-Length: abc\r\n\r\n";

        auto parsed = parseHttpRequest(raw, 1024);
        assert(!parsed.ok);
    }

    // 5. Transfer-Encoding is unsupported.
    {
        const std::string raw =
            "POST /echo HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "Transfer-Encoding: chunked\r\n\r\n";

        auto parsed = parseHttpRequest(raw, 1024);
        assert(!parsed.ok);
    }

    // 6. Incomplete request body must be rejected.
    {
        const std::string raw =
            "POST /echo HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "Content-Length: 5\r\n\r\n"
            "hel";

        auto parsed = parseHttpRequest(raw, 1024);
        assert(!parsed.ok);
    }

    std::cout << "Parser tests passed\n";
}
