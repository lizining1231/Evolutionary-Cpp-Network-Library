#ifndef DELIMITERDECODER_H
#define DELIMITERDECODER_H

#include "decoder/FrameDecoder.h"

class DelimiterDecoder : public FrameDecoder {
  public:
    explicit DelimiterDecoder(std::string delimiter);

    std::optional<std::string> decode(Buffer &buf) override;
    std::unique_ptr<FrameDecoder> clone() const override;

  private:
    std::string delimiter_;
};

#endif
