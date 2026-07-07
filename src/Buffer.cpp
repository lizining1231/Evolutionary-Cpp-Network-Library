#include "Buffer.h"

void Buffer::appendData(const char *data, ssize_t len) {
    recv_buffer_.append(data, len);
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