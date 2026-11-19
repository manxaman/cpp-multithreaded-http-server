# Multithreaded HTTP Server (C++17)

A portfolio-ready educational HTTP/1.1 server built from scratch with POSIX TCP sockets,
a fixed-size worker pool, a bounded thread-safe task queue, basic routing, static-file
serving, request limits, graceful shutdown, and operational statistics.

> Target: Linux (Ubuntu / WSL2). The server uses POSIX APIs (`socket`, `bind`, `listen`,
> `accept`, `poll`, `send`, `recv`, `sigaction`, `close`) and is not a native Windows
> executable. Docker deployment is included.

## Features

- TCP listener with configurable bind address and port.
- Fixed-size worker pool (no thread-per-request).
- Bounded producer/consumer queue with mutex + condition variable.
- HTTP/1.1 request-line and header parsing, `Content-Length` body handling.
- `GET` and `HEAD` routes, plus `POST /echo`.
- `/`, `/health`, `/hello`, `/stats`, `/files/<path>`.
- Static-file path traversal protection and a configurable maximum response size.
- Connection and request timeouts; one request per connection, then close.
- Graceful shutdown on SIGINT/SIGTERM.
- Thread-safe counters and request logging to stdout.
- CMake build, unit-ish parser tests, Dockerfile, Compose, GitHub Actions CI,
  and a simple concurrent load generator.

## Quick start (Ubuntu / WSL2)

Install tools:

```bash
sudo apt update
sudo apt install -y build-essential cmake git curl
```

Build and run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/http_server
```

Open another terminal:

```bash
curl -i http://127.0.0.1:8080/
curl -i http://127.0.0.1:8080/health
curl -i http://127.0.0.1:8080/hello
curl -i http://127.0.0.1:8080/stats
curl -i -X POST --data 'hello from curl' http://127.0.0.1:8080/echo
curl -i http://127.0.0.1:8080/files/example.txt
```

Stop with Ctrl+C.

## Configuration

Environment variables:

| Variable | Default | Meaning |
|---|---:|---|
| `HTTP_HOST` | `0.0.0.0` | Bind address |
| `HTTP_PORT` | `8080` | TCP port |
| `HTTP_THREADS` | hardware concurrency (minimum 2) | Worker count |
| `HTTP_QUEUE_CAPACITY` | `256` | Maximum queued sockets |
| `HTTP_DOC_ROOT` | `./public` | Static-file directory |
| `HTTP_MAX_HEADER_BYTES` | `16384` | Maximum header bytes |
| `HTTP_MAX_BODY_BYTES` | `1048576` | Maximum request body |
| `HTTP_MAX_FILE_BYTES` | `5242880` | Maximum static file |
| `HTTP_IO_TIMEOUT_SECONDS` | `5` | Per socket I/O timeout |

Example:

```bash
HTTP_PORT=9000 HTTP_THREADS=8 ./build/http_server
```

## API

| Method | Path | Result |
|---|---|---|
| GET / HEAD | `/` | Welcome HTML |
| GET / HEAD | `/health` | JSON health status |
| GET / HEAD | `/hello` | Plain text greeting |
| GET / HEAD | `/stats` | JSON counters |
| POST | `/echo` | Echoes request body |
| GET / HEAD | `/files/<name>` | Serves a file under `public/` |

Unsupported methods return `405`; unknown routes return `404`; malformed requests
return an appropriate `4xx` response. Connections are deliberately closed after one
request (the response includes `Connection: close`) to keep the first implementation
small and auditable. Chunked request bodies and TLS are not implemented.

## Tests and load test

```bash
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Start the server, then in another terminal:

```bash
python3 benchmarks/load_test.py --url http://127.0.0.1:8080/health --requests 1000 --concurrency 50
```

The script reports completed requests, errors, elapsed time, throughput, and latency
percentiles. It is a lightweight smoke benchmark, not a substitute for `wrk`, `hey`,
or a controlled performance study. Compare builds on the same machine with the same
workload; do not put unmeasured performance claims on a resume.

## Docker

```bash
docker build -t cpp-http-server .
docker run --rm -p 8080:8080 cpp-http-server
```

Or:

```bash
docker compose up --build
```

The container listens on port 8080. It runs as a non-root user.

## Deploying for a resume

The repository includes a Dockerfile suitable for a container host. A free hosting
provider's plan, sleep policy, port detection, and C++/Docker support can change; verify
current terms before choosing a host. Free instances commonly sleep when idle, so the
first request may be slow. Do not expose this educational server to untrusted traffic
without further hardening (TLS termination, stronger parser tests, rate limiting,
resource limits, and security review).

Recommended portfolio checklist:

1. Push this directory to a public GitHub repository.
2. Add screenshots / benchmark results from your own machine.
3. Deploy the Docker image to a free container service that currently supports it.
4. Add the live `/health` URL and GitHub URL to your resume.
5. Keep the service alive within the provider's free-tier constraints.

## Architecture

```text
TCP clients
    |
    v
Server::acceptLoop()
    |
    v
Bounded TaskQueue<int> -- mutex + condition_variable
    |
    v
N worker threads (ThreadPool)
    |
    v
handleClient() -> parseRequest() -> Router -> HttpResponse -> send()
```

The accept thread is the producer. Worker threads are consumers. If the bounded queue
is full, the accept loop sends `503 Service Unavailable` and closes the client socket.
The queue closes during shutdown, wakes waiting workers, and drains already accepted
tasks before workers exit.

## Known limitations / next improvements

- HTTP/1.1 keep-alive and pipelining are intentionally omitted.
- No TLS, HTTP/2, chunked transfer encoding, multipart parsing, or CGI.
- Basic HTTP parser; extend fuzz and malformed-input tests before production use.
- Static files are read into memory up to the configured size limit.
- A slow client can occupy a worker until the configured socket timeout.
- Consider `epoll`/`kqueue`, structured logging, rate limiting, connection caps,
  sanitizers, fuzzing, and a more comprehensive benchmark as future enhancements.
