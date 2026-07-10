#ifndef CONNECTION_H
#define CONNECTION_H

#include "Buffer.h"
#include "decoder/FrameDecoder.h"

#include <chrono>
#include <functional>
#include <string>

class ConnectionManager;

using MessageCallback = std::string (*)(char const *msg, size_t len);
using CloseCallback = void (*)(int fd, ConnectionManager *connmgr);

class Connection {
public:
    Connection(int client_fd);
    ~Connection();
    Connection() = delete;

    void send(int client_fd);
    void recv(int client_fd);

    void setMessageCallback(MessageCallback cb);
    void setCloseCallback(CloseCallback cb, ConnectionManager *connmgr);
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
    ConnectionManager *connmgr_;
    CloseCallback cb_;
    MessageCallback handler_;

    std::chrono::steady_clock::time_point last_active_time_;

    std::function<void()> enable_write_;
    std::function<void()> disable_write_;
};

#endif