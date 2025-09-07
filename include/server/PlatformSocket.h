#pragma once

#include <cstdint>
#include <cstddef>

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  using socket_t = SOCKET;
  using socklen_t = int;
  using ssize_t = int; // 统一接口用
#else
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <fcntl.h>
  #include <unistd.h>
  #include <errno.h>
  using socket_t = int;
#endif

namespace net {

// 初始化 / 清理（Windows 需要，POSIX 空实现）
void socket_startup();
void socket_cleanup();

// 套接字通用操作
bool set_nonblocking(socket_t s);
void close_socket(socket_t s);

// 系统错误工具
int  last_sys_err();
void sys_perror(const char* where);

ssize_t socket_recv(socket_t s, char* buf, size_t len);
ssize_t socket_send(socket_t s, const char* buf, size_t len);
socket_t socket_accept(socket_t s, sockaddr* addr, socklen_t* len);
bool is_would_block(int err);

bool is_valid_socket(socket_t s);


// 设置非阻塞（在 Socket_posix.cpp / Socket_win.cpp 里实现）
bool set_socket_nonblocking(socket_t s);

} // namespace net
