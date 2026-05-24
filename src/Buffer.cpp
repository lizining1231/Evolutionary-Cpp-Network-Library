#include "Buffer.h"

void Buffer::appendData(const char *data, ssize_t len) {
    recv_buffer.append(data, len);
}

bool Buffer::takeData(std::string &request, const std::string &delimeter) {
    size_t pos = recv_buffer.find(delimeter);

    if (pos == std::string::npos) {
        return false; // 如果没找到字符串就返回false, 调用层进行处理
    } else {
        request = recv_buffer.substr(0, pos + 4);
        recv_buffer.erase(0, pos + 4);
        return true;
    }
}