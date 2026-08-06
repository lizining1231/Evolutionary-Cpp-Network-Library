#include <gtest/gtest.h>
#include <string>
#include "Buffer.h"

TEST(BufferTest, AppendAndRetrieve) {
    Buffer buf;
    buf.appendData("hello", 5);
    EXPECT_EQ(buf.readableBytes(), 5);
    EXPECT_EQ(std::string(buf.peek(), 5), "hello");
    buf.retrieve(5);
    EXPECT_EQ(buf.readableBytes(), 0);
}

TEST(BufferTest, MultipleAppend) {
    Buffer buf;
    buf.appendData("ab", 2);
    buf.appendData("cd", 2);
    EXPECT_EQ(buf.readableBytes(), 4);
    EXPECT_EQ(std::string(buf.peek(), 4), "abcd");
}

TEST(BufferTest, PartialRetrieve) {
    Buffer buf;
    buf.appendData("hello world", 11);
    buf.retrieve(6);
    EXPECT_EQ(buf.readableBytes(), 5);
    EXPECT_EQ(std::string(buf.peek(), 5), "world");
}

TEST(BufferTest, EmptyBuffer) {
    Buffer buf;
    EXPECT_EQ(buf.readableBytes(), 0);
}

