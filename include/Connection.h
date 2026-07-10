#ifndef CONNECTION_H
#define CONNECTION_H

#include "Buffer.h"
#include "decoder/FrameDecoder.h"

#include <chrono>
#include <functional>
#include <string>

class ConnectionManager;

using MessageCallback = std::function<std::string(const char *msg, size_t len)>;
using CloseCallback = std::function<void(int fd)>;

class Connection {
public:
    Connection(int client_fd);
    ~Connection();
    Connection() = delete;

    void send(int client_fd);
    void recv(int client_fd);

    void setMessageCallback(MessageCallback cb);
    void setCloseCallback(CloseCallback cb);
    void setDecoder(std::unique_ptr<FrameDecoder> decoder);

    void setWriteCallbacks(std::function<void()> enable, std::function<void()> disable);

    int getFd() const { return client_fd_; }
    void touch();
    std::chrono::steady_clock::time_point getLastActiveTime() const;

private:
    int client_fd_;
    std::unique_ptr<FrameDecoder> decoder_;
    std::string send_buffer_;

    Buffer recv_buffer_;
    CloseCallback cb_;
    MessageCallback handler_;

    std::chrono::steady_clock::time_point last_active_time_;

    std::function<void()> enable_write_;
    std::function<void()> disable_write_;
};

#endif
