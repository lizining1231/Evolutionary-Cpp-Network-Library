#include "Buffer.h"

void Buffer::appendData(const char *data, ssize_t len) {
    recv_buffer_.append(data, len);
}

bool Buffer::takeData(std::string &request, const std::string &delimiter) {
    size_t pos = recv_buffer_.find(delimiter);

    if (pos == std::string::npos) {
        return false; // 如果没找到字符串就返回false, 调用层进行处理
    } else {
        size_t delim_len = delimiter.length();
        request = recv_buffer_.substr(0, pos + delim_len);
        recv_buffer_.erase(0, pos + delim_len);
        return true;
    }
}