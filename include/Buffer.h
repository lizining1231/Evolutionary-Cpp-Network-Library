#ifndef BUFFER_H
#define BUFFER_H

#include <string>
#include <sys/types.h>

class Buffer {
  public:
    void appendData(const char *data, ssize_t length);
    bool takeData(std::string &request, const std::string &delimeter);

  private:
    std::string recv_buffer;
};

#endif