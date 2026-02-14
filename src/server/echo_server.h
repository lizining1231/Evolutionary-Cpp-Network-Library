#ifndef ECHO_SERVER_H
#define ECHO_SERVER_H

#include<sys/types.h>
#include<unistd.h>
#include <string>

class Socket{
    public:
    explicit Socket(int port);
    ~Socket();
    int getServer_fd() const;
    

    private:
    int server_fd;
    void initSocket(int port);
    void cleanupServer();

    Socket(const Socket&)=delete;// 禁止拷贝
    Socket& operator=(const Socket&)=delete;
};

class EchoServer{ 
    public:
    EchoServer(int port);
    ~EchoServer();
    void start();


    private:
    int port;
    Socket socket;

    int acceptClient();
    void handleClient(int client_fd);
    std::string handleMessage(const char* buffer, ssize_t bytes_read);
    void cleanupClient(int client_fd);

};


#endif