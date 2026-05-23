#include "TCPserver.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#define BACKLOG 128

SocketListener::SocketListener(int port) : listen_fd_(-1) { initSocket(port); }

SocketListener::~SocketListener() { close(); }

int SocketListener::getFd() const { return listen_fd_; }

void SocketListener::initSocket(int port) {
    // 设置套接字
    listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);

    if (listen_fd_ < 0) {
        throw std::runtime_error("Socket creation failed");
    }

    int old_flags = fcntl(listen_fd_, F_GETFL, 0);
    fcntl(listen_fd_, F_SETFL, old_flags | O_NONBLOCK);

    // 设置套接字选项
    int opt = 1;
    if (setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) <
        0) {
        throw std::runtime_error("Setsocketopt failed");
    }

    // 绑定地址、端口
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(listen_fd_, (sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        throw std::runtime_error("Bind failed");
    }

    // 监听
    if (listen(listen_fd_, BACKLOG) < 0) {
        throw std::runtime_error("Listen failed");
    }
    std::cout << "Server listening on port" << port << std::endl;
}

int SocketListener::accept() {
    sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);

    int client_fd = ::accept(listen_fd_, (sockaddr *)&client_addr, &client_len);

    if (client_fd < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return -1;
        }

        throw std::runtime_error(std::string("Accept failed:") +
                                 strerror(errno));
    }

    int old_flags = fcntl(client_fd, F_GETFL, 0);
    fcntl(client_fd, F_SETFL, old_flags | O_NONBLOCK);

    // 将二进制的IP地址转换成字符串
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);

    // 将网络字节序转化为主机字节序
    std::cout << client_ip << ":" << ntohs(client_addr.sin_port)
              << "(fd:" << client_fd << ")" << std::endl;

    return client_fd;
}

void SocketListener::close() {
    if (listen_fd_ >= 0) {
        shutdown(listen_fd_, SHUT_WR); // 发送FIN
        ::close(listen_fd_);

        listen_fd_ = -1;
    }
}

// SocketListener类没有无参构造函数，这里添加显示构造
Poller::Poller(int listen_fd) : listen_fd_(listen_fd) {
    epoll_fd_ = epoll_create1(0);

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = listen_fd;
    epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, listen_fd, &ev);
}

void Poller::addFd(int client_fd) {
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = client_fd;
    epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, client_fd, &ev);
}

void Poller::wait() {
    ready_count_ = epoll_wait(epoll_fd_, events_, MAX_EVENTS, 1);

    if (ready_count_ < 0) {
        throw std::runtime_error(std::string("epoll_wait:") + strerror(errno));
    }
}

void Poller::removeFd(int client_fd) {
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = client_fd;
    epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, client_fd, &ev);

    ::close(client_fd);
}

void Poller::enableOut(int client_fd) {
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLOUT;
    ev.data.fd = client_fd;
    epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, client_fd, &ev);
}

void Poller::disableOut(int client_fd) {
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = client_fd;
    epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, client_fd, &ev);
}

int Poller::getEpoll_fd() { return epoll_fd_; }

void Buffer::appendData(const char *data, ssize_t len) {
    recv_buffer.append(data, len);
}

bool Buffer::takeData(std::string &request, const std::string &delimeter) {
    size_t pos = recv_buffer.find(delimeter);

    if (pos == std::string::npos) {
        return false; // 如果没找到字符串就返回false, 调用层进行处理
    } else {
        request = recv_buffer.substr(0, pos + 4);
        recv_buffer.erase(0, pos + 4);
        return true;
    }
}

Connection::Connection(int client_fd) : client_fd(client_fd) {}

// 当map找不到key值时会利用此默认构造函数来创建
Connection::Connection() { poller_ = nullptr; }

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
    connections_.erase(client_fd);
    poller_->removeFd(client_fd);
}

void Connection::setCloseCallback(CloseCallback close_cb,
                                  ConnectionManager *connmgr) {
    close_cb_ = close_cb;
    connmgr_ = connmgr;
}

