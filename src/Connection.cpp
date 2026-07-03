#include "Connection.h"
#include "EventLoop.h"
#include "logger.h"

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

void Connection::setDelimiter(const std::string &delim) { delimiter_ = delim; }

void Connection::recv(int client_fd) {
    char temp_buffer[65536];
    ssize_t bytes_read;

    bytes_read = ::recv(client_fd, temp_buffer, sizeof(temp_buffer), 0);

    if (bytes_read <= 0) {
        if (bytes_read == 0) {
            LOG_DEBUG("Client disconnected (fd:%d)", client_fd);
            close_cb_(client_fd, connmgr_);
            return;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return;
            } else {
                LOG_ERROR("Receive error (fd:%d): %s", client_fd, strerror(errno));
                close_cb_(client_fd, connmgr_);
                return;
            }
        }
    }

    recv_buffer_.appendData(temp_buffer, bytes_read);

    // 解析 recv_buffer_，调 handler 生成响应，放入 send_buffer_
    std::string request;
    while (recv_buffer_.takeData(request, delimiter_)) {
        if (!handler_) {
            LOG_WARN("handler is null, skipping request");
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
                if (enable_write_) {
                    enable_write_();
                }
                return;

            } else {
                LOG_ERROR("Send error (fd:%d): %s", client_fd, strerror(errno));
                close_cb_(client_fd, connmgr_);
                return;
            }
        }
    }

    if (disable_write_) {
        disable_write_();
    }
}

void Connection::setWriteCallbacks(std::function<void()> enable,
                                   std::function<void()> disable) {
    enable_write_ = std::move(enable);
    disable_write_ = std::move(disable);
}