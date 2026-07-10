#include "Connection.h"

#include "EventLoop.h"
#include "logger.h"

#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <iostream>

Connection::Connection(int client_fd)
    : client_fd_(client_fd), last_active_time_(std::chrono::steady_clock::now()) {}

Connection::~Connection() {
    ::close(client_fd_);
}

void Connection::setCloseCallback(CloseCallback cb) {
    cb_ = std::move(cb);
}

void Connection::setMessageCallback(MessageCallback cb) {
    handler_ = std::move(cb);
}

void Connection::setDecoder(std::unique_ptr<FrameDecoder> decoder) {
    decoder_ = std::move(decoder);
}

void Connection::recv(int client_fd) {
    char temp_buffer[65536];
    ssize_t bytes_read;

    bytes_read = ::recv(client_fd, temp_buffer, sizeof(temp_buffer), 0);

    if (bytes_read <= 0) {
        if (bytes_read == 0) {
            LOG_DEBUG("Client disconnected (fd:%d)", client_fd);
            cb_(client_fd);
            return;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return;
            } else {
                LOG_ERROR("Receive error (fd:%d): %s", client_fd, strerror(errno));
                cb_(client_fd);
                return;
            }
        }
    }

    touch();

    recv_buffer_.appendData(temp_buffer, bytes_read);

    // 解析 recv_buffer_，调 handler 生成响应，放入 send_buffer_
    while (auto request = decoder_->decode(recv_buffer_)) {
        if (!handler_) {
            LOG_WARN("handler is null, skipping request");
            continue;
        }
        std::string response = handler_(request->c_str(), request->size());
        send_buffer_.append(response);
    }
}

void Connection::send(int client_fd) {
    while (!send_buffer_.empty()) {
        ssize_t n = ::send(client_fd, send_buffer_.c_str(), send_buffer_.length(), 0);

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
                cb_(client_fd);
                return;
            }
        }
        touch();
    }

    if (disable_write_) {
        disable_write_();
    }
}

void Connection::setWriteCallbacks(std::function<void()> enable, std::function<void()> disable) {
    enable_write_ = std::move(enable);
    disable_write_ = std::move(disable);
}

void Connection::touch() {
    last_active_time_ = std::chrono::steady_clock::now();
}

std::chrono::steady_clock::time_point Connection::getLastActiveTime() const {
    return last_active_time_;
}
