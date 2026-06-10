# EPOLL阶段决策日志

## 6月5日：关于单线程事件循环的瓶颈确认与多线程方案选型

### 场景

当前项目为 epoll LT + 单线程事件循环，压测数据如下：

| 并发   | QPS    | 平均延迟    | P99        | 错误率 |
| ---- | ------ | ------- | ---------- | --- |
| 100  | 81k    | 1.42ms  | 6.34ms     | 0%  |
| 200  | 78k    | 2.51ms  | 7.10ms     | 0%  |
| 400  | 68k    | 25.57ms | 575ms      | 0%  |
| 600+ | 62-73k | 20-56ms | 600ms-1.1s | 0%  |

400 并发时 P99 爆增至 575ms，600 并发时超过 1s。这是单线程事件循环的**排队延迟**——所有活跃连接共享一个线程，处理顺序先到先得，排在后面的连接被迫等待前面所有连接处理完毕。

### 分析

单线程瓶颈会**掩盖**后续 IO 模型优化（epoll ET、io\_uring）的性能差异。就像用一根吸管喝水——换粗吸管还是换水龙头，水流速度都被吸管直径卡住了。因此需要将多线程提前到路线图的最前面：

```
原路线图：epoll LT → epoll ET → io_uring → 多线程
新路线图：多线程 → epoll ET → io_uring
```

### 方案

经过联网搜索业界实践，主要有三种多线程模型：

**方案 A：REUSEPORT + 一线程一 EventLoop（Nginx 模型）**

```
┌──────────────┐  ┌──────────────┐  ┌──────────────┐
│  Thread 0    │  │  Thread 1    │  │  Thread N    │
│  EventLoop   │  │  EventLoop   │  │  EventLoop   │
│  ├─ Poller   │  │  ├─ Poller   │  │  ├─ Poller   │
│  ├─ ConnMgr  │  │  ├─ ConnMgr  │  │  ├─ ConnMgr  │
│  └─ Listener │  │  └─ Listener │  │  └─ Listener │
│  (SO_REUSE)  │  │  (SO_REUSE)  │  │  (SO_REUSE)  │
└──────┬───────┘  └──────┬───────┘  └──────┬───────┘
       │                 │                 │
       └─────────┬───────┴─────────┬───────┘
                 │   :8080         │
            ┌────┴─────────────────┴────┐
            │    Linux Kernel (REUSEPORT)│
            └───────────────────────────┘
```

特点：每个线程 bind 同一端口，内核基于四元组哈希分发。零共享、零锁。

**方案 B：主线程 Accept + 分发（Master-Worker 模型）**

```
┌──────────────────────┐
│      Main Thread     │
│   accept() → dispatch │
└──┬────────┬──────┬───┘
   │ fd     │ fd   │ fd
   ▼        ▼      ▼
┌──────┐ ┌──────┐ ┌──────┐
│Worker│ │Worker│ │Worker│
│  0   │ │  1   │ │  N   │
└──────┘ └──────┘ └──────┘
```

特点：主线程负责 accept，通过线程安全队列将 fd 分发给 worker。需要锁。

**方案 C：I/O 线程池（Redis 6.0 模型）**

```
┌──────────────────────┐
│     Main Thread      │
│  epoll_wait + 业务逻辑  │
│         ↓            │
│   分配 client 到线程   │
└──┬────────┬──────┬───┘
   │        │      │
   ▼        ▼      ▼
┌──────┐ ┌──────┐ ┌──────┐
│I/O 线程│ │I/O 线程│ │I/O 线程│
│recv   │ │recv   │ │recv   │
│send   │ │send   │ │send   │
└──────┘ └──────┘ └──────┘
```

特点：主线程独占 epoll 和业务逻辑，I/O 线程只做 socket 读写。需要锁同步。

### 综合对比

