#include "server/echo_server.h"
#include<iostream>

int main(void){
    try{
        std::cout<<"echo server 启动"<<std::endl;

        EchoServer server(8080);
        server.start();

        std::cout<<"echo server 关闭"<<std::endl;
        return 0;

    }catch(const std::runtime_error&e){
        std::cerr<<"运行时错误："<<e.what()<<std::endl;
        return 1;

    }catch(const std::exception&e){
        std::cerr<<"标准异常："<<e.what()<<std::endl;
        return 1;

    }catch(...){
        std::cerr<<"未知异常"<<std::endl;
        return 1;
    }
    
}
