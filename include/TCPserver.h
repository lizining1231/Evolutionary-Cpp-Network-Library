#ifndef TCPSERVER_H
#define TCPSERVER_H

#include <sys/epoll.h>
#include <sys/types.h>
#include <unistd.h>

#include <map>
#include <string>
#include <vector>

class Connection;        // 前置声明
class ConnectionManager; // 因为有互相依赖

using MessageCallback = std::string (*)(char const *msg, size_t len);
using CloseCallback = void (*)(int fd, ConnectionManager *connmgr);

class SocketListener {
  public:
    explicit SocketListener(int port);
    ~SocketListener();
    int getFd() const;
    int accept();
    void close();

  private:
    int listen_fd_;
    void initSocket(int port);

    SocketListener(const SocketListener &) = delete; // 禁止拷贝
    SocketListener &operator=(const SocketListener &) = delete;
};

class Poller {
  public:
    explicit Poller(int listen_fd);
    ~Poller() = default;

    void addFd(int client_fd);
    void removeFd(int client_fd);
    void wait();

    void enableOut(int client_fd);
    void disableOut(int client_fd);

    int getReadyFd(int index);
    int getEpoll_fd();

    static const int MAX_EVENTS = 4096;
    struct epoll_event events_[MAX_EVENTS];
    int ready_count_;

  private:
    int port;
    int listen_fd_;
    int epoll_fd_;
};

class Buffer {
  public:
    void appendData(const char *data, ssize_t length);
    bool takeData(std::string &request, const std::string &delimeter);

  private:
    std::string recv_buffer;
};

class ConnectionManager {
  public:
    explicit ConnectionManager(Poller *poller_);
    Connection *getconn(int client_fd);
    void add(int client_fd);
    void remove(int client_fd);

  private:
    std::map<int, Connection> connections_; // 一个fd对应一个连接
    Poller *poller_;
};

class Connection {
  public:
    void send(int client_fd);
    void recv(int client_fd);

    Connection(int client_fd);
    Connection();

    void setMessageCallback(MessageCallback cb);
    void setCloseCallback(CloseCallback close_cb, ConnectionManager *connmgr);

    void setDelimeter(const std::string &delim);
    Poller *poller_;

  private:
    std::string delimeter_;
    std::string send_buffer;
    int client_fd;
    Buffer recv_buffer;
    ConnectionManager *connmgr_;
    CloseCallback close_cb_;
    MessageCallback handler;
};

class EventLoop {
  public:
    EventLoop(int port);
    ~EventLoop();
    void start();
    void stop();
    void setMessageCallback(MessageCallback cb);
    void setDelimeter(const std::string &delim);

  private:
    SocketListener listener;
    Poller poller;
    ConnectionManager connmgr;
    MessageCallback user_handler;
    bool running_;
    std::string delimeter_;
};

#endif