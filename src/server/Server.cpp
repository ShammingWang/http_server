#include "server/Server.h"
#include "server/PlatformSocket.h"
#include <iostream>
#include <vector>

namespace net {

Server::Server(uint16_t port)
    : port_(port) {
    // WinSock 初始化（在 POSIX 下为 no-op）
    socket_startup();
}

Server::~Server() {
    stop();
    // 关闭残留连接与监听套接字
    for (auto& [fd, _] : conns_) close_socket(fd);
    conns_.clear();
    if (is_valid_socket(listen_fd_)) close_socket(listen_fd_);
    socket_cleanup();
}

bool Server::setup_socket() {
    listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (!is_valid_socket(listen_fd_)) {
        sys_perror("socket");
        return false;
    }

    // 端口复用
    int opt = 1;
    if (::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt)) < 0) {
        sys_perror("setsockopt(SO_REUSEADDR)");
        // 不致命，继续
    }

    // 绑定 0.0.0.0:port_
    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY); // 设置监听的ip为0.0.0.0
    addr.sin_port        = htons(port_);
    if (::bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        sys_perror("bind");
        return false;
    }

    if (::listen(listen_fd_, 128) < 0) {
        sys_perror("listen");
        return false;
    }

    if (!set_nonblocking(listen_fd_)) {
        sys_perror("set_nonblocking(listen_fd_)");
        // 这里不强制失败，但建议继续返回 true
    }

    return true;
}

bool Server::handle_read(Connection& c) {
    // 为了避免恶意请求撑爆内存，这里给 inbuf 设一个上限
    // 你可按需调整（例如 1MB）
    static constexpr size_t kMaxRequestSize = 1 * 1024 * 1024;

    char buf[8192];

    // 非阻塞尽量读空内核缓冲
    for (;;) {
        const ssize_t n = socket_recv(c.fd, buf, sizeof(buf));
        if (n > 0) { //后续还要继续读
            c.inbuf.append(buf, static_cast<size_t>(n)); //处理读事件，把数据放入连接的输入缓冲区

            // 请求过大：直接返回 413 并关闭（keep-alive=false）
            if (c.inbuf.size() > kMaxRequestSize) {
                http::HttpResponse resp;
                resp.status = 413;
                resp.reason = "Payload Too Large";
                resp.body   = "Payload Too Large";
                resp.set_content_type("text/plain; charset=utf-8");
                resp.set_keep_alive(false);

                c.outbuf     = resp.to_string(); // 组装上述结构体数据到输出缓冲区
                c.keep_alive = false;
                c.inbuf.clear();
                c.parser.reset();
                return true; // 让写阶段发送响应；发送完会根据 keep_alive 关闭
            }
            continue;
        }

        if (n == 0) {
            // 对端正常关闭
            return false;
        }

        // n < 0：错误或暂不可读
        const int err = last_sys_err();
        if (is_would_block(err)) {
            break; // 读完当前可得数据，跳出去解析
        }
        // 其他错误
        sys_perror("recv");
        return false;
    } // 将内核缓冲全读到inbuf中

    // 解析一次完整请求（按你现有的 Parser 语义：parse 成功即得到一个完整请求）
    if (c.parser.parse(c.inbuf)) {
        auto& req = c.parser.request();
        http::HttpResponse resp;

        c.keep_alive = req.keep_alive();
        resp.set_keep_alive(c.keep_alive);

        bool routed = false;
        if (router_) routed = router_->route(req, resp); //找到路由并执行处理函数

        if (!routed) { // 未命中路由，返回 404，也可能是服务器对象未设置路由
            resp.status = 404;
            resp.reason = "Not Found";
            resp.body   = "Not Found";
            resp.set_content_type("text/plain; charset=utf-8");
        }

        // 生成响应
        c.outbuf = resp.to_string();

        // 假设 parse 消费了整个请求（你的实现里也是这样做的）
        c.inbuf.clear();
        c.parser.reset();

        return true; // 已有可写数据
    }

    // 解析失败：返回 400
    if (c.parser.error()) {
        http::HttpResponse resp;
        resp.status = 400;
        resp.reason = "Bad Request";
        resp.body   = "Bad Request";
        resp.set_content_type("text/plain; charset=utf-8");
        resp.set_keep_alive(false);

        c.outbuf     = resp.to_string();
        c.keep_alive = false;
        c.inbuf.clear();
        c.parser.reset();
        return true; // 让写阶段发送 400
    }

    // 既未出错也未解析出完整请求 => 继续等更多数据
    return true;
}


