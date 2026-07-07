#include "decoder/DelimiterDecoder.h"

DelimiterDecoder::DelimiterDecoder(std::string delimiter)
    : delimiter_(std::move(delimiter)){}

std::optional<std::string> DelimiterDecoder::decode(Buffer &buf) {
    std::string request;

    const char* data = buf.peek();
    size_t data_len = buf.readableBytes();
    std::string_view view(data, data_len);

    size_t pos = view.find(delimiter_);
    if (pos == std::string::npos) {
        return std::nullopt; // 如果没找到字符串就返回false, 调用层进行处理

    } else {
        size_t msg_len = pos + delimiter_.length();
        request = std::string(data, msg_len);
        buf.retrieve(msg_len);
        
        return request;
    }
}

std::unique_ptr<FrameDecoder> DelimiterDecoder::clone() const {
    return std::make_unique<DelimiterDecoder>(delimiter_);
}
