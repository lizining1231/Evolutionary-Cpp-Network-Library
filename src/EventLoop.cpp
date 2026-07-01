#include "EventLoop.h"
#include "ConnectionManager.h"

#include <csignal>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/epoll.h>
#include <unistd.h>
#include <sys/eventfd.h>
#include <unistd.h>

// EventLoop类的实现
EventLoop::EventLoop(int port)
    : listener_(port), poller_(listener_.getFd()), connmgr_(&poller_),
      delimiter_("") {
    signal(SIGPIPE, SIG_IGN); // 防止Fin包未收到时send数据导致进程被砍

    wakeup_fd_ = eventfd(0, EFD_NONBLOCK);
    if(wakeup_fd_ == -1) {
        throw std::runtime_error("eventfd creation failed");
    }
    poller_.addFd(wakeup_fd_);

    std::cout << "the initialized TCP server on port" << port << std::endl;
}

EventLoop::~EventLoop() {
    if (wakeup_fd_ != -1) {
        ::close(wakeup_fd_);
    }
}

void EventLoop::setMessageCallback(MessageCallback cb) { user_handler = cb; }

void EventLoop::setDelimiter(const std::string &delim) { delimiter_ = delim; }

void EventLoop::start() {
    int listen_fd = listener_.getFd();

    running_ = true;

    while (running_) {
        poller_.wait();

        poller_.iterateEvents([&](int fd, uint32_t events) {

            if (fd == listen_fd) {
                int new_fd = listener_.accept();
                connmgr_.add(new_fd);
                Connection *conn = connmgr_.getconn(new_fd);
                if (conn == nullptr) {
                    return;
                }

                conn->setMessageCallback(user_handler);
                conn->setCloseCallback(onConnectionClose, &connmgr_);
                conn->setDelimiter(delimiter_);
                conn->setWriteCallbacks(
                    [this, new_fd]() { poller_.enableOut(new_fd); },
                    [this, new_fd]() { poller_.disableOut(new_fd); }
                );

            } else if (fd == wakeup_fd_) {
                uint64_t one;
                ::read(wakeup_fd_, &one, sizeof(one));

            } else {
                Connection *conn = connmgr_.getconn(fd);

                if (conn == nullptr) {
                    return;
                }

                if (events & EPOLLIN) {
                    conn->recv(fd);
                    conn = connmgr_.getconn(fd); // 防止recv失败后send读到野指针
                    if (conn == nullptr) {
                        return;
                    }
                    conn->send(fd);
                }

                if (events & EPOLLOUT) {
                    conn = connmgr_.getconn(fd);
                    if (conn == nullptr) {
                        return;
                    }
                    conn->send(fd);
                }
            }

        });
    }
}

void EventLoop::stop() {
    running_ = false;

    uint64_t one = 1;
    ::write(wakeup_fd_, &one, sizeof(one));
}