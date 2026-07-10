#include "ConnectionManager.h"

ConnectionManager::ConnectionManager(Poller *poller) : poller_(poller) {}

Connection *ConnectionManager::getconn(int client_fd) {
    if (client_fd < 0 || client_fd >= (int)connections_.size()) {
        return nullptr;
    }
    return connections_[client_fd].get();
}

void ConnectionManager::add(int client_fd) {
    if (client_fd < 0) {
        return;
    }

    if (client_fd >= (int)connections_.size()) {
        connections_.resize(client_fd + 1);
    }
    connections_[client_fd] = std::make_unique<Connection>(client_fd);

    poller_->addFd(client_fd);
}

void ConnectionManager::remove(int client_fd) {
    poller_->removeFd(client_fd);  // 先从 epoll 移除

    if (client_fd >= 0 && client_fd < (int)connections_.size()) {
        connections_[client_fd].reset();
    }
}

void ConnectionManager::forEachConn(std::function<void(Connection *)> cb) {
    for (auto &conn_ptr : connections_) {
        if (conn_ptr) {
            cb(conn_ptr.get());
        }
    }
}
