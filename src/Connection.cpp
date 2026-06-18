#include "Connection.h"
#include "EventLoop.h"
#include "Poller.h"

#include <cerrno>
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>

Connection::Connection(int client_fd) : client_fd_(client_fd) {}

Connection::~Connection() {
    ::close(client_fd_);
}

void Connection::setCloseCallback(CloseCallback close_cb,
                                  ConnectionManager *connmgr) {
    close_cb_ = close_cb;
    connmgr_ = connmgr;
}

void Connection::setMessageCallback(MessageCallback handleMessage) {
    handler_ = handleMessage;
}

void Connection::setDelimeter(const std::string &delim) { delimeter_ = delim; }

void Connection::recv(int client_fd) {
    char temp_buffer[65536];
    ssize_t bytes_read;

    bytes_read = ::recv(client_fd, temp_buffer, sizeof(temp_buffer), 0);

    if (bytes_read <= 0) {
        if (bytes_read == 0) {
            std::cout << "[DEBUG] Client disconnected (fd:" << client_fd << ")" << std::endl;
            close_cb_(client_fd, connmgr_);
            return;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return;
            } else {
                std::cerr << "[ERROR] Receive error (fd:" << client_fd << "): " << strerror(errno) << std::endl;
                close_cb_(client_fd, connmgr_);
                return;
            }
        }
    }

    recv_buffer_.appendData(temp_buffer, bytes_read);

    // 解析 recv_buffer_，调 handler 生成响应，放入 send_buffer_
    std::string request;
    while (recv_buffer_.takeData(request, delimeter_)) {
        if (!handler_) {
            std::cerr << "[WARN] handler is null, skipping request" << std::endl;
            continue;
        }
        std::string response = handler_(request.c_str(), request.size());
        send_buffer_.append(response);
    }
}

void Connection::send(int client_fd) {
    while (!send_buffer_.empty()) {
        ssize_t n =
            ::send(client_fd, send_buffer_.c_str(), send_buffer_.length(), 0);

        if (n > 0) {
            send_buffer_.erase(0, n);
        } else {
            // n < 0
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                poller_->enableOut(client_fd);
                send_blocked_ = true;

                return;
            } else {
                std::cerr << "[ERROR] Send error (fd:" << client_fd << "): " << strerror(errno) << std::endl;
                close_cb_(client_fd, connmgr_);
                return;
            }
        }
    }

    if (send_blocked_) {
        poller_->disableOut(client_fd);
        send_blocked_ = false;
    }
}