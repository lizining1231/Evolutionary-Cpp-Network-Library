#ifndef CONNECTION_H
#define CONNECTION_H

#include "Buffer.h"
#include <string>

class Poller;
class ConnectionManager;

using MessageCallback = std::string (*)(char const *msg, size_t len);
using CloseCallback = void (*)(int fd, ConnectionManager *connmgr);

class Connection {
  public:
    Connection(int client_fd);
    Connection() = delete;

    void send(int client_fd);
    void recv(int client_fd);

    void setMessageCallback(MessageCallback cb);
    void setCloseCallback(CloseCallback close_cb, ConnectionManager *connmgr);
    void setDelimeter(const std::string &delim);

    Poller *poller_;

  private:
  private:
    int client_fd_;
    bool send_blocked_;
    std::string delimeter_;
    std::string send_buffer_;

    Buffer recv_buffer_;
    ConnectionManager *connmgr_;
    CloseCallback close_cb_;
    MessageCallback handler_;
};

#endif