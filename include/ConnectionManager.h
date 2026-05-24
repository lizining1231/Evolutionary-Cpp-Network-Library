#ifndef CONNECTIONMANAGER_H
#define CONNECTIONMANAGER_H

#include "Connection.h"
#include "Poller.h"

#include <map>

class Connection; // 前置声明

class ConnectionManager {
  public:
    explicit ConnectionManager(Poller *poller_);
    Connection *getconn(int client_fd);
    void add(int client_fd);
    void remove(int client_fd);

  private:
    std::map<int, Connection> connections_; // 一个fd对应一个连接
    Poller *poller_;
};

void onConnectionClose(int fd, ConnectionManager *connmgr);

#endif
