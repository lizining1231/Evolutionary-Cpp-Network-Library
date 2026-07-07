#ifndef BUFFER_H
#define BUFFER_H

#include <string>
#include <sys/types.h>

class Buffer {
  public:
    void appendData(const char *data, ssize_t length);

    const char* peek() const;
    size_t readableBytes() const;
    void retrieve(size_t len);

  private:
    std::string recv_buffer_;
};

#endif