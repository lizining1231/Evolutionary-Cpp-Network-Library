#include "EventLoop.h"
#include "decoder/DelimiterDecoder.h"
#include "logger.h"
#include "Config.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

std::atomic<bool> stopping{false};

void signalHandle(int) {
    stopping = true;
}

std::string handleMessage(char const *request,
                          size_t len) {  // 之后可以改成const *std::string, 无需兼容数组

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
    Config cfg;
    cfg.init(argc, argv);              // 默认值 → config.json → CLI

    if (cfg.dump_config_) {
        cfg.dump();
        return 0;
    }

    LOG_INFO("Port: %d", cfg.port);
    LOG_INFO("Threads: %d", cfg.thread_count);

    try {
        signal(SIGINT, signalHandle);
        signal(SIGTERM, signalHandle);

        unsigned int thread_count = 4;
        std::vector<std::thread> threads;

        LOG_INFO("Starting %u threads", thread_count);

        std::vector<std::unique_ptr<EventLoop>> loops;

        for (unsigned int i = 0; i < thread_count; i++) {
            auto loop = std::make_unique<EventLoop>(cfg);
            loop->setMessageCallback(handleMessage);
            loop->setDecoder(std::make_unique<DelimiterDecoder>("\r\n\r\n"));
            loops.push_back(std::move(loop));
        }

        for (unsigned int i = 0; i < thread_count; i++) {
            threads.emplace_back([ptr = loops[i].get(), i]() {
                LOG_INFO("[Thread %u] starting...", i);
                ptr->start();
            });
        }

        while (!stopping) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        for (unsigned int i = 0; i < thread_count; i++) {
            loops[i]->stop();
        }

        for (auto &t : threads) {
            t.join();
        }

        LOG_INFO("TCP server stopped");
        return 0;

    } catch (const std::runtime_error &e) {
        LOG_ERROR("Runtime error: %s", e.what());
        return 1;

    } catch (const std::exception &e) {
        LOG_ERROR("Standard exception: %s", e.what());
        return 1;

    } catch (...) {
        LOG_ERROR("Unknown exception");
        return 1;
    }
}
