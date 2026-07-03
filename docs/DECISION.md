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

### 场景 

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

### 分析 

**问题一：Poller 职责不清**

`Poller` 作为 epoll 封装层，只应负责事件注册/注销/等待。`::close(fd)` 是 socket 资源管理的职责，不应放在 Poller 里。

**问题二：Connection 无 RAII 守护**

Connection 持有 `client_fd_`，但析构时不自动 close。若某个路径忘记手动 close，就会 fd 泄漏。

**问题三：进程暴力退出**

当前 `Ctrl+C` → 内核 SIGINT → 进程直接被 kill。4 个 EventLoop 线程正在处理的请求被中断，未发送数据丢失，打开的 fd 来不及清理。

**核心阻塞链：** 无优雅退出 → 无法安全终止 EventLoop → 无法安全释放资源。

### 方案 

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

### 综合改动 

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

---

## 7月1日：#8 Connection 解除对 Poller 的耦合 — 回调注入方案

### 场景 

当前 `Connection` 以 **public 成员** 持有 `Poller*` 指针：

```cpp
// Connection.h:26
Poller *poller_;  //  public，外部直接赋值
```

在 `EventLoop::start()` 中直接侵入式赋值：

```cpp
// EventLoop.cpp:45
conn->poller_ = &poller_;
```

而 `Connection::send()` 中通过该指针直接操作 epoll API：

```cpp
// Connection.cpp:75, :88
poller_->enableOut(client_fd);   // 注册 EPOLLOUT
poller_->disableOut(client_fd);  // 注销 EPOLLOUT
```

### 问题 

| 问题 | 说明 |
|:----|:------|
|  **破坏封装** | `Poller*` 是 public 的，外部可随意修改 |
|  **耦合过紧** | Connection 直接依赖 Poller 的具体 API，换 IO 模型（ET / io_uring）得改 Connection |
|  **不安全** | 裸指针，无生命周期保证 |
|  **架构异味** | #6 已完成 Poller 内部数据私有化，但 Connection 这头还留着后门 |

### 方案调研 🔍

通过联网搜索业界实践（muduo、libevent、nginx），归纳出四条路线：

**方案 0：最小改动 — 仅封装 public→private**

只把 `Poller *poller_` 从 public 移到 private，加个 setter。

- 评价：治标不治本，Connection 仍然知道 Poller ❌

**方案 1：回调注入（libevent 风格）✅**

Connection 不持 Poller，改持两个 `std::function<void()>`：
- `enable_write_` — "帮我注册 EPOLLOUT"
- `disable_write_` — "帮我注销 EPOLLOUT"

EventLoop accept 时通过 lambda 注入。Connection 只需"喊一嗓子"，不关心底层 IO 模型。

**方案 2：Event 抽象（muduo 风格）**

新增 `WriteEvent` 结构体，封装 fd + 回调 + enable/disable 方法。

- 评价：架构更纯粹，但当前阶段略显超前，属于"为了抽象而抽象"

**方案 3：纯拉模式**

EventLoop 每次循环遍历所有 Connection 检查 `send_buffer_` 是否为空，决定注册/注销 EPOLLOUT。

- 评价：架构最纯粹，但引入 100ms 延迟 + O(n) 遍历开销 ❌

### 决策依据 

| 对比维度 | 方案0 仅封装 | 方案1 回调注入 | 方案2 Event 抽象 | 方案3 纯拉模式 |
|:---------|:----------:|:------------:|:--------------:|:------------:|
| 改动量 | ⭐ 极小 | ⭐⭐ 小 | ⭐⭐⭐ 中 | ⭐⭐ 小 |
| Connection 知道 Poller？ | ❌ 知道 | ✅ 不知道 | ✅ 不知道 | ✅ 不知道 |
| 延迟影响 | 无 | 无 | 无 | +100ms ⚠️ |
| 与现有风格匹配度 | — | ✅ 与 #6 Poller 同款 | ⚠️ 引入新概念 | — |

**关键因素：** 上一个改动（#6 Poller 私有化）已引入 `std::function<EventCallback>` + lambda 模式。方案 1 延续此风格，代码库内保持一致，且改动最小、不新增文件。

### 最终决策 ✅

**采用方案 1：回调注入方案**

具体改动：

