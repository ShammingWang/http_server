#ifndef _WIN32
#include "server/PlatformSocket.h"
#include <cstdio>

namespace net {

void socket_startup() {}
void socket_cleanup() {}

bool set_nonblocking(socket_t s) {
    int flags = ::fcntl(s, F_GETFL, 0);
    if (flags < 0) return false;
    return ::fcntl(s, F_SETFL, flags | O_NONBLOCK) == 0;
}

void close_socket(socket_t s) {
    ::close(s);
}

int last_sys_err() {
    return errno;
}

void sys_perror(const char* where) {
    ::perror(where);
}

ssize_t socket_recv(socket_t s, char* buf, size_t len) {
    return ::recv(s, buf, len, 0);
}
ssize_t socket_send(socket_t s, const char* buf, size_t len) {
    return ::send(s, buf, len, 0);
}
socket_t socket_accept(socket_t s, sockaddr* addr, socklen_t* len) {
    return ::accept(s, addr, len);
}
bool is_would_block(int err) {
    return err == EAGAIN || err == EWOULDBLOCK;
}

bool is_valid_socket(socket_t s) {
    return s >= 0;
}

bool set_socket_nonblocking(socket_t s) {
    int flags = fcntl(s, F_GETFL, 0);
    if (flags == -1) return false;
    return fcntl(s, F_SETFL, flags | O_NONBLOCK) == 0;
}

} // namespace net
#endif
