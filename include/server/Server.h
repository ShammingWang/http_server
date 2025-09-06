#pragma once
#include <atomic>
#include <unordered_map>
#include <string>

#include "http/Router.h"
#include "http/HttpParser.h"
#include "server/PlatformSocket.h" // 提供 socket_t / is_valid_socket / closesocket / set_socket_nonblocking

namespace net {

struct Connection {
    socket_t         fd{};           // 默认初始化
    std::string      inbuf;
    std::string      outbuf;
    http::HttpParser parser;
    bool             keep_alive{true};
};

class Server {
public:
    explicit Server(uint16_t port);
    ~Server();

    // 避免句柄被复制
    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;
    Server(Server&&) = delete;
    Server& operator=(Server&&) = delete;

    void set_router(const http::Router* r) noexcept { router_ = r; }

    [[nodiscard]] bool listen_and_serve(); // 失败返回 false
    void stop() noexcept;

private:
    [[nodiscard]] bool setup_socket();
    static void        close_socket(socket_t s) noexcept;
    static bool        set_nonblocking(socket_t s) noexcept;
    [[nodiscard]] bool handle_read(Connection& c);
    [[nodiscard]] bool handle_write(Connection& c);

private:
    uint16_t                                 port_{};
    socket_t                                 listen_fd_{};
    std::unordered_map<socket_t, Connection> conns_;
    std::atomic<bool>                        running_{false};
    const http::Router*                      router_{nullptr};
};

} // namespace net
