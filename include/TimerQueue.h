#ifndef TIMERQUEUE_H
#define TIMERQUEUE_H

#include <chrono>
#include <functional>
#include <queue>
#include <vector>

class EventLoop;  // 此处只取指针，不用EventLoop的接口，向前声明即可

class TimerQueue {
public:
    using TimerCallback = std::function<void()>;

    struct TimerId {
        int64_t sequence;
    };

    explicit TimerQueue(EventLoop* loop);
    ~TimerQueue();

    TimerId addTimer(TimerCallback cb, std::chrono::steady_clock::time_point when,
                     std::chrono::milliseconds interval);

    void cancel(TimerId timerId);

    int getTimerFd() const;
    void handleRead();

private:
    struct Timer {
        TimerId id;
        std::chrono::steady_clock::time_point when;
        TimerCallback cb;
        std::chrono::milliseconds interval;

        bool operator>(const Timer& rhs) const { return when > rhs.when; }
    };

    using TimerHeap = std::priority_queue<Timer, std::vector<Timer>, std::greater<Timer> >;

    int createTimerfd();
    void resetTimerfd(std::chrono::steady_clock::time_point when);

    EventLoop* loop_;
    int timerfd_;
    TimerHeap heap_;
    int64_t seq_counter_;
};

#endif
