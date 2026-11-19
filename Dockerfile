FROM ubuntu:24.04 AS build
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    g++ cmake make && rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY . .
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF \
    && cmake --build build -j"$(nproc)"

FROM ubuntu:24.04
RUN useradd --system --uid 10001 --create-home appuser
WORKDIR /app
COPY --from=build /app/build/http_server /app/http_server
COPY --from=build /app/public /app/public
USER appuser
ENV HTTP_HOST=0.0.0.0 HTTP_PORT=8080 HTTP_DOC_ROOT=/app/public
EXPOSE 8080
CMD ["/app/http_server"]
