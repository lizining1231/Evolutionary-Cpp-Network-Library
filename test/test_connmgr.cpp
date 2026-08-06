#include <gtest/gtest.h>
#include <sys/socket.h>
#include "Config.h"
#include "ConnectionManager.h"
#include "Poller.h"

TEST(ConnectionManagerTest, AddAndGetConn) {
    Config cfg;
    int sv[2];
    socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
    Poller poller(sv[0], cfg);
    ConnectionManager cm(&poller, cfg);
    cm.add(sv[1]);
    Connection *conn = cm.getconn(sv[1]);
    EXPECT_NE(conn, nullptr);
    close(sv[0]);
    close(sv[1]);
}

TEST(ConnectionManagerTest, GetNonExistentConn) {
    Config cfg;
    int sv[2];
    socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
    Poller poller(sv[0], cfg);
    ConnectionManager cm(&poller, cfg);
    EXPECT_EQ(cm.getconn(999), nullptr);
    close(sv[0]);
    close(sv[1]);
}

TEST(ConnectionManagerTest, RemoveConn) {
    Config cfg;
    int sv[2];
    socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
    Poller poller(sv[0], cfg);
    ConnectionManager cm(&poller, cfg);
    cm.add(sv[1]);
    cm.remove(sv[1]);
    EXPECT_EQ(cm.getconn(sv[1]), nullptr);
    close(sv[0]);
    close(sv[1]);
}

TEST(ConnectionManagerTest, ForEachConn) {
    Config cfg;
    int sv[2];
    socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
    Poller poller(sv[0], cfg);
    ConnectionManager cm(&poller, cfg);
    cm.add(sv[1]);
    int count = 0;
    cm.forEachConn([&count](Connection *) { count++; });
    EXPECT_EQ(count, 1);
    close(sv[0]);
    close(sv[1]);
}

// AddNegativeFd：验证 add 方法的负 fd 分支
TEST(ConnectionManagerTest, AddNegativeFd) {
    Config cfg;
    int sv[2];
    socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
    Poller poller(sv[0], cfg);
    ConnectionManager cm(&poller, cfg);
    cm.add(-1);  // 测试负 fd，应该直接返回，不崩溃
    EXPECT_EQ(cm.getconn(-1), nullptr);
    close(sv[0]);
    close(sv[1]);
}
