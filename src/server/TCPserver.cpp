#include "TCPserver.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdexcept>
#include <cerrno>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <vector> 
#include <algorithm>
#include <sys/time.h>
#include <string>

#define BACKLOG 128

Socket::Socket(int port):server_fd(-1){
    initSocket(port);
}

Socket::~Socket(){
    cleanupServer();
}

int Socket::getServer_fd() const{
    return server_fd;
}

std::string& Socket::getRecv_buffer(){    // handleClient()要修改该成员变量，所以采取引用
    return recv_buffer;
}

void Socket::initSocket(int port){
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



void Socket::cleanupServer(){
     if(server_fd>=0){
        shutdown(server_fd, SHUT_WR);// 发送FIN

        close(server_fd);

        server_fd=-1;
    }
}


// TCPServer类的实现
TCPServer::TCPServer(int port):socket(port),port(port){
    int client_fd=-1;
    
    std::cout<<"the initialized TCP server on port"<<port<<std::endl;
}

TCPServer::~TCPServer(){}

    
void TCPServer::eventLoop(){

    server_fd=socket.getServer_fd();

    FD_ZERO(&all_fds);
    FD_SET(server_fd,&all_fds);

    max_fd=server_fd;
    
    std::cout<<"Waiting for client connection..."<<std::endl;

    while(1){

    read_fds=all_fds;

    timeval tv;
    tv.tv_sec=0;
    tv.tv_usec=1000;
    
    // 防止select阻塞导致P99飙升
    int activity=select(max_fd+1,&read_fds,NULL,NULL,&tv);

    if(activity<0){
        throw std::runtime_error(std::string("select:")+strerror(errno));
        continue;
        }

    if(FD_ISSET(server_fd,&read_fds)){
        acceptClient();
        }

    for(int fd:client_fds){
        if (FD_ISSET(fd, &read_fds)) {  // 检查这个fd是否有数据
            handleClientData(fd);
            }
        }
    }       
    cleanupClient(); 
}



int TCPServer::acceptClient(){
    sockaddr_in client_addr{};
    socklen_t client_len=sizeof(client_addr);

    //客户端的client_fd作为局部变量，每个连接独立管理，互不干扰
    int client_fd=accept(server_fd,(sockaddr*)&client_addr,&client_len);

    if(client_fd<0){
        throw std::runtime_error(
            std::string("Accept failed:")+strerror(errno)
        );
    }
    if(client_fd>0){
        FD_SET(client_fd,&all_fds);   //把新客户端加入被监听队伍
        client_fds.push_back(client_fd);
    }
    if(client_fd>max_fd){
        max_fd=client_fd;   // client_fd是递增的，可以用来重新设置最大值
    }

    // 将二进制的IP地址转换成字符串
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET,&client_addr.sin_addr,client_ip,INET_ADDRSTRLEN);

    //将网络字节序转化为主机字节序
    std::cout<<client_ip<<":"<<ntohs(client_addr.sin_port)<<"(fd:"<<client_fd<<")"<<std::endl;

    return client_fd;
}


void TCPServer::handleClientData(int client_fd){
    // char buffer[BUFFER_SIZE];取消通用连接池，并且BUFFER_SIZE变更为RECV_BUFSIZE

    char temp_buffer[4096];
    std::string& recv_buffer=socket.getRecv_buffer();
    ssize_t bytes_read;
    size_t pos;

    bytes_read=recv(client_fd,temp_buffer,sizeof(temp_buffer),0);

    if(bytes_read<=0){

        if(bytes_read==0){
            std::cout<<"Client disconnected"<<std::endl;
        }

        else{
            std::cerr<<"Receive error"<<std::endl;
        }

        // 清理资源并返回
        close(client_fd);
        FD_CLR(client_fd,&all_fds);
        
        auto it=std::find(client_fds.begin(),client_fds.end(),client_fd);

        
        if(it!=client_fds.end()){
        client_fds.erase(it);   // 将此fd从vector中删除
        }

        if(client_fd==max_fd){
            max_fd=server_fd;   //重置fd再遍历寻找最大值
            for(int fd:client_fds){
                if(fd>max_fd)max_fd=fd;
            }
        }
        return;
    }

    
    recv_buffer.append(temp_buffer,bytes_read);    // 为进行职责分离，将临时缓冲区的数据追加到永久缓冲区，永久缓冲区负责处理

    pos=recv_buffer.find('\r\n\r\n');

    if(pos==std::string::npos){
        return;    // 如果没找到字符串直接返回继续等待请求
    }
    else{

        std::string request=recv_buffer.substr(0,pos+4);
 // 依赖反转
        // recv_buffer[bytes_read] = '\0';

        std::string response=handleMessage(request.c_str(),bytes_read);

        if(send(client_fd, response.c_str(), response.length(), 0)<0){
        std::cerr<<" send error"<<std::endl;
        }

        recv_buffer.erase(0,pos+4);

        std::cout<<"recv num: "<<bytes_read<<std::endl;
    }

}


void TCPServer::cleanupClient(){
    for(int fd:client_fds){
        close(fd);
        FD_CLR(fd,&all_fds);
    }
    client_fds.clear();
    }



std::string TCPServer::handleMessage(char const* request,ssize_t bytes_read){     // 之后可以改成const *std::string，无需兼容数组
    
    bool is_http_request=(std::strstr(request,"HTTP/")!=NULL);

        if (is_http_request) {
            // 为测试工具提供HTTP响应
            return
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: Text/plain\r\n"
            "Content-Length: 12\r\n"
            "Connection: close\r\n"
            "\r\n"
            "hello,world\n";
        }else{
            // 正常TCP响应
            return std::string(request,bytes_read);
        }
        
}

