#include "EventLoop.h"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

std::string
handleMessage(char const *request,
              size_t len) { // 之后可以改成const *std::string, 无需兼容数组

    bool is_http_request = (std::strstr(request, "HTTP/") != NULL);

    if (is_http_request) {
        // 为测试工具提供HTTP响应
        return "HTTP/1.1 200 OK\r\n"
               "Content-Type: Text/plain\r\n"
               "Content-Length: 12\r\n"
               "Connection: keep-alive\r\n"
               "\r\n"
               "hello,world\n";
    } else {
        // 正常TCP响应
        return std::string(request, len);
    }
}

int main(int argc, char *argv[]) {
    int port = 8080;

    if (argc >= 2) {
        port = std::atoi(argv[1]);

        if (port <= 0 || port > 65535) {
            std::cerr << "错误: 端口号必须在1-65535之间" << std::endl;
            return 1;
        }
    }

    std::cout << "端口：" << port << std::endl;

    try {

        unsigned int thread_count = std::thread::hardware_concurrency();
        std::vector<std::thread> threads;

        std::cout << "Starting " << thread_count << " threads" << std::endl;

        for (unsigned int i = 0; i < thread_count; i++) {
            threads.emplace_back([port, i]() {
                EventLoop loop(port);
                loop.setMessageCallback(handleMessage);
                loop.setDelimeter("\r\n\r\n");
                std::cout << "[Thread " << i << "] started" << std::endl;
                loop.start();
            });
        }

        for (auto &t : threads) {
            t.join();
        }

        std::cout << "TCP server stopped" << std::endl;
        return 0;

    } catch (const std::runtime_error &e) {
        std::cerr << "运行时错误：" << e.what() << std::endl;
        return 1;

    } catch (const std::exception &e) {
        std::cerr << "标准异常：" << e.what() << std::endl;
        return 1;

    } catch (...) {
        std::cerr << "未知异常" << std::endl;
        return 1;
    }
}
