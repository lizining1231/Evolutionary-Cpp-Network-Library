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
    : listener(port), poller(listener.getFd()), connmgr(&poller),
      delimeter_("") {
    signal(SIGPIPE, SIG_IGN); // 防止Fin包未收到时send数据导致进程被砍
    std::cout << "the initialized TCP server on port" << port << std::endl;
}

EventLoop::~EventLoop() {}

void EventLoop::setMessageCallback(MessageCallback cb) { user_handler = cb; }

void EventLoop::setDelimeter(const std::string &delim) { delimeter_ = delim; }

void EventLoop::start() {
    int listen_fd = listener.getFd();

    running_ = true;

    while (running_) {
        poller.wait();

        for (int i = 0; i < poller.ready_count_; i++) {
            int fd = poller.events_[i].data.fd;

            if (fd == listen_fd) {
                int new_fd = listener.accept();
                connmgr.add(new_fd);
                Connection *conn = connmgr.getconn(new_fd);

                conn->setMessageCallback(user_handler);
                conn->setCloseCallback(onConnectionClose, &connmgr);
                conn->setDelimeter(delimeter_);
                conn->poller_ = &poller;

            } else {
                uint32_t events = poller.events_[i].events;
                Connection *conn = connmgr.getconn(fd);

                if (conn == nullptr) {
                    continue;
                }

                if (events & EPOLLIN) {
                    conn->recv(fd);
                    conn = connmgr.getconn(fd); // 防止recv失败后send读到野指针
                    if (conn == nullptr) {
                        continue;
                    }
                    conn->send(fd);
                }

                if (events & EPOLLOUT) {
                    conn = connmgr.getconn(fd);
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