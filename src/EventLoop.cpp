#include "EventLoop.h"
#include "ConnectionManager.h"

#include <csignal>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/epoll.h>
#include <unistd.h>
// EventLoop类的实现
EventLoop::EventLoop(int port)
    : listener_(port), poller_(listener_.getFd()), connmgr_(&poller_),
      delimiter_("") {
    signal(SIGPIPE, SIG_IGN); // 防止Fin包未收到时send数据导致进程被砍
    std::cout << "the initialized TCP server on port" << port << std::endl;
}

EventLoop::~EventLoop() {}

void EventLoop::setMessageCallback(MessageCallback cb) { user_handler = cb; }

void EventLoop::setDelimiter(const std::string &delim) { delimiter_ = delim; }

void EventLoop::start() {
    int listen_fd = listener_.getFd();

    running_ = true;

    while (running_) {
        poller_.wait();

        for (int i = 0; i < poller_.ready_count_; i++) {
            int fd = poller_.events_[i].data.fd;

            if (fd == listen_fd) {
                int new_fd = listener_.accept();
                connmgr_.add(new_fd);
                Connection *conn = connmgr_.getconn(new_fd);
                if (conn == nullptr) {
                    continue;
                }

                conn->setMessageCallback(user_handler);
                conn->setCloseCallback(onConnectionClose, &connmgr_);
                conn->setDelimiter(delimiter_);
                conn->poller_ = &poller_;

            } else {
                uint32_t events = poller_.events_[i].events;
                Connection *conn = connmgr_.getconn(fd);

                if (conn == nullptr) {
                    continue;
                }

                if (events & EPOLLIN) {
                    conn->recv(fd);
                    conn = connmgr_.getconn(fd); // 防止recv失败后send读到野指针
                    if (conn == nullptr) {
                        continue;
                    }
                    conn->send(fd);
                }

                if (events & EPOLLOUT) {
                    conn = connmgr_.getconn(fd);
                    if (conn == nullptr) {
                        continue;
                    }
                    conn->send(fd);
                }
            }
        }
    }
}

void EventLoop::stop() { running_ = false; }