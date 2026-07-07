#include "Buffer.h"

void Buffer::appendData(const char *data, ssize_t len) {
    recv_buffer_.append(data, len);
}

bool Buffer::takeData(std::string &request, const std::string &delimiter) {
    const char* data = peek();
    size_t data_len = readableBytes();
    std::string view(data, data_len);

    size_t pos = view.find(delimiter);
    if (pos == std::string::npos) {
        return false; // 如果没找到字符串就返回false, 调用层进行处理

    } else {
        size_t msg_len = pos + delimiter.length();
        request = std::string(data, msg_len);
        retrieve(msg_len);
        
        return true;
    }
}

const char* Buffer::peek() const {
    return recv_buffer_.data();
}

size_t Buffer::readableBytes() const {
    return recv_buffer_.size();
}

void Buffer::retrieve(size_t len) {
    recv_buffer_.erase(0, len);
}