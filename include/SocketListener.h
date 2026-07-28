#ifndef SOCKETLISTENER_H
#define SOCKETLISTENER_H

#include "Config.h"

class SocketListener {
public:
    explicit SocketListener(Config& cfg);
    ~SocketListener();
    int getFd() const;
    int accept();
    void close();

private:
    int listen_fd_;
    void initSocket(int port, int backlog);

    SocketListener(const SocketListener &) = delete;  // 禁止拷贝
    SocketListener &operator=(const SocketListener &) = delete;
};

#endif
