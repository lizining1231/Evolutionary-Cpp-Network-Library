#ifndef EVENTLOOP_H
#define EVENTLOOP_H

#include "Connection.h"
#include "ConnectionManager.h"
#include "Poller.h"
#include "SocketListener.h"
#include "TimerQueue.h"
#include "decoder/FrameDecoder.h"

#include <sys/eventfd.h>

#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class EventLoop {
public:
    EventLoop(int port);
    ~EventLoop();
    void start();
    void stop();

    void setMessageCallback(MessageCallback cb);
    void setDecoder(std::unique_ptr<FrameDecoder> decoder);

    void queueInLoop(std::function<void()> cb);
    void runInLoop(std::function<void()> cb);

    TimerQueue::TimerId runAt(std::chrono::steady_clock::time_point when,
                              TimerQueue::TimerCallback cb);
    TimerQueue::TimerId runAfter(std::chrono::milliseconds delay, TimerQueue::TimerCallback cb);
    TimerQueue::TimerId runEvery(std::chrono::milliseconds interval, TimerQueue::TimerCallback cb);

private:
    void startIdleTimeout();
    SocketListener listener_;
    Poller poller_;
    ConnectionManager connmgr_;
    std::unique_ptr<FrameDecoder> decoder_;
    TimerQueue timer_queue_;

    MessageCallback user_handler_;
    bool running_;
    int wakeup_fd_;  // eventfd：跨线程唤醒 epoll_wait
    std::mutex mutex_;
    std::thread::id loop_thread_id_;
    std::vector<std::function<void()>> tasks_;
};

#endif