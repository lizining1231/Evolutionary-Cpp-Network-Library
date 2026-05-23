🔴 P0：数据损坏/UB，必须立即修（5 分钟零成本）
1. Buffer::takeData 硬编码 pos + 4
cpp
// Buffer.cpp 第 103 行
request = recv_buffer.substr(0, pos + 4);  // 假设分隔符长度永远是 4
recv_buffer.erase(0, pos + 4);
后果：设置分隔符为 \n 时，会多删 3 字节，静默丢数据。

修复：

cpp
size_t delim_len = delimeter.length();
request = recv_buffer.substr(0, pos + delim_len);
recv_buffer.erase(0, pos + delim_len);
成本：改两行，改完就走。

2. EventLoop::start 里 conn->recv 后悬垂指针
cpp
// TCPserver.cpp 第 199 行
conn->recv(fd);   // 可能 close(fd) + erase(conn)
conn->send(fd);   // fd 已关闭，conn 可能已被 erase
后果：

::send 用已关闭的 fd（可能已被新连接复用，发错数据）

conn->recv_buffer 访问已析构对象（未定义行为）

修复：

cpp
conn->recv(fd);
if (connmgr.getconn(fd) == nullptr) continue;  // 就这一行
conn->send(fd);
成本：加一行判断。

3. if (!this) 在成员函数里
cpp
// Connection::send 和 Connection::recv 里都有
if (!this) { return; }
后果：如果 this 真的为空，在调用 this->recv_buffer 时已经崩了，走不到这行。给人虚假的安全感。

修复：直接删除，在调用方保证指针非空。

成本：删 4 行。

4. ConnectionManager::remove 里 close(fd) 后 erase
cpp
void ConnectionManager::remove(int client_fd) {
    connections_.erase(client_fd);   // 先删 map
    poller_->removeFd(client_fd);    // poller 里会 ::close(fd)
}
后果：如果 erase 后另一个线程/回调还在用 Connection 对象，访问已析构的 recv_buffer。

当前影响小（单线程 select），但切 epoll 时如果引入多线程就会炸。

修复：先 close(fd)，从 poller 移除，最后再 erase。

cpp
poller_->removeFd(client_fd);          // 1. 关闭 fd，停止监听
connections_.erase(client_fd);         // 2. 最后删对象
成本：调整两行顺序。

🟡 P1：架构设计缺陷，epoll 重构时一并修
5. "recv 即 send" 的反模式
cpp
// EventLoop::start()
conn->recv(fd);    // 读数据 → 解析 → 直接 ::send 返回
conn->send(fd);    // 这个 send 是"处理 5 个请求"的 send
当前问题：

断连后还 send（你已遇到）

非阻塞 socket 下大响应会丢数据（::send 只能发一部分）

无法支持异步业务逻辑（handler 里不能有耗时操作）

epoll 重构时正确做法：

可读事件 → 只读 + 解析 + 生成响应 → 放入 send_buffer

可写事件 → 从 send_buffer 取数据 → ::send

读写完全分离

6. 缺少分帧策略抽象
cpp
if (!this->recv_buffer.takeData(request, delimeter_)) {
    break;
}
当前问题：

echo 服务不需要分隔符，但被迫使用

二进制协议（长度前缀）无法支持

你提出的“空字符串特殊处理”就是这个问题

epoll 重构时正确做法：

cpp
enum class FrameMode { DELIMITER, LENGTH_PREFIX, STREAM };
bool Buffer::takeData(std::string& request, FrameMode mode, ...);
7. send 里 ::send 不处理部分发送
cpp
if (::send(client_fd, response.c_str(), response.length(), 0) < 0) {
    std::cerr << " send error" << std::endl;
}
当前问题：

非阻塞 socket 下，::send 返回正数但不等于总长度（只发了部分），剩余数据直接丢弃

errno == EAGAIN 时直接当错误打日志，实际是正常情况

epoll 重构时正确做法：设置 socket 为非阻塞 + 用发送缓冲区 + 可写事件循环发送。

8. ConnectionManager 用 std::map 而非 unordered_map
cpp
std::map<int, Connection> connections_;
影响：每次查找 O(log n)，对性能影响不大（n 很小），但用 unordered_map 语义更清晰（不要求 key 有序）。

修复时机：切 epoll 时顺手改。

🟢 P2：代码质量问题，择机重构
9. 大量 DEBUG 日志未使用日志级别
cpp
std::cout << "我进入了Connection::send函数！" << std::endl;
问题：生产环境没法关闭，干扰正常输出。

修复：用 #ifdef DEBUG 或者宏控制，或者直接删掉。

10. Connection 有无参构造函数仅为 map 妥协
cpp
Connection::Connection() {}  // 当 map 找不到 key 值时会利用此默认构造函数来创建
问题：std::map::operator[] 在 key 不存在时会插入默认值，这可能不是你想要的。你的 getconn 返回 nullptr 说明你已经意识到了，但这个默认构造函数还在。

修复：getconn 用 map.find()，不依赖 operator[]，删除默认构造函数。

11. SelectPoller::closeAllClients 从未被调用
cpp
void SelectPoller::closeAllClients() {
    for (int fd : client_fds) {
        ::close(fd);
        FD_CLR(fd, &all_fds);
    }
    client_fds.clear();
}
问题：

析构函数里没调，程序退出时可能漏关 fd

SocketListener 析构只 shutdown(SHUT_WR)，没 close(listen_fd_)（实际有 ::close，但变量名是 listen_fd_）

修复：在 ~EventLoop 或 ~SelectPoller 里调用 closeAllClients。

12. Buffer 缺少可读大小判断接口
cpp
bool Buffer::takeData(std::string& request, ...) {
    size_t pos = recv_buffer.find(delimeter);
    // ...
}
问题：没有 empty() 或 readableSize() 给外部用，后续读写分离时需要。

修复：加 size_t readableSize() const { return recv_buffer.size(); }。

📊 技术债汇总
优先级	数量	修复时机	风险
🔴 P0	4	现在，5 分钟	静默丢数据、UB、fd 复用发错数据
🟡 P1	4	epoll 重构时	架构限制，扩展性差
🟢 P2	4	择机	代码异味，不影响运行
