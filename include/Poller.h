#ifndef POLLER_H
#define POLLER_H

#include <sys/epoll.h>

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

#endif