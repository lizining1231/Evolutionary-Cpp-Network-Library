#include "TimerQueue.h"
#include "EventLoop.h"
#include "logger.h"

#include <sys/timerfd.h>
#include <unistd.h>
#include <string.h>


TimerQueue::TimerQueue(EventLoop* loop)
    : loop_(loop),
      timerfd_(createTimerfd()),
      seq_counter_(1) {}

TimerQueue::~TimerQueue() {
    if (timerfd_ != -1) {
        ::close(timerfd_);
    }
}

TimerQueue::TimerId TimerQueue::addTimer(
    TimerCallback cb,
    std::chrono::steady_clock::time_point when,
    std::chrono::milliseconds interval) {
        int seq = seq_counter_;
        seq_counter_++;

        Timer timer{{seq}, when, std::move(cb), interval};
        
        bool earliest = heap_.empty() || timer.when < heap_.top().when;
        heap_.push(std::move(timer));
        
        if (earliest) {
            resetTimerfd(when);
        }

        return TimerId{seq};
    }

void TimerQueue::cancel(TimerId timerId) {
    (void)timerId;
}

int TimerQueue::getTimerFd() const {
    return timerfd_;
}

void TimerQueue::handleRead() {
    uint64_t exp;
    ssize_t n = ::read(timerfd_, &exp, sizeof(exp));
    if (n != sizeof(exp)){
        LOG_ERROR("timerfd read error");
        return;
    }

    auto now = std::chrono::steady_clock::now();

    while (!heap_.empty() && heap_.top().when <= now) {
        Timer timer = heap_.top();
        heap_.pop();
        timer.cb();

        if (timer.interval.count() > 0) {
            timer.when = now + timer.interval;
            heap_.push(std::move(timer));
        }
    }
    
    if(!heap_.empty()) {
        resetTimerfd(heap_.top().when);
    }
}

int TimerQueue::createTimerfd() {
    int timerfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if (timerfd == -1) {
        LOG_ERROR("timerfd_create failed: %s", strerror(errno));
    }
    return timerfd;
}

void TimerQueue::resetTimerfd(std::chrono::steady_clock::time_point when) {
    auto now = std::chrono::steady_clock::now();

    auto duration = when > now ? (when - now) : std::chrono::nanoseconds(0);

    auto secs = std::chrono::duration_cast<std::chrono::seconds>(duration);
    auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(duration - secs);

    struct itimerspec spec;
    memset(&spec, 0, sizeof(spec));
    spec.it_value.tv_sec = secs.count();
    spec.it_value.tv_nsec = nanos.count();

    if (timerfd_settime(timerfd_, 0, &spec, nullptr) == -1) {
        LOG_ERROR("timerfd_settime failed");
    }
}
