#include"echo_server.h"
#include<iostream>
#include<cstring>
#include<unistd.h>
#include<arpa/inet.h>
#include<sys/socket.h>
#include<stdexcept>
#include <cerrno>
#include <cstring>
#include<netinet/tcp.h>

#define BUFFER_SIZE 1024
#define BACKLOG 1

EchoServer::EchoServer(int port):client_fd(-1),server_fd(-1),port(port){
      std::cout<<"调试：构造函数被调用"<<std::endl;// 调试
    std::cout<<"the initialized echo server on port"<<port<<std::endl;
};
EchoServer::~EchoServer(){
     std::cout<<"调试：析构函数被调用"<<std::endl;// 调试
    stop();
};


void EchoServer::start(){
    std::cout<<"调试：start()函数被调用"<<std::endl;// 调试

    setupSocket();
    acceptClient();
    handleClient();

    cleanup();
}   


void EchoServer::stop(){
    std::cout<<"调试：stop()函数被调用"<<std::endl;// 调试
    cleanup();
    std::cerr<<"Server stoped"<<std::endl;
}


void EchoServer::cleanup(){
    std::cout<<"调试：cleanup()函数被调用"<<std::endl;// 调试
    if(client_fd>=0){
        shutdown(client_fd, SHUT_WR);// 发送FIN
        sleep(1);// 等1秒，让客户端有机会回应
        close(client_fd);
        client_fd=-1;
    }

    if(server_fd>=0){
        shutdown(server_fd, SHUT_WR);// 发送FIN

        close(server_fd);
        server_fd=-1;
    }
}


void EchoServer::setupSocket(){
     std::cout<<"调试：setupSocket()函数被调用"<<std::endl;// 调试
    // 设置套接字
    server_fd=socket(AF_INET,SOCK_STREAM,0);
    if(server_fd<0){
        throw std::runtime_error("Socket creation failed");
    }

    // 设置套接字选项
    int opt=1;
    if(setsockopt(server_fd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt))<0){
        throw std::runtime_error("Setsocketopt failed");
    }

    // 绑定地址、端口
    sockaddr_in server_addr{};
    server_addr.sin_family=AF_INET;
    server_addr.sin_addr.s_addr=INADDR_ANY;
    server_addr.sin_port=htons(port);

    if(bind(server_fd,(sockaddr*)&server_addr,sizeof(server_addr))<0){
        throw std::runtime_error("Bind failed");
    }

    //监听
    if(listen(server_fd,BACKLOG)<0){
        throw std::runtime_error("Listen failed");
    }

    std::cout<<"Server listening on port"<<port<<std::endl;

}


void EchoServer::acceptClient(){
      std::cout<<"调试：acceptClient()函数被调用"<<std::endl;// 调试
    std::cout<<"Waiting for client connection..."<<std::endl;

    sockaddr_in client_addr{};
    socklen_t client_len=sizeof(client_addr);

    // 阻塞等待客户端连接
    client_fd=accept(server_fd,(sockaddr*)&client_addr,&client_len);

    if(client_fd<0){
        throw std::runtime_error(
            std::string("Accept failed:")+strerror(errno)
        );
         int flag = 1;
    
    // 1. 禁用Nagle算法（立即发送数据）
    if(setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag)) < 0){
        std::cerr << "Warning: Failed to set TCP_NODELAY" << std::endl;
    }
    
    // 2. 禁用延迟确认（立即发送ACK）
    flag = 1;
    if(setsockopt(client_fd, IPPROTO_TCP, TCP_QUICKACK, &flag, sizeof(flag)) < 0){
        std::cerr << "Warning: Failed to set TCP_QUICKACK" << std::endl;
    }
    
    // 3. 设置TCP立即发送FIN（不等待）
    struct linger linger_opt;
    linger_opt.l_onoff = 1;      // 启用linger
    linger_opt.l_linger = 0;     // 超时时间为0，立即关闭
    if(setsockopt(client_fd, SOL_SOCKET, SO_LINGER, &linger_opt, sizeof(linger_opt)) < 0){
        std::cerr << "Warning: Failed to set SO_LINGER" << std::endl;
    }
    }

    // 将二进制的IP地址转换成字符串
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET,&client_addr.sin_addr,client_ip,INET_ADDRSTRLEN);

    //将网络字节序转化为主机字节序
    std::cout<<client_ip<<":"<<ntohs(client_addr.sin_port)<<"(fd:"<<client_fd<<")"<<std::endl;

}


void EchoServer::handleClient(){
      std::cout<<"调试：handleClient()函数被调用"<<std::endl;// 调试
    char buffer[BUFFER_SIZE];
    while(true){
        ssize_t bytes_read=recv(client_fd,buffer,BUFFER_SIZE-1,0);

        if(bytes_read<=0){
            if(bytes_read==0){
                std::cout<<"Client disconnected"<<std::endl;
              
            }
            else{
                std::cerr<<"Receive error"<<std::endl;
                
            }
            break;
        }

        buffer[bytes_read]='\0';// 读取到bytes_read字节，设此字节为‘\0’

        if(send(client_fd,buffer,bytes_read,0)<0){
            std::cerr<<"Send error"<<std::endl;
            break;
        }
        std::cout<<"recv num："<<bytes_read<<std::endl;
    }

}

