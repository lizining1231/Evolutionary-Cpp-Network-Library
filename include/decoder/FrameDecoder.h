#ifndef FRAMEDECODER_H
#define FRAMEDECODER_H

#include "Buffer.h"
#include <memory>
#include <optional>
#include <string>

class FrameDecoder {
  public:
    virtual std::optional<std::string> decode(Buffer &buf) = 0;
    virtual std::unique_ptr<FrameDecoder> clone() const = 0;
    virtual ~FrameDecoder() = default;
};

#endif