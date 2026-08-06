#ifndef POLLER_H
#define POLLER_H

#include "Config.h"

#include <sys/epoll.h>

#include <functional>
#include <vector>

class Poller {
public:
    using EventCallback = std::function<void(int fd, uint32_t events)>;

    explicit Poller(int listen_fd, Config& config);
    ~Poller() = default;

    void addFd(int client_fd);
    void removeFd(int client_fd);
    void wait();

    void enableOut(int client_fd);
    void disableOut(int client_fd);

    void iterateEvents(EventCallback cb);

private:
    int listen_fd_;
    int epoll_fd_;

    int epoll_timeout_ms_;
    int max_events_;
    std::vector<struct epoll_event> events_;
    int ready_count_;
};

#endif
