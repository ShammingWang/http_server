# cpp-web-server

A minimal, extensible C++20 HTTP/1.1 server skeleton (Windows + Linux) using non-blocking sockets and a select-based event loop. Designed as a clean starting point for evolving into a higher‑performance framework (epoll / IOCP, zero‑copy, thread pool, metrics, etc.).

轻量可扩展的 C++20 简易 HTTP/1.1 服务器脚手架，当前基于非阻塞 socket + select 事件循环，支持路由与静态文件映射，便于逐步演进为高性能版本。

---

## 1. Core Features
- C++20 / CMake project layout (header + src + examples)
- Cross‑platform socket abstraction (Windows WSA / POSIX)
- Non‑blocking sockets + select loop
- Simple HTTP/1.1 request parser (request line + headers + Content-Length body)
- Keep-Alive support (no pipelining yet)
- Router (GET / POST + custom verbs)
- Static file serving under configurable URL prefix
- Basic MIME inference (html/json/css/js/images/fonts/pdf inline)
- Clean separation: networking / parsing / routing

---

## 2. Quick Start

Build (default example enabled):
```
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release
```

Run example server (port 8080):
```
# Windows
./Release/hello.exe
# Linux / macOS
./hello
```

Test routes:
```
curl http://127.0.0.1:8080/hello
curl http://127.0.0.1:8080/static/anyfile
```

---

## 3. Minimal Example (see examples/hello_world.cpp)
```cpp
http::Router router;
router.get("/hello", [](const http::HttpRequest& req, http::HttpResponse& resp) {
    resp.set_content_type("application/json");
    resp.body = R"({"message":"Hello"})";
});
router.set_static("/static", "static");

net::Server server(8080);
server.set_router(&router);
server.listen_and_serve();
```

---

## 4. Public API Snapshot

Server:
- Server(uint16_t port)
- void set_router(const http::Router*)
- bool listen_and_serve()
- void stop()

Router:
- void get(path, Handler)
- void post(path, Handler)
- void add(Method, path, Handler)
- void set_static(url_prefix, dir_root)
- bool route(request, response)

Handler signature:
```
using Handler = std::function<void(const HttpRequest&, HttpResponse&)>;
```

HttpRequest (essentials):
- Method method
- std::string path, query, body
- headers map
- bool keep_alive() const

HttpResponse:
- int status (default 200)
- std::string body
- set_content_type(), set_header(), set_keep_alive()
- std::string to_string()

---

## 5. Directory Layout
```
include/
  http/ (HttpRequest.h HttpResponse.h HttpParser.h Router.h)
  server/ (Server.h PlatformSocket.h)
src/
  http/HttpParser.cpp
  server/Server.cpp
  platform/Socket_win.cpp | Socket_posix.cpp
examples/hello_world.cpp
CMakeLists.txt
```

---

## 6. Design Notes
Networking:
- Single-threaded select loop for portability
- All client sockets set non-blocking
- Outbound buffering per connection (std::string)
- Close after response if !keep-alive or error

Parsing:
- Line-based CRLF parsing
- Content-Length only (no chunked)
- Single request per connection at a time (no pipelining)

Static Files:
- Fully read into memory (future: sendfile / mmap / caching)
- Naive extension-based MIME
- Basic path traversal guard

Error Handling:
- 400 on parse failure
- 404 on missing route / file
- 413 on oversized request body (>1MB default)

---

## 7. Current Limitations (Intentional)
- select() scaling limits (FD_SETSIZE / O(N) scan)
- No TLS
- No chunked encoding / streaming
- No compression
- No request pipelining
- No timeout management (idle / header / keep-alive)
- No backpressure strategy besides kernel EWOULDBLOCK
- No logging / metrics / access logs
- No unit tests yet

---

## 8. Roadmap (Planned Evolution)
Networking:
- epoll (Linux) / IOCP (Windows) / kqueue (BSD)
- Timer wheel: idle + keep-alive + request deadlines
- Connection limits + accept throttling

Performance:
- Zero-copy static serving (sendfile / TransmitFile)
- Optional mmap + small-file LRU cache
- Write coalescing + scatter/gather (writev / WSASend)

Concurrency:
- Thread pool (handler execution)
- Lock-free queues or work stealing
- NUMA-aware batching (later phase)

HTTP Features:
- Chunked Transfer-Encoding
- Streaming responses
- Multipart (optional)
- Graceful shutdown + draining

Observability:
- Structured logging (JSON)
- Prometheus /metrics
- Latency + error histograms

Security / Hardening:
- Request size & header count caps
- Slowloris mitigation (header timeout)
- Basic rate limiting middleware

Tooling:
- Unit tests (parsers, router)
- Integration tests (curl harness)
- CI pipeline (GitHub Actions)
- clang-tidy, clang-format, ASAN/TSAN builds

---

## 9. Performance Tips (Baseline)
- Build Release (-O2 / /O2)
- Prefer persistent connections (curl --keepalive is default in HTTP/1.1)
- Avoid giant static files until zero-copy added
- Watch FD usage (ulimit -n on Linux)
- Profiling: Linux (perf), Windows (VS Profiler)

---

## 10. Extending
Suggested starting points:
1. Add logging middleware (wrap route dispatch)
2. Implement epoll backend (parallel to select)
3. Replace static file read loop with sendfile
4. Introduce connection timeout manager
5. Add request/response abstraction layers (middleware chain)

---

## 11. Contributing
- Fork + feature branch
- Keep changes focused
- Run formatting (clang-format) if integrated
- Provide brief benchmark or rationale for perf-sensitive patches

---

## 12. License
(Choose one: MIT / Apache-2.0 / BSD-2-Clause) – not yet specified.

---

## 13. FAQ
Q: Why not use Boost.Beast or existing frameworks?  
A: Educational + controlled evolution toward a performance-oriented stack.

Q: Is it production ready?  
A: Not yet—baseline only, missing robustness (timeouts, security, tests).

---

## 14. 中文快速使用
1. 编译：参考上方 Build
2. 添加路由：router.get("/hi", handler)
3. 静态资源：router.set_static("/static", "static")
4. 运行：访问 http://127.0.0.1:8080/hello

---

Keep it small, readable, and easy to evolve.

