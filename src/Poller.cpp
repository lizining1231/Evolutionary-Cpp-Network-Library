#include "Poller.h"

#include <cerrno>
#include <cstring>
#include <stdexcept>

#include <sys/epoll.h>
#include <unistd.h>

// SocketListener类没有无参构造函数，这里添加显示构造
Poller::Poller(int listen_fd) : listen_fd_(listen_fd) {
    epoll_fd_ = epoll_create1(0);

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = listen_fd;
    epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, listen_fd, &ev);
}

void Poller::addFd(int client_fd) {
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = client_fd;
    epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, client_fd, &ev);
}

void Poller::wait() {
    ready_count_ = epoll_wait(epoll_fd_, events_, MAX_EVENTS, 100);

    if (ready_count_ < 0 && errno != EINTR) {
        throw std::runtime_error(std::string("epoll_wait:") + strerror(errno));
    }
}

void Poller::removeFd(int client_fd) {
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = client_fd;
    epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, client_fd, &ev);
}

void Poller::enableOut(int client_fd) {
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLOUT;
    ev.data.fd = client_fd;
    epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, client_fd, &ev);
}

void Poller::disableOut(int client_fd) {
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = client_fd;
    epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, client_fd, &ev);
}

void Poller::iterateEvents(EventCallback cb) {
    for (int i = 0; i < ready_count_; i++) {
        cb(events_[i].data.fd, events_[i].events);
    }
}