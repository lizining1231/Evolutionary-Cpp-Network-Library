#include <gtest/gtest.h>
#include <string>
#include <optional>
#include "Buffer.h"
#include "decoder/DelimiterDecoder.h"

// 完整消息：发送带分隔符的完整消息，应能正确解析
TEST(DelimiterDecoderTest, FullMessage) {
    Buffer buf;
    DelimiterDecoder decoder("\r\n\r\n");
    buf.appendData("hello\r\n\r\n", 10);
    auto result = decoder.decode(buf);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, "hello\r\n\r\n");
}

// 半包消息：发送不完整的消息（没到分隔符），应返回空
TEST(DelimiterDecoderTest, PartialMessage) {
    Buffer buf;
    DelimiterDecoder decoder("\r\n\r\n");
    buf.appendData("hello\r\n", 7);
    auto result = decoder.decode(buf);
    EXPECT_FALSE(result.has_value());
}

// 连续消息：一个 Buffer 中有两条完整消息，应只解析第一条，第二条留在缓冲区
TEST(DelimiterDecoderTest, MultipleMessages) {
    Buffer buf;
    DelimiterDecoder decoder("\r\n\r\n");
    buf.appendData("msg1\r\n\r\nmsg2\r\n\r\n", 16);
    auto result1 = decoder.decode(buf);
    EXPECT_TRUE(result1.has_value());
    EXPECT_EQ(*result1, "msg1\r\n\r\n");
    EXPECT_EQ(buf.readableBytes(), 8);
    EXPECT_EQ(std::string(buf.peek(), 8), "msg2\r\n\r\n");
}

// 空缓冲区：空 Buffer 应该返回空
TEST(DelimiterDecoderTest, EmptyBuffer) {
    Buffer buf;
    DelimiterDecoder decoder("\r\n\r\n");
    auto result = decoder.decode(buf);
    EXPECT_FALSE(result.has_value());
}

// clone 方法：验证多态拷贝
TEST(DelimiterDecoderTest, Clone) {
    DelimiterDecoder decoder("\r\n\r\n");
    auto cloned = decoder.clone();
    EXPECT_NE(cloned, nullptr);
}
