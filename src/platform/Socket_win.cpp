#ifdef _WIN32
#include "server/PlatformSocket.h"
#include <ws2def.h>
#include <iostream>

namespace net {

void socket_startup() {
    WSADATA w;
    if (WSAStartup(MAKEWORD(2,2), &w) != 0) {
        std::cerr << "WSAStartup failed\n";
    }
}
void socket_cleanup() { WSACleanup(); }

bool set_nonblocking(socket_t s) {
    u_long mode = 1;
    return ::ioctlsocket(s, FIONBIO, &mode) == 0;
}

void close_socket(socket_t s) {
    ::closesocket(s);
}

int last_sys_err() {
    return WSAGetLastError();
}

void sys_perror(const char* where) {
    std::cerr << where << " error: " << WSAGetLastError() << "\n";
}

ssize_t socket_recv(socket_t s, char* buf, size_t len) {
    int r = ::recv(s, buf, static_cast<int>(len), 0);
    return r;
}
ssize_t socket_send(socket_t s, const char* buf, size_t len) {
    int r = ::send(s, buf, static_cast<int>(len), 0);
    return r;
}
socket_t socket_accept(socket_t s, sockaddr* addr, socklen_t* len) {
    return ::accept(s, addr, len);
}
bool is_would_block(int err) {
    return err == WSAEWOULDBLOCK;
}

bool is_valid_socket(socket_t s) {
    return s != INVALID_SOCKET;
}

bool set_socket_nonblocking(socket_t s) {
    u_long mode = 1; //开启非阻塞模式
    return ioctlsocket(s, FIONBIO, &mode) == 0;
}

} // namespace net
#endif
