#ifndef CONFIG_H
#define CONFIG_H

#include <string>

struct Config {
    int backlog = 128;
    int thread_count = 4;
    int port = 8080;
    int max_events = 4096;
    int epoll_timeout_ms = 100;
    int recv_buffer_size = 65536;

    void loadFromFile(const std::string& path);
    void parseCommandLine(int argc, char** argv);
};

#endif