| 维度             |   方案 A (REUSEPORT)  | 方案 B (Accept+分发) | 方案 C (I/O 线程池) |
| -------------- | :-----------------: | :--------------: | :------------: |
| 实现复杂度          |          低          |         中        |        高       |
| 零锁设计           |          ✅          |         ❌        |        ❌       |
| 线性扩展           |          ✅          |   ⚠️ Accept 瓶颈   |   ⚠️ 业务逻辑单线程   |
| 代码改动量          |        \~11 行       |      \~30 行      |     \~50+ 行    |
| 适配当前架构         |        🟢 完美        |      🟡 需要重构     |     🔴 需要大改    |
| 对 io\_uring 友好 |         ✅ 独立        |       ✅ 独立       |      ❌ 冲突      |
| 业界实践           | Nginx、Envoy、HAProxy |    传统 Web 服务器    |   Redis 6.0+   |

### trade-off

**方案 B 的 Accept 瓶颈：** 主线程 accept 后通过队列分发 fd，高并发新连接场景下主线程本身成为瓶颈。而且需要线程安全队列，引入锁竞争。

**方案 C 不适合我们的原因：** Redis 6.0 之所以用 I/O 线程池模型，是因为 Redis 的**业务逻辑（命令处理）必须单线程**以保证数据一致性。我们的业务逻辑（handler\_）只是简单的 echo，没有共享状态，不需要单线程保护。强行用这个模型反而把简单问题复杂化了。

**方案 A 的优势：**

1. 当前 EventLoop 已经是自包含的——Poller、ConnectionManager、SocketListener 都是值成员，不需要任何重构
2. Linux 3.9+ 的 SO\_REUSEPORT 由内核做负载均衡，无需应用层介入
3. 每个线程独立 epoll 实例，天然避免惊群效应
4. 未来切 io\_uring 时只需改 Poller 内部实现，线程模型不变

### 最终决策

**采用方案 A：REUSEPORT + 一线程一 EventLoop**

具体改动：

| 文件                   | 改动                                                         |
| -------------------- | ---------------------------------------------------------- |
| `SocketListener.cpp` | `setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEPORT, ...)`    |
| `main.cpp`           | `std::vector<std::thread>` 创建 N 个线程，每个跑 EventLoop::start() |

线程数默认使用 `std::thread::hardware_concurrency()`。

### 预期效果

| 并发     | 单线程 P99 | 多线程 P99（预期）  |
| ------ | ------- | ------------ |
| 100    | 6ms     | \~2ms        |
| 400    | 575ms   | \~10ms       |
| 600    | 1.1s    | \~15ms       |
| QPS 峰值 | \~81k   | \~250k+（4 核） |

***

## 6月8日：关于资源释放职责梳理与优雅退出方案选型

### 场景 🎯

发现 ConnectionManager::remove 中的删除顺序与职责划分不符合行业实践：

**现状：**

```
ConnectionManager::remove(fd)
    └── connections_.erase(fd)    // ① 先析构 Connection（当时不做什么）
    └── Poller::removeFd(fd)      // ② epoll_ctl(DEL) + ::close(fd)
```

- `Poller::removeFd` 既管 epoll 又管 close，职责越界
- Connection 析构时不自动释放 socket，依赖外部手动 close
- `erase` 和 `removeFd` 顺序反直觉

同时，当前 `Ctrl+C` 会直接 kill 进程，没有任何清理流程。

### 分析 🔍

**问题一：Poller 职责不清**

`Poller` 作为 epoll 封装层，只应负责事件注册/注销/等待。`::close(fd)` 是 socket 资源管理的职责，不应放在 Poller 里。

**问题二：Connection 无 RAII 守护**

Connection 持有 `client_fd_`，但析构时不自动 close。若某个路径忘记手动 close，就会 fd 泄漏。

**问题三：进程暴力退出**

当前 `Ctrl+C` → 内核 SIGINT → 进程直接被 kill。4 个 EventLoop 线程正在处理的请求被中断，未发送数据丢失，打开的 fd 来不及清理。

**核心阻塞链：** 无优雅退出 → 无法安全终止 EventLoop → 无法安全释放资源。

### 方案 🛠️

**决策 1：Connection 资源管理**

