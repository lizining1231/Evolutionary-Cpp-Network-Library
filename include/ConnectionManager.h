#ifndef CONNECTIONMANAGER_H
#define CONNECTIONMANAGER_H

#include "Connection.h"
#include "Poller.h"
#include "Config.h"

#include <functional>
#include <memory>
#include <vector>

class ConnectionManager {
public:
    explicit ConnectionManager(Poller *poller, Config& cfg);
    Connection *getconn(int client_fd);
    void add(int client_fd);
    void remove(int client_fd);
    void forEachConn(std::function<void(Connection *)> cb);

private:
    std::vector<std::unique_ptr<Connection>> connections_;  // 一个fd对应一个连接
    int recv_buffer_size_;
    Poller *poller_;
};

#endif
