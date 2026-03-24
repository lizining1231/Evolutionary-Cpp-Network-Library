#ifndef TCPSERVER_H
#define TCPSERVER_H

#include<sys/types.h>
#include<unistd.h>
#include<string>
#include<vector>
// 为什么类里面把public放前面，private里面声明变量放后面
// 编译的时候不应该先定义声明变量吗？不会出错吗

    // 管理单个socket的申请与释放
class Socket{
    public:
    explicit Socket(int port);    // ?
    ~Socket();
    int getServer_fd() const;
    std::string& getRecv_buffer();
    

    private:
    int server_fd;
    std::string recv_buffer;
    void initSocket(int port);
    void cleanupServer();

    Socket(const Socket&)=delete;// 禁止拷贝
    Socket& operator=(const Socket&)=delete;

    // 加移动语义?
};
     // 连接池实现，管理多个socket
     
/*class socketPool{
    public:

    private:
    
};*/

class TCPServer{ 
    public:
    TCPServer(int port);
    ~TCPServer();
    //void start();
    void eventLoop();

    private:
    int port;
    Socket socket;

    fd_set all_fds;
    fd_set read_fds;
    int max_fd;
    int server_fd;
    std::vector<int> client_fds;


    int acceptClient();
    void handleClientData(int client_fd);
    std::string handleMessage(const char* buffer, ssize_t bytes_read);
    void cleanupClient();

};


#endif