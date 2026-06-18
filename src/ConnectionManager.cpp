#include "ConnectionManager.h"

ConnectionManager::ConnectionManager(Poller *poller) : poller_(poller) {}

Connection *ConnectionManager::getconn(int client_fd) {
    auto it = connections_.find(client_fd);

    if (it != connections_.end()) {
        return &(it->second);
    } else {
        return nullptr;
    }
}

void ConnectionManager::add(int client_fd) {
    connections_.emplace(client_fd, client_fd);
    poller_->addFd(client_fd);
}

void ConnectionManager::remove(int client_fd) {
    poller_->removeFd(client_fd);       // 先从 epoll 移除
    connections_.erase(client_fd);      // std::map::erase，触发析构 → ::close(fd)
}

void onConnectionClose(int fd, ConnectionManager *connmgr) {
    if (!connmgr) {
        return;
    }
    connmgr->remove(fd);
}