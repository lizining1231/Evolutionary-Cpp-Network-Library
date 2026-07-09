#include "EventLoop.h"
#include "ConnectionManager.h"
#include "logger.h"
#include "TimerQueue.h"

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
      timer_queue_(this) {

    signal(SIGPIPE, SIG_IGN); // 防止Fin包未收到时send数据导致进程被砍

    wakeup_fd_ = eventfd(0, EFD_NONBLOCK);
    if(wakeup_fd_ == -1) {
        throw std::runtime_error("eventfd creation failed");
    }
    poller_.addFd(wakeup_fd_);
    poller_.addFd(timer_queue_.getTimerFd());

    LOG_INFO("TCP server initialized on port %d", port);
}

EventLoop::~EventLoop() {
    if (wakeup_fd_ != -1) {
        ::close(wakeup_fd_);
    }
}

void EventLoop::setMessageCallback(MessageCallback cb) { user_handler_ = cb; }

void EventLoop::setDecoder(std::unique_ptr<FrameDecoder> decoder) {
    decoder_ = std::move(decoder);
}

void EventLoop::start() {
    int listen_fd = listener_.getFd();

    running_ = true;
    loop_thread_id_ = std::this_thread::get_id();

    startIdleTimeout();

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

                conn->setMessageCallback(user_handler_);
                conn->setCloseCallback(onConnectionClose, &connmgr_);
                conn->setDecoder(decoder_->clone());
                conn->setWriteCallbacks(
                    [this, new_fd]() { poller_.enableOut(new_fd); },
                    [this, new_fd]() { poller_.disableOut(new_fd); }
                );

            } else if (fd == wakeup_fd_) {
                uint64_t one;
                ::read(wakeup_fd_, &one, sizeof(one));

            } else if (fd == timer_queue_.getTimerFd()) {
                timer_queue_.handleRead();

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

        std::vector<std::function<void()>> local_tasks;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            local_tasks.swap(tasks_);    // 不直接执行tasks_, 避免回调中调用queueInloop导致死锁
        }
        for (auto &task : local_tasks) {
            task();
        }

    }
}

void EventLoop::stop() {
    running_ = false;

    uint64_t one = 1;
    ::write(wakeup_fd_, &one, sizeof(one));
}

void EventLoop::runInLoop(std::function<void()> cb) {
    if (std::this_thread::get_id() == loop_thread_id_) {
        cb();
    } else {
        queueInLoop(std::move(cb));
    }
}

void EventLoop::queueInLoop(std::function<void()> cb) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        tasks_.push_back(std::move(cb));
    }
    uint64_t one = 1;
    ::write(wakeup_fd_, &one, sizeof(one));
}

void EventLoop::startIdleTimeout() {
    runEvery(std::chrono::seconds(1), [this]() {
        auto now = std::chrono::steady_clock::now();
        auto timeout = std::chrono::seconds(60);

        connmgr_.forEachConn([&](Connection *conn) {
            if (now - conn->getLastActiveTime() > timeout) {
                LOG_INFO("[IdleTimeout] fd:%d idle timeout, close", conn->getFd());
                onConnectionClose(conn->getFd(), &connmgr_);
            }
        });
    });
}

TimerQueue::TimerId EventLoop::runAt(
    std::chrono::steady_clock::time_point when,
    TimerQueue::TimerCallback cb) {
    
    return timer_queue_.addTimer(std::move(cb), when,
    std::chrono::milliseconds(0));
}

TimerQueue::TimerId EventLoop::runAfter(
    std::chrono::milliseconds delay,
    TimerQueue::TimerCallback cb) {

    // delay 是时间段，要与时间点加和得到绝对时间用于执行
    auto when = std::chrono::steady_clock::now() + delay;
    return timer_queue_.addTimer(std::move(cb), when,
    std::chrono::milliseconds(0));
}

TimerQueue::TimerId EventLoop::runEvery(
    std::chrono::milliseconds interval,
    TimerQueue::TimerCallback cb) {

    auto when = std::chrono::steady_clock::now() + interval;
    return timer_queue_.addTimer(std::move(cb), when, interval);
}