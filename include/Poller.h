#ifndef POLLER_H
#define POLLER_H

#include <sys/epoll.h>
#include <functional>

class Poller {
  public:
    using EventCallback = std::function<void(int fd, uint32_t events)>;

    explicit Poller(int listen_fd);
    ~Poller() = default;

    void addFd(int client_fd);
    void removeFd(int client_fd);
    void wait();

    void enableOut(int client_fd);
    void disableOut(int client_fd);

    void iterateEvents(EventCallback cb);

  private:
    int port_;
    int listen_fd_;
    int epoll_fd_;

    static const int MAX_EVENTS = 4096;
    struct epoll_event events_[MAX_EVENTS];
    int ready_count_;
};

#endif