#ifndef CONFIG_H
#define CONFIG_H

#include <string>

#define CONFIG_ITEMS(X) \
    X(port,             "port",             "--port",             8080)   \
    X(thread_count,     "threads",          "--threads",          4)      \
    X(backlog,          "backlog",          "--backlog",          128)    \
    X(max_events,       "max-events",       "--max-events",       4096)   \
    X(epoll_timeout_ms, "epoll-timeout",    "--epoll-timeout",    100)    \
    X(recv_buffer_size, "recv-buffer",      "--recv-buffer",      65536)

struct Config {
    #define X(field, json_key, cli_flag, default_val) int field = default_val;
    CONFIG_ITEMS(X)
    #undef X

    std::string config_path = "config.json";

    void init(int argc, char** argv);
    void loadFromFile(const std::string& path);
    void parseCommandLine(int argc, char** argv);
};

#endif
