#!/usr/bin/env python3
\"\"\"Small concurrent HTTP smoke benchmark; uses only Python standard library.\"\"\"
import argparse
import concurrent.futures
import statistics
import time
import urllib.error
import urllib.request

def percentile(values, p):
    if not values:
        return 0.0
    ordered = sorted(values)
    index = min(len(ordered) - 1, max(0, int((p / 100) * len(ordered))))
    return ordered[index]

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--url", default="http://127.0.0.1:8080/health")
    parser.add_argument("--requests", type=int, default=1000)
    parser.add_argument("--concurrency", type=int, default=50)
    args = parser.parse_args()

    latencies, errors = [], 0
    def one(_):
        start = time.perf_counter()
        try:
            with urllib.request.urlopen(args.url, timeout=10) as response:
                response.read()
                if response.status != 200:
                    raise RuntimeError(f"HTTP {response.status}")
            return time.perf_counter() - start, True
        except Exception:
            return time.perf_counter() - start, False

    start_all = time.perf_counter()
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.concurrency) as pool:
        for latency, ok in pool.map(one, range(args.requests)):
            latencies.append(latency)
            if not ok:
                errors += 1
    elapsed = time.perf_counter() - start_all
    completed = args.requests - errors
    print(f"URL: {args.url}")
    print(f"Requests: {args.requests} | Concurrency: {args.concurrency}")
    print(f"Completed: {completed} | Errors: {errors}")
    print(f"Elapsed: {elapsed:.3f}s | Throughput: {completed / elapsed:.2f} req/s")
    print(f"Latency ms: mean={statistics.mean(latencies)*1000:.2f}, "
          f"p50={percentile(latencies,50)*1000:.2f}, "
          f"p95={percentile(latencies,95)*1000:.2f}, "
          f"p99={percentile(latencies,99)*1000:.2f}")

if __name__ == "__main__":
    main()
