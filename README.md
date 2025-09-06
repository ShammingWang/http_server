# cpp-web-server (sprint-ready)

High-level, resume-friendly C++20 HTTP server scaffold you can extend into a high-performance project. This baseline runs on Windows and Linux using a simple select-based event loop, with routing and static file serving. Next steps include epoll/IOCP, thread pool, zero-copy, metrics, and tests.

Features (baseline)
- C++20 + CMake project structure
- Minimal HTTP/1.1 parser (request line, headers, Content-Length body)
- Router with GET/POST handlers and static file serving under `/static`
- Non-blocking sockets + select-based loop (Windows/Linux)
- Example: `/hello` returns JSON

Planned enhancements (2-week sprint targets)
- Epoll (Linux) and IOCP (Windows) backends; timer wheel for keep-alive timeouts
- Thread pool for handler execution; backpressure control
- Zero-copy static files (sendfile/TransmitFile) and LRU file cache
- Middleware: logging, rate limiting; Prometheus `/metrics`
- CI, tests (parsers, router, LRU), ASAN/TSAN, clang-tidy

Build
```
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release
```

Run example
```
# Windows
./Release/hello.exe

# Linux/macOS
./hello
```

Try it
- Hello route: `curl http://127.0.0.1:8080/hello`
- Static files: put files under `static/`, then `curl http://127.0.0.1:8080/static/yourfile`

Repo layout
```
include/
  http/HttpRequest.h HttpResponse.h HttpParser.h Router.h
  server/Server.h
src/
  http/HttpParser.cpp
  server/Server.cpp
examples/hello_world.cpp
```

Notes
- This baseline uses select for portability; FD_SETSIZE limits may apply on Windows.
- Keep-alive is supported; pipelining is not. Chunked encoding is not yet implemented.
- Static file serving reads whole files into memory; replace with zero-copy later.

## 平台抽象更新
已将原先分散在 Server 中的 `#ifdef` 套接字差异封装到:
- include/server/PlatformSocket.h
- src/platform/Socket_win.cpp / Socket_posix.cpp

CMake 会依据平台自动编译对应实现，便于后续扩展 epoll / IOCP。