void onConnectionClose(int fd, ConnectionManager *connmgr) {
    if (!connmgr) {
        return;
    }
    connmgr->remove(fd);
}

void Connection::setMessageCallback(MessageCallback handleMessage) {
    handler = handleMessage;
}

void Connection::setDelimeter(const std::string &delim) { delimeter_ = delim; }

void Connection::recv(int client_fd) {
    if (this == nullptr) {
        return;
    }

    char temp_buffer[65536];
    ssize_t bytes_read;

    bytes_read = ::recv(client_fd, temp_buffer, sizeof(temp_buffer), 0);

    if (bytes_read <= 0) {
        if (bytes_read == 0) {
            std::cout << "Client disconnected" << std::endl;
            close_cb_(client_fd, connmgr_);
            return;
        } else {
            // bytes_read<0
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return;
            } else {
                std::cerr << "Receive error" << std::endl;
                close_cb_(client_fd, connmgr_);
                return;
            }
        }
    }

    // 为进行职责分离, 将临时缓冲区的数据追加到永久缓冲区, 永久缓冲区负责处理
    if (this) {
        this->recv_buffer.appendData(temp_buffer, bytes_read);
    } // 不用写else，因为如果没有conn什么都不用做
}

void Connection::send(int client_fd) {
    std::string request;
    bool is_need_restore = false;

    while (recv_buffer.takeData(
        request, delimeter_)) { // 循环直到无数据，打包发送到缓冲区

        if (!handler) {
            std::cout << "handler 为空!\n";
            continue;
        }

        std::string response = handler(request.c_str(), request.size());
        send_buffer.append(response);
    }

    while (!send_buffer.empty()) {
        ssize_t n =
            ::send(client_fd, send_buffer.c_str(), send_buffer.length(), 0);

        if (n > 0) {
            send_buffer.erase(0, n);
        } else {
            // n < 0
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                poller_->enableOut(client_fd);
                is_need_restore = true;

                return;
            } else {
                std::cerr << " send error" << std::endl;
                close_cb_(client_fd, connmgr_);
                return;
            }
        }
    }

    if (is_need_restore) {
        poller_->disableOut(client_fd);
        is_need_restore = false;
    }
}

// EventLoop类的实现
EventLoop::EventLoop(int port)
    : listener(port), poller(listener.getFd()), connmgr(&poller),
      delimeter_("") {
    signal(SIGPIPE, SIG_IGN); // 防止Fin包未收到时send数据导致进程被砍
    std::cout << "the initialized TCP server on port" << port << std::endl;
}

EventLoop::~EventLoop() {}

void EventLoop::setMessageCallback(MessageCallback cb) { user_handler = cb; }

void EventLoop::setDelimeter(const std::string &delim) { delimeter_ = delim; }

void EventLoop::start() {
    int listen_fd = listener.getFd();

    running_ = true;

    while (running_) {
        poller.wait();

        for (int i = 0; i < poller.ready_count_; i++) {
            int fd = poller.events_[i].data.fd;

            if (fd == listen_fd) {
                int new_fd = listener.accept();
                connmgr.add(new_fd);
                Connection *conn = connmgr.getconn(new_fd);

                conn->setMessageCallback(user_handler);
                conn->setCloseCallback(onConnectionClose, &connmgr);
                conn->setDelimeter(delimeter_);
                conn->poller_ = &poller;

            } else {
                uint32_t events = poller.events_[i].events;
                Connection *conn = connmgr.getconn(fd);

                if (conn == nullptr) {
                    continue;
                }

                if (events & EPOLLIN) {
                    conn->recv(fd);
                    conn = connmgr.getconn(fd); // 防止recv失败后send读到野指针
                    if (conn == nullptr) {
                        continue;
                    }
                    conn->send(fd);
                }

                if (events & EPOLLOUT) {
                    conn = connmgr.getconn(fd);
                    if (conn == nullptr) {
                        continue;
                    }
                    conn->send(fd);
                }
            }
        }
    }
}

void EventLoop::stop() { running_ = false; }