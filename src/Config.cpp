#include "Config.h"

#include <fstream>
#include <string>

/*void Config::loadFromFile(const std::string& path) {
    std::ifstream file(path);
    if(!file.is_open()) {
        return;
    }

    // JSON解析
}*/

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

        if (arg == "--help") {
            // 后续添加
        }

    }
}