| 选项              | 做法                                     | 分析                        |
| :-------------- | :------------------------------------- | :------------------------ |
| ❌ 旧方案           | `::close(fd)` 在 `Poller::removeFd` 里   | 职责混在一起，Poller 越界管了 socket |
| ✅ **新方案（RAII）** | `::close(fd)` 移到 `~Connection()`       | 对象销毁 = 资源释放，RAII 风格       |
| <br />          | `Poller::removeFd` 只做 `epoll_ctl(DEL)` | 职责单一，仅处理 epoll            |

**决策 2：删除顺序**

由于 close 移到析构中，顺序必须调整为：

```
1. Poller::removeFd(fd)    → epoll_ctl(DEL)  从 epoll 移除
2. connections_.erase(fd)  → 析构 → ::close  关 socket
```

若先 erase 会导致 removeFd 去删一个已被 close 的 fd（虽然内核不会崩，但属于无效操作）。

**决策 3：EventLoop 存储方式**

| 方案                                             | 描述           | 分析                               |
| :--------------------------------------------- | :----------- | :------------------------------- |
| ❌ `list<EventLoop>` + 引用                       | 链表存储，引用传递给线程 | 节点分散不友好，但 4 个对象无影响               |
| ✅ **`vector<unique_ptr<EventLoop>>`** **+ 指针** | 堆上分配，指针传递给线程 | 业界主流（libevent/libuv/nginx），灵活性更高 |

选指针方案的原因：

1. 业界主流网络库（libevent、libuv、nginx）均使用堆指针
2. EventLoop 生命周期较长，指针可跨线程传递
3. 指针支持 `nullptr`，可表示"无 EventLoop"的语义
4. `unique_ptr` 自动管理生命周期，无需手动 delete

**决策 4：信号处理方式**

| 方案                 | 分析                                           |
| :----------------- | :------------------------------------------- |
| ❌ 信号处理函数直接调 stop   | 信号处理函数中可做操作有限，不安全                            |
| ✅ **原子标志 + 主线程轮询** | 信号只设 `atomic<bool>`，主线程 sleep 轮询，发现后遍历调 stop |

### 综合改动 📋

| 文件                      | 改动                                                 | 原因          |
| ----------------------- | -------------------------------------------------- | ----------- |
| `Connection.h`          | 添加 `~Connection()` 析构声明                            | RAII 化      |
| `Connection.cpp`        | 实现析构：`::close(client_fd_)`                         | 自动释放 socket |
| `Poller.cpp`            | `removeFd` 中删除 `::close(fd)`                       | 职责单一化       |
| `ConnectionManager.cpp` | 调换顺序：先 `removeFd` 再 `erase`                        | 符合直觉        |
| `EventLoop.h`           | 添加 `stop()` 和 `start()` 的接口设计                      | 为优雅退出准备     |
| `main.cpp`              | 改为 `vector<unique_ptr<EventLoop>>` + 信号处理 + 原子标志轮询 | 进程级优雅退出     |
| `Poller.h`              | 暂不调整                                               | <br />      |

### trade-off ：

**list + 引用 vs vector\<unique\_ptr> + 指针：**

- list + 引用语法更简洁（`.` vs `->`），但整体灵活性不如指针
- 指针方案在所有主流网络库中得到验证，学习成本在未来项目中是正资产
- 仅 4 个 EventLoop，两者性能差异可忽略，选指针主要看中灵活性

**原子标志 + 轮询 vs 信号函数直接操作：**

- 原子标志方案虽然多 100ms 的停止延迟，但信号处理函数中只设一个 bool，安全且可移植
- 信号处理函数中禁止 malloc/cout 等操作，直接调 stop 有潜在风险

### 最终决策 ：

1. **Connection 资源管理采用 RAII 方案** — `close(fd)` 移到析构函数
2. **删除顺序调整为** **`removeFd`** **→** **`erase`** — 先从 epoll 移除再析构 Connection
3. **EventLoop 存储采用** **`vector<unique_ptr<EventLoop>>`** **+ 指针** — 向业界看齐
4. **信号处理采用原子标志 + 主线程轮询** — 安全且简单

<br />

