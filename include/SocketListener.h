#ifndef SOCKETLISTENER_H
#define SOCKETLISTENER_H

class SocketListener {
  public:
    explicit SocketListener(int port);
    ~SocketListener();
    int getFd() const;
    int accept();
    void close();

  private:
    int listen_fd_;
    void initSocket(int port);

    SocketListener(const SocketListener &) = delete; // 禁止拷贝
    SocketListener &operator=(const SocketListener &) = delete;
};

#endif