| 文件 | 改动 |
|:----|:-----|
| `include/Connection.h` | 删除 `Poller *poller_` public 成员；新增 `setWriteCallbacks()` 和两个 `std::function<void()>` 私有成员 |
| `src/Connection.cpp` | `send()` 中 `poller_->enableOut/disableOut` 替换为 `enable_write_()/disable_write_()` |
| `src/EventLoop.cpp` | accept 新连接时注入 lambda：`conn->setWriteCallbacks([this,fd](){...})` |
| `src/ConnectionManager.cpp` | 移除 `Poller*` 参数传递，简化构造函数 |

### 设计要点 

```
改前：
    Connection → poller_->enableOut(fd)          ← 知道 epoll API

改后：
    Connection → enable_write_()                  ← 只知道"我要写"
                    ↕ std::function
    EventLoop → poller_.enableOut(fd)             ← 真正的操作者

    Connection 与 IO 模型完全解耦 ✅
    后续换 ET / io_uring，改 EventLoop 即可     ✅
```

### 后续拓展 

方案 1 可平滑升级为方案 2：后续引入定时器（#11）或对象池（#21）时，可将 `std::function` 统一包裹为 `Event` 结构体，实现复用。



---

## 7月3日：#10 日志系统 — 编译期零开销同步日志先行，异步日志后续叠加

### 场景

当前项目日志散落在 4 个文件共 15 处，全部使用裸 `std::cout` / `std::cerr`：

| 文件 | 处数 | 用途 |
|:---|:---:|:---|
| src/Connection.cpp | 4 | 断连/错误/warn 日志 |
| src/EventLoop.cpp | 1 | 启动信息 |
| src/SocketListener.cpp | 2 | 启动信息、客户端连接 |
| src/main.cpp | 8 | 启动/异常/线程信息 |

**三个痛点：**

1. **压测无法关** — 只能 `> /dev/null 2>&1` 重定向，函数调用和格式化开销仍在
2. **格式不统一** — `[DEBUG]`、`[ERROR]`、裸字符串混用
3. **无级别过滤** — INFO 和 ERROR 混在一起

### 分析

**需求定义：**
1. 编译期零开销 — Release 构建时日志代码完全不生成，连参数求值都没有
2. 高性能 — 日常开发时日志不能拖累 QPS

**两种日志模型的性能公式推导：**

设 `N` = 每秒日志条数，`T_sc` = 一次 `write()` 系统调用耗时，`T_fmt` = 格式化一条日志的 CPU 时间，`B` = 异步日志攒批大小

```
同步日志前台 CPU/秒 = C_biz + N x T_sc + N x T_fmt

异步日志前台 CPU/秒 = C_biz + N x T_fmt（syscall 卸载到后台线程）

前台线程 CPU 节省 = N x T_sc（批处理因子 ~0.98，近似忽略）

收益 = N x T_sc / (C_biz + N x T_sc + N x T_fmt)
```

**参数取值依据：**

| 参数 | 数值 | 来源 |
|:---|:---:|:---|
| T_sc（write syscall） | ~0.5 us | Linux 性能实战，Intel i9-12900K 实测 ~85ns 最简 syscall，write 含数据拷贝保守取 0.5us |
| T_fmt（格式化） | ~0.2 us | 业界估算，string 拼接 + 时间戳格式化 |
| B（异步攒批） | ~50 条 | 4KB 缓冲区 / 80 字节每日志 |
| 当前日志量 | ~400k logs/s | 200k QPS x 2 条/请求，4 线程均摊 |

**代入当前数据（200k QPS, 4 线程）：**

```
每线程日志量 = 400k / 4 = 100k logs/s
每线程业务 CPU ≈ 0.93 核秒（接近满负载）

收益 = 100k x 0.5us / (0.93 + 100k x 0.5us + 100k x 0.2us)
     = 0.05 / (0.93 + 0.05 + 0.02)
     = 0.05 / 1.0
     = 5%
```

当前异步收益仅 ~5%，低于 10% 的投入产出比门槛。

**异步日志收益 >= 10% 的临界条件：**

```
N x T_sc / (C_biz + N x T_sc + N x T_fmt) >= 0.1

代入 T_sc = 0.5us, T_fmt = 0.2us, C_biz = 1 核秒 = 10^6 us：

N x 0.5 / (10^6 + N x 0.5 + N x 0.2) >= 0.1
N x 0.5 >= 100,000 + N x 0.05 + N x 0.02
N x 0.43 >= 100,000
N >= 232,558 logs/s/线程
```

