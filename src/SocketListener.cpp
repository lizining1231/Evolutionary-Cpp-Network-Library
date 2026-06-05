#include "SocketListener.h"

#include <cstring>

#include <cerrno>
#include <iostream>
#include <stdexcept>

#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

#define BACKLOG 128

SocketListener::SocketListener(int port) : listen_fd_(-1) { initSocket(port); }

SocketListener::~SocketListener() { close(); }

int SocketListener::getFd() const { return listen_fd_; }

void SocketListener::initSocket(int port) {
    // 设置套接字
    listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);

    if (listen_fd_ < 0) {
        throw std::runtime_error("Socket creation failed");
    }

    int old_flags = fcntl(listen_fd_, F_GETFL, 0);
    fcntl(listen_fd_, F_SETFL, old_flags | O_NONBLOCK);

    // 设置套接字选项
    int opt = 1;
    if (setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) <
        0) {
        throw std::runtime_error("Setsockopt SO_REUSEADDR failed");
    }
    if (setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) <
        0) {
        throw std::runtime_error("Setsockopt SO_REUSEPORT failed");
    }

    // 绑定地址、端口
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(listen_fd_, (sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        throw std::runtime_error("Bind failed");
    }

    // 监听
    if (listen(listen_fd_, BACKLOG) < 0) {
        throw std::runtime_error("Listen failed");
    }
    std::cout << "Server listening on port" << port << std::endl;
}

int SocketListener::accept() {
    sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);

    int client_fd = ::accept(listen_fd_, (sockaddr *)&client_addr, &client_len);

    if (client_fd < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return -1;
        }

        throw std::runtime_error(std::string("Accept failed:") +
                                 strerror(errno));
    }

    int old_flags = fcntl(client_fd, F_GETFL, 0);
    fcntl(client_fd, F_SETFL, old_flags | O_NONBLOCK);

    // 将二进制的IP地址转换成字符串
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);

    // 将网络字节序转化为主机字节序
    std::cout << client_ip << ":" << ntohs(client_addr.sin_port)
              << "(fd:" << client_fd << ")" << std::endl;

    return client_fd;
}

void SocketListener::close() {
    if (listen_fd_ >= 0) {
        shutdown(listen_fd_, SHUT_WR); // 发送FIN
        ::close(listen_fd_);

        listen_fd_ = -1;
    }
}
