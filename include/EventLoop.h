#ifndef EVENTLOOP_H
#define EVENTLOOP_H

#include "Connection.h"
#include "ConnectionManager.h"
#include "Poller.h"
#include "SocketListener.h"

#include <string>
#include <sys/eventfd.h>
#include <thread>
#include <functional>
#include <vector>
#include <mutex>

class EventLoop {
  public:
    EventLoop(int port);
    ~EventLoop();
    void start();
    void stop();
    void setMessageCallback(MessageCallback cb);
    void setDelimiter(const std::string &delim);

    void queueInLoop(std::function<void()> cb);
    void runInLoop(std::function<void()> cb);
  private:
    SocketListener listener_;
    Poller poller_;
    ConnectionManager connmgr_;
    MessageCallback user_handler;
    bool running_;
    std::string delimiter_;

    int wakeup_fd_;  // eventfd：跨线程唤醒 epoll_wait
    std::mutex mutex_;
    std::thread::id loop_thread_id_;
    std::vector<std::function<void()>> tasks_;
};

#endif