#include <gtest/gtest.h>
#include <sys/socket.h>
#include "ConnectionManager.h"
#include "Poller.h"

TEST(ConnectionManagerTest, AddAndGetConn) {
    int sv[2];
    socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
    Poller poller(sv[0]);
    ConnectionManager cm(&poller);
    cm.add(sv[1]);
    Connection *conn = cm.getconn(sv[1]);
    EXPECT_NE(conn, nullptr);
    close(sv[0]);
    close(sv[1]);
}

TEST(ConnectionManagerTest, GetNonExistentConn) {
    int sv[2];
    socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
    Poller poller(sv[0]);
    ConnectionManager cm(&poller);
    EXPECT_EQ(cm.getconn(999), nullptr);
    close(sv[0]);
    close(sv[1]);
}

TEST(ConnectionManagerTest, RemoveConn) {
    int sv[2];
    socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
    Poller poller(sv[0]);
    ConnectionManager cm(&poller);
    cm.add(sv[1]);
    cm.remove(sv[1]);
    EXPECT_EQ(cm.getconn(sv[1]), nullptr);
    close(sv[0]);
    close(sv[1]);
}

TEST(ConnectionManagerTest, ForEachConn) {
    int sv[2];
    socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
    Poller poller(sv[0]);
    ConnectionManager cm(&poller);
    cm.add(sv[1]);
    int count = 0;
    cm.forEachConn([&count](Connection *) { count++; });
    EXPECT_EQ(count, 1);
    close(sv[0]);
    close(sv[1]);
}

// onConnectionClose：验证回调函数，包括 nullptr 分支
TEST(ConnectionManagerTest, OnConnectionClose) {
    int sv[2];
    socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
    Poller poller(sv[0]);
    ConnectionManager cm(&poller);
    cm.add(sv[1]);

    // 测试 connmgr 为 nullptr 的分支
    onConnectionClose(sv[1], nullptr);

    // 测试正常分支
    onConnectionClose(sv[1], &cm);
    EXPECT_EQ(cm.getconn(sv[1]), nullptr);

    close(sv[0]);
    close(sv[1]);
}

// AddNegativeFd：验证 add 方法的负 fd 分支
TEST(ConnectionManagerTest, AddNegativeFd) {
    int sv[2];
    socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
    Poller poller(sv[0]);
    ConnectionManager cm(&poller);
    cm.add(-1);  // 测试负 fd，应该直接返回，不崩溃
    EXPECT_EQ(cm.getconn(-1), nullptr);
    close(sv[0]);
    close(sv[1]);
}
