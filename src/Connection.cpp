#include "Connection.h"
#include "EventLoop.h"
#include "Poller.h"

#include <cerrno>
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>

Connection::Connection(int client_fd) : client_fd_(client_fd) {}

// 当map找不到key值时会利用此默认构造函数来创建
Connection::Connection() { poller_ = nullptr; }

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
    if (this == nullptr) {
        return;
    }

    char temp_buffer[65536];
    ssize_t bytes_read;

    bytes_read = ::recv(client_fd, temp_buffer, sizeof(temp_buffer), 0);

    if (bytes_read <= 0) {
        if (bytes_read == 0) {
            std::cout << "Client disconnected" << std::endl;
            close_cb_(client_fd, connmgr_);
            return;
        } else {
            // bytes_read<0
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return;
            } else {
                std::cerr << "Receive error" << std::endl;
                close_cb_(client_fd, connmgr_);
                return;
            }
        }
    }

    // 为进行职责分离, 将临时缓冲区的数据追加到永久缓冲区, 永久缓冲区负责处理
    if (this) {
        this->recv_buffer_.appendData(temp_buffer, bytes_read);
    } // 不用写else，因为如果没有conn什么都不用做

    // 解析 recv_buffer_，调 handler 生成响应，放入 send_buffer_
    std::string request;
    while (recv_buffer_.takeData(request, delimeter_)) {
        if (!handler_) {
            std::cout << "handler 为空!\n";
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
                std::cerr << " send error" << std::endl;
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