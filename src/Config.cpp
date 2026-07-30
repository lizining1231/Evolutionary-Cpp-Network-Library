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

    #define X(field, json_key, cli_flag, default_val) \
        if (j.contains(json_key)) field = j[json_key];
    CONFIG_ITEMS(X)
    #undef X

}

void Config::parseCommandLine(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "--config" && i + 1 < argc) {
            config_path = argv[++i];
            continue;
        }
        if (arg == "--help") {
            // 后续添加
        }

        // int 类型的参数配置由 X-macro 生成
        #define X(field, json_key, cli_flag, default_val) \
            if (arg == cli_flag && i + 1 < argc) {        \
                field = std::stoi(argv[++i]);             \
                continue;                                 \
            }
        CONFIG_ITEMS(X)
        #undef X
    }
}