等价于 **~116k QPS/线程**（每请求 2 条日志）。当前单线程 ~50k QPS，距此还有一倍差距。

### 方案

**二阶段路线图：**

```
         Step 1（现在）               Step 2（未来，QPS 翻倍后）
    ┌─────────────────────┐     ┌──────────────────────┐
    │  编译期宏开关        │     │  异步日志（底层替换）  │
    │  + 高效同步日志      │ ──▶ │                      │
    │                     │     │  双缓冲 + 后台线程     │
    │  Release: 零开销     │     │  前台零阻塞           │
    │  Debug: 比 cout 快   │     │  API 不变，零改动     │
    └─────────────────────┘     └──────────────────────┘
             当前收益 >0%                收益 >10%
         实现量：1 header，~200 行
```

**Step 1 关键设计决策：**

| 决策 | 选项 | 选择 |
|:---|:---|:---:|
| 编译期控制方式 | (a) NDEBUG 全关 (b) 自定义宏 LOG_LEVEL 分级控制 | **方案 b** — Release 构建默认 LOG_LEVEL_NONE，Debug 构建默认 LOG_LEVEL_INFO |
| API 风格 | (a) printf 式 (b) 流式 | **方案 a** — 编译期比流式少生成代码，配合 fmtlib 性能更优 |
| 输出方式 | (a) write() (b) fwrite 攒批 (c) cout | **方案 b** — fwrite 用户态缓冲区，4KB 才一次 syscall，syscall 减少 3 个数量级 |
| 时间戳精度 | (a) 秒级 (b) 毫秒级 (c) 微秒级 | **方案 c** — 面试亮点，clock_gettime 通过 vDSO 仅 ~50ns |

**Step 1 替换清单：**

| 文件 | 替换内容 |
|:---|:---|
| src/Connection.cpp | 4 处 cout/cerr -> LOG_DEBUG / LOG_ERROR |
| src/EventLoop.cpp | 1 处 cout -> LOG_INFO |
| src/SocketListener.cpp | 2 处 cout -> LOG_INFO |
| src/main.cpp | 8 处 cout/cerr -> LOG_INFO / LOG_ERROR |

### 综合对比

**为什么现阶段不上异步日志：**

| 对比维度 | 同步日志（Step 1） | 异步日志（Step 2） |
|:---------|:---------------:|:---------------:|
| 当前收益 | ✅ 编译期零开销（Release） | ❌ 仅 ~5%，不够门槛 |
| 实现复杂度 | ✅ 1 header，~200 行 | ⚠️ 双缓冲 + 后台线程，~500+ 行 |
| 面试含金量 | ✅ 编译期零开销设计 | ✅ 异步日志架构 |
| 可拓展性 | ✅ 底层可被异步替换 | — |

**为什么 cout 一定得换：**

`std::cout << ... << std::endl` 每次调用包含一次 `write()` 系统调用（endl = flush = write），400k logs/s 意味着 400k syscall/s。`fwrite` 攒批后仅需 ~100 次 syscall/s，syscall 开销从 **~20% CPU** 降至 **~0.005% CPU**，这才是当前最大的优化空间。

### 最终决策

**采用二阶段路线图：Step 1 同步日志先行，Step 2 异步日志待单线程 QPS 突破 ~100k 后叠加**

具体改动：

| 文件 | 改动 |
|:---|:---|
| include/Logger.h | 新建，单头文件日志系统，宏开关 + fwrite 攒批 |
| src/Connection.cpp | cout/cerr -> LOG_DEBUG / LOG_ERROR |
| src/EventLoop.cpp | cout -> LOG_INFO |
| src/SocketListener.cpp | cout -> LOG_INFO |
| src/main.cpp | cout/cerr -> LOG_INFO / LOG_ERROR |
| CMakeLists.txt | Release 构建默认 -DLOG_LEVEL=LOG_LEVEL_NONE -DNDEBUG |

### 预期效果

| 指标 | 改前（cout） | 改后 Release（宏全关） | 改后 Debug（同步日志） |
|:---|:---:|:---:|:---:|
| 日志 syscall 次数 | 400k/s | 0 | ~100/s |
| 日志 CPU 开销 | ~20% | 0% | ~0.005% |
| QPS 影响 | 有（20% CPU 被日志吃掉） | 零 | 可忽略 |
| 代码行数 | 15 处散落 | 0（空宏） | 替换为统一 API |
