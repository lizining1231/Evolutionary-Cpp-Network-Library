#ifndef EVENTLOOP_H
#define EVENTLOOP_H

#include "Connection.h"
#include "ConnectionManager.h"
#include "Poller.h"
#include "SocketListener.h"

#include <string>

class EventLoop {
  public:
    EventLoop(int port);
    ~EventLoop();
    void start();
    void stop();
    void setMessageCallback(MessageCallback cb);
    void setDelimeter(const std::string &delim);

  private:
    SocketListener listener_;
    Poller poller_;
    ConnectionManager connmgr_;
    MessageCallback user_handler;
    bool running_;
    std::string delimeter_;
};

#endif