bool Server::handle_write(Connection& c) {
    while (!c.outbuf.empty()) {
        const ssize_t n = socket_send(c.fd, c.outbuf.data(), c.outbuf.size()); //发送数据
        //由于当前设置了非阻塞，send可能会返回-1并设置errno为EAGAIN或EWOULDBLOCK，（Windows 下是 WSAEWOULDBLOCK）
        //表示当前无法发送数据，需要稍后重试
        //这种情况通常发生在发送缓冲区已满时，应用程序需要等待缓冲区有空间后再尝试发送
        if (n > 0) {
            c.outbuf.erase(0, static_cast<size_t>(n)); // n是发送出去的长度
            continue;
        }
        const int err = last_sys_err();
        if (is_would_block(err)) {
            // 发送缓冲区满，等下次可写
            // 认为是正常情况，返回true，只是没成功让系统发送出去
            return true;
        }
        // 其他错误
        return false;
    }
    return true;
}

bool Server::listen_and_serve() {
    if (running_.exchange(true)) {
        // 已在运行
        return false;
    }
    if (!setup_socket()) { // 向内核申请一个socket对象，并绑定端口开始监听，同时初始化fd
        running_ = false;
        return false;
    }

    std::cout << "Server listening on port " << port_ << std::endl;

    while (running_) {
        fd_set rfds, wfds; //创建一个新的读写集合
        FD_ZERO(&rfds); //清空读写集合（设置长度为0）
        FD_ZERO(&wfds);

        socket_t maxfd = listen_fd_;
        FD_SET(listen_fd_, &rfds); //设置监听套fd到读集合中 每次循环都需要监听这个来看是否有新连接

        for (auto& [fd, c] : conns_) { // 把当前服务器维护的所有连接的fd放入读写集合
            FD_SET(fd, &rfds);
            if (!c.outbuf.empty()) FD_SET(fd, &wfds); // 只关心输出缓冲区非空时的情况
            if (fd > maxfd) maxfd = fd;
        }

        // 1s 超时，便于可中断 stop() //不设置间隔就无法检查服务器的running_状态
        timeval tv{1, 0};
        const int nready = ::select(static_cast<int>(maxfd + 1), &rfds, &wfds, nullptr, &tv);
        if (nready < 0) {
            if (!running_) break; // 正在退出
            sys_perror("select");
            continue;
        }
        // nready >= 0：超时或有事件发生
        // 新连接
        if (FD_ISSET(listen_fd_, &rfds)) { //如果listen_fd_在读集合中没被select去掉，说明有新连接到来
            for (;;) { // 可能有多个连接同时到来
                sockaddr_in cli{};
                socklen_t   len = sizeof(cli);
                socket_t    cfd = socket_accept(listen_fd_, reinterpret_cast<sockaddr*>(&cli), &len);
                if (!is_valid_socket(cfd)) {
                    const int err = last_sys_err();
                    if (is_would_block(err)) break; // 没有更多可接收的连接
                    sys_perror("accept");
                    break;
                }
                set_nonblocking(cfd);
                conns_.emplace(cfd, Connection{cfd}); //聚合初始化结构体Connection对象+emplace穿参少调用一次拷贝
            }
        }

        // 已有连接读写
        std::vector<socket_t> to_close;
        to_close.reserve(32);

        for (auto& [fd, c] : conns_) {
            bool ok = true;

            if (FD_ISSET(fd, &rfds)) { //如果fd在读集合中没被select去掉，说明这个连接有数据可读
                ok = handle_read(c);
            }
            if (ok && FD_ISSET(fd, &wfds)) { //如果fd在写集合中没被select去掉，说明这个连接有数据可写
                ok = handle_write(c);
            }
            // 短连接：发送完或者标记为不保持连接 -> 关闭
            if (ok && c.outbuf.empty() && !c.keep_alive) {
                ok = false; // 标记为关闭
            }

            if (!ok) to_close.push_back(fd);
        }

        for (socket_t fd : to_close) {
            close_socket(fd);
            conns_.erase(fd);
        }
    } // while (running_) 结束

    // 退出清理
    for (auto& [fd, _] : conns_) close_socket(fd);
    conns_.clear();
    if (is_valid_socket(listen_fd_)) close_socket(listen_fd_);
    return true;
}

void Server::stop() noexcept {
    running_ = false;
}

// —— 工具方法 ——
// 注意：你的头文件把 set_nonblocking 设为 static，这里直接调用平台封装函数。
// close_socket 是成员函数（非 static），这里仅包一层便于统一管理。
//本质上就是是写了一个类的成员函数调用命名空间中的全局函数
void Server::close_socket(socket_t s) noexcept {
    if (is_valid_socket(s)) {
        net::close_socket(s);
    }
}

bool Server::set_nonblocking(socket_t s) noexcept {
    return net::set_socket_nonblocking(s);
}

} // namespace net
