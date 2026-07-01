#ifndef CONNECTION_H
#define CONNECTION_H

#include "Buffer.h"
#include <string>
#include <functional>

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
    void setCloseCallback(CloseCallback close_cb, ConnectionManager *connmgr);
    void setDelimiter(const std::string &delim);

    void setWriteCallbacks(std::function<void()> enable, std::function<void()> disable);

  private:
    int client_fd_;
    bool send_blocked_;
    std::string delimiter_;
    std::string send_buffer_;

    Buffer recv_buffer_;
    ConnectionManager *connmgr_;
    CloseCallback close_cb_;
    MessageCallback handler_;

    std::function<void()> enable_write_;
    std::function<void()> disable_write_;
};

#endif