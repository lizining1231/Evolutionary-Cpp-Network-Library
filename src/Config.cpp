#include "Config.h"

#include "nlohmann/json.hpp"

#include <fstream>
#include <string>

using json = nlohmann::json;

void Config::init(int argc, char** argv) {
    // 先扫 --config，确定 JSON 路径
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "--config" && i + 1 < argc) {
            config_path = argv[i + 1];
            break;
        }
    }
    loadFromFile(config_path);     // 加载 JSON（覆盖默认值）
    parseCommandLine(argc, argv);   // 解析 CLI（覆盖 JSON）
}

void Config::loadFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return;
    }

    json j = json::parse(file);

    if (j.contains("port"))          port             = j["port"];
    if (j.contains("threads"))       thread_count     = j["threads"];
    if (j.contains("backlog"))       backlog          = j["backlog"];
    if (j.contains("max_events"))    max_events       = j["max_events"];
    if (j.contains("epoll_timeout")) epoll_timeout_ms = j["epoll_timeout"];
    if (j.contains("recv_buffer"))   recv_buffer_size = j["recv_buffer"];
}

void Config::parseCommandLine(int argc, char** argv) {
    for (int i = 1; i <argc; i++) {
        std::string arg = argv[i];

        if (arg == "--port" && i + 1 < argc) {
            port = std::stoi(argv[i + 1]);
            i += 1;
            continue;
        }

        if (arg == "--threads" && i + 1 < argc) {
            thread_count = std::stoi(argv[i + 1]);
            i += 1;
            continue;
        }

        if (arg == "--backlog" && i + 1 < argc) {
            backlog = std::stoi(argv[i + 1]);
            i += 1;
            continue;
        }

        if (arg == "--max-events" && i + 1 < argc) {
            max_events = std::stoi(argv[i + 1]);
            i += 1;
            continue;
        }

        if (arg == "--epoll-timeout" && i + 1 < argc) {
            epoll_timeout_ms = std::stoi(argv[i + 1]);
            i += 1;
            continue;
        }

        if (arg == "--recv-buffer" && i + 1 < argc) {
            recv_buffer_size = std::stoi(argv[i + 1]);
            i += 1;
            continue;
        }

        if (arg == "--config" && i + 1 < argc) {
            config_path = argv[i + 1];
            i += 1;
            continue;
        }

        if (arg == "--help") {
            // 后续添加
        }

    }
}
