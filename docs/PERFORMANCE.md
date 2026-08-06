# EPOLL-ET 阶段性能报告

我是怎么把QPS从1w提升到xx w的？本篇博客着重记录epoll-ET阶段的踩坑实录与性能瓶颈分析而非具体优化，其实都是一些这次debug让我有了一个感知：思维要在代码优化和系统数据中切换。

## 最终性能对比快照

### 性能基准

### 最终性能

## 演进/排查/优化过程

### 一、IO模型已更换select→epoll，QPS 不变反降

改前数据现象：
🤗🤗🤗🤗🤗🤗

原先的**错误认知是select→epoll ，QPS就可以翻数倍**，但是这里有一个前提，就是**QPS的瓶颈在于连接数**，如果因为1024限制，导致 select 无法达到更高的QPS，那么从 select 换至 epoll 是有效的，但是数据显示当并发数达到400时，QPS 达到峰值，随后开始下降，显然不符合这个前提。所以当IO模型从 select 换至 epoll 时，QPS并不会上升，依旧到了400并发达到峰值，随后开始下降

那么为什么换了epoll之后 QPS 甚至比select的低？
😏😏😏😏😏😏
改后数据现象：
🫢🫢🫢🫢🫢🫢

所以 QPS 瓶颈不在于 IO模型，同时还存在除100并发以外 P99 全部崩塌的问题，具体原因还需要进一步排查

### 二、从短连接改为长连接，QPS 暴涨 9.3 倍 ⭐

数据现象：

服务日志：
（大量的disclient）

ss 测试，短连接风暴
我做的是长连接场景，wrk默认测试HTTP响应也是长连接测试，然而响应回复被我写成了close，强制走了短连接场景

为什么之前没有意识到：没有思考服务日志为什么会打印那么多disclient，认为业务逻辑没问题，误以为是资源限制，也可以解释为什么select

<br />

### 三、低并发下高吞吐，QPS 随并发数均匀下降

| 并发数    | 总请求    | QPS      | 平均延迟     | P99      | 错误率   | 状态                        |
| ------ | ------ | -------- | -------- | -------- | ----- | ------------------------- |
| 100    | 955307 | 94590.77 | 1.24ms   | 3.04ms   | 0%    | \033\[0;32m✓ 正常\033\[0m   |
| 200    | 913860 | 90488.10 | 2.14ms   | 4.63ms   | 0%    | \033\[0;32m✓ 正常\033\[0m   |
| 400    | 837805 | 83143.97 | 17.65ms  | 633.35ms | 0%    | \033\[0;32m✓ 正常\033\[0m   |
| 600    | 757694 | 75192.19 | 36.55ms  | 1.09s    | 0%    | \033\[1;33m△ 偶发错误\033\[0m |
| 800    | 741159 | 73704.07 | 32.55ms  | 973.59ms | 0%    | \033\[1;33m△ 偶发错误\033\[0m |
| 1000   | 750619 | 74797.57 | 25.84ms  | 628.23ms | 0%    | \033\[1;33m△ 偶发错误\033\[0m |
| 1200   | 739710 | 73338.81 | 24.70ms  | 491.17ms | 0%    | \033\[1;33m△ 偶发错误\033\[0m |
| 1400   | 702326 | 69809.35 | 23.18ms  | 85.88ms  | 0%    | \033\[1;33m△ 偶发错误\033\[0m |
| 1600   | 723427 | 71822.43 | 23.20ms  | 25.47ms  | 0%    | \033\[1;33m△ 偶发错误\033\[0m |
| 1800   | 717783 | 71469.00 | 22.94ms  | 30.24ms  | 0%    | \033\[1;33m△ 偶发错误\033\[0m |
| 2000   | 669632 | 66645.18 | 23.74ms  | 35.24ms  | 0%    | \033\[1;33m△ 偶发错误\033\[0m |
| 2200   | 704220 | 69805.98 | 22.39ms  | 27.98ms  | 0%    | \033\[1;33m△ 偶发错误\033\[0m |
| 2400   | 672738 | 66627.67 | 22.81ms  | 30.67ms  | 0%    | \033\[1;33m△ 偶发错误\033\[0m |
| 2600   | 686418 | 67998.23 | 22.11ms  | 28.74ms  | 0%    | \033\[1;33m△ 偶发错误\033\[0m |
| 2800   | 710446 | 70593.79 | 32.02ms  | 38.42ms  | 0%    | \033\[1;33m△ 偶发错误\033\[0m |
| 3000   | 716470 | 70982.80 | 32.10ms  | 40.95ms  | 0%    | \033\[1;33m△ 偶发错误\033\[0m |
| 4000   | 656262 | 64974.02 | 49.68ms  | 61.35ms  | 0%    | \033\[1;33m△ 偶发错误\033\[0m |
| 5000   | 631244 | 62720.69 | 40.30ms  | 50.43ms  | 0%    | \033\[1;33m△ 偶发错误\033\[0m |
| 7000   | 598970 | 59313.63 | 49.45ms  | 62.85ms  | 0%    | \033\[1;33m△ 偶发错误\033\[0m |
| 10000  | 607404 | 60138.22 | 67.45ms  | 85.87ms  | 0%    | \033\[1;33m△ 偶发错误\033\[0m |
| 15000  | 458643 | 45441.68 | 295.17ms | 1.34s    | .1%   | \033\[1;33m△ 偶发错误\033\[0m |
| 20000  | 219968 | 21773.30 | 490.60ms | 1.89s    | 1.5%  | \033\[1;33m⚠ 过载\033\[0m   |
| 30000  | 5212   | 285.93   | 26.88ms  | 68.23ms  | 27.8% | \033\[0;31m✗ 大量错误\033\[0m |
| 40000  | 7911   | 234.75   | 37.86ms  | 73.22ms  | 61.8% | \033\[0;31m✗ 大量错误\033\[0m |
| 50000  | 15149  | 339.56   | 22.35ms  | 52.46ms  | 61.0% | \033\[0;31m✗ 大量错误\033\[0m |
| 60000  | 32863  | 588.95   | 26.82ms  | 74.72ms  | 49.7% | \033\[0;31m✗ 大量错误\033\[0m |
| 70000  | 24339  | 340.69   | 22.67ms  | 57.66ms  | 63.9% | \033\[0;31m✗ 大量错误\033\[0m |
| 80000  | 21733  | 276.96   | 21.99ms  | 68.63ms  | 70.6% | \033\[0;31m✗ 大量错误\033\[0m |
| 90000  | 17201  | 197.83   | 25.18ms  | 46.14ms  | 78.6% | \033\[0;31m✗ 大量错误\033\[0m |
| 100000 | 31505  | 318.08   | 28.15ms  | 65.73ms  | 69.6% | \033\[0;31m✗ 大量错误\033\[0m |

折线图：

分析：
我原先在考虑开销

下次遇到性能问题，先别打开 IDE，拿出一张纸，画：

text
\[客户端]
↓ (SYN 包)
\[网卡]
↓ (软中断)
\[SYN 队列] ← 大小: net.ipv4.tcp\_max\_syn\_backlog
↓ (三次握手完成)
\[accept 队列] ← 大小: listen(fd, backlog)
↓ (accept() 取出)
\[进程 fd 表] ← 上限: ulimit -n
↓ (epoll\_ctl 添加)
\[epoll 就绪队列]
↓ (epoll\_wait 取出) ← 你的 events\[] 数组大小
\[你的业务逻辑]
然后问自己：600 个并发时，哪个环节会先满？

SYN 队列默认 128？→ 600 瞬间涌入，满了

backlog 默认 128？→ 满了

fd 限制 1024？→ 600 还没满，排除

events\[] 数组 64？→ 一次只能取 64 个，要多轮处理，延迟增加

### 四、多线程扩展，REUSEPORT + 一线程一 EventLoop ⭐

#### 为什么要扩展？

单线程 EventLoop 在 400 并发时 P99 延迟达到 523ms，600 并发即崩溃。单核 CPU 满载，epoll\_wait 事件排队导致尾部延迟失控。虽然 QPS 峰值尚可（90k），但 **P99 尖刺和高并发容量天花板** 才是真正的瓶颈。

#### 怎么扩展的？

采用 **REUSEPORT + 一线程一 EventLoop** 方案，改动仅 1 个文件 \~15 行代码：

1. `SocketListener.cpp`：`setsockopt(SO_REUSEPORT)` 允许多个 socket 绑定同一端口，内核基于四元组哈希自动负载均衡
2. `main.cpp`：`std::thread::hardware_concurrency()` 获取 CPU 核数，创建 N 个线程，每个线程独立运行 EventLoop

关键优势：EventLoop 所有成员均为值类型，天然线程安全，零共享、零锁。

#### 最终结果

| 指标               | 单线程 (05-24) | 多线程 (06-06) | 变化      |
| ---------------- | :---------: | :---------: | :------ |
| **QPS 峰值**       |    90,269   |   115,939   | +28.4%  |
| **零错误上限**        |    400 并发   |  15,000 并发  | +37.5x  |
| **崩溃起始点**        |     400+    |  20,000 并发  | +50x    |
| **P99 @ 400 并发** |    523ms    |   17.60ms   | 降 96.6% |
| **平均延迟 @ 400**   |   15.42ms   |    3.94ms   | 降 74.5% |

|   并发  | 单线程 QPS | 多线程 QPS | 单线程 P99 | 多线程 P99 |
| :---: | :-----: | :-----: | :-----: | :-----: |
|  100  |   90k   |   107k  |  3.84ms | 12.73ms |
|  200  |   73k   |   105k  |  1.41s  |  679ms  |
|  400  |   77k   |   116k  |  523ms  | 17.60ms |
|  600  |  💀 崩溃  |   105k  |    —    | 22.54ms |
|  1000 |    —    |   97k   |    —    |   39ms  |
|  5000 |    —    |   45k   |    —    |  201ms  |
| 15000 |    —    |   62k   |    —    |  924ms  |
| 20000 |    —    |   6.8k  |    —    |  1.98s  |

**本地回环补充对比** — 容器内数据受 Docker bridge 网络层影响，为排除干扰，在宿主机本地回环上补充压测（单线程 3 轮中位数 vs 多线程 4 线程，均关闭日志，wrk -t12 -d10s）：

| 指标                |    单线程（中位数）    | 多线程（4 线程） | 提升           |
| ----------------- | :------------: | :-------: | :----------- |
| **QPS 峰值**        |       71k      |    254k   | 🟢 **+3.6x** |
| **P99 @ 400 并发**  |      315ms     |   8.08ms  | 🟢 **降 97%** |
| **P99 @ 1000 并发** |      311ms     |  16.79ms  | 🟢 **降 95%** |
| **最大稳定并发**        | 200（超 400 即失稳） |   20,000  | 🟢 **100x**  |

|   并发  | 单线程 QPS | 4 线程 QPS |  QPS 提升 | 单线程 P99 | 4 线程 P99 |
| :---: | :-----: | :------: | :-----: | :-----: | :------: |
|  100  |  66,311 |  217,555 | 🟢 3.3x |  2.78ms |  1.77ms  |
|  200  |  60,311 |  241,407 | 🟢 4.0x |  5.87ms |  2.02ms  |
|  400  |  49,139 |  240,185 | 🟢 4.9x |  315ms  |  8.08ms  |
|  600  |  50,273 |  248,294 | 🟢 4.9x |   \~1s  |  6.58ms  |
|  800  |  39,117 |  247,439 | 🟢 6.3x | \~800ms |  8.29ms  |
|  1000 |  34,086 |  242,273 | 🟢 7.1x |  311ms  |  16.79ms |
|  2000 |  32,814 |  246,238 | 🟢 7.5x |   31ms  |   179ms  |
|  3000 |  34,863 |  251,002 | 🟢 7.2x |   42ms  |   625ms  |
|  5000 |  34,717 |  238,852 | 🟢 6.9x |   38ms  |   675ms  |
| 10000 |  35,661 |  191,447 | 🟢 5.4x |   79ms  |   62ms   |
| 15000 |  68,799 |  215,448 | 🟢 3.1x |   46ms  |   84ms   |
| 20000 |  71,197 |  162,169 | 🟢 2.3x |  112ms  |   142ms  |

分析：

- **容器内数据受 Docker bridge 网络层影响**，多线程相对单线程仅 1.5x QPS 提升。本地回环排除 Docker 干扰后真实收益为 **3.6x**，说明多线程扩展本身有效，容器内数据严重低估了实际效果。
- 容器内瓶颈在 Docker 网络栈（docker-proxy + iptables NAT）而非 CPU。本地回环比容器内 QPS 高 2 倍以上（240k vs 115k），P99 降 50%+。
- 本地回环下多线程 100\~2000 并发全线 240k+ QPS，400 并发 P99 从单线程 315ms 降至 8ms（降 97%），**P99 尖刺问题被彻底消灭**。
- 多线程的核心价值在于 **消灭 P99 尖刺** 和 **提升容量天花板**（零错误上限从 400 并发跃升至 20,000）。
- 2000 并发以上多线程 P99 高于单线程，因为 4 线程在 4 核上竞争内核网络栈。宿主机本地回环 400 并发实测 CPU 分解：us 14.6%（用户态业务）、sy 50.8%（系统调用）、si 26.0%（软中断）、id 8.6%（空闲），内核网络栈（sy + si）合计占 76.8%，尾部延迟受上下文切换和软中断竞争影响。这是 epoll 模型的天花板，io\_uring 才是突破方向。
- 后续压测应优先在宿主机本地回环上进行，容器内只做回归验证。

### 五、线程数探索：4/6/8/10/12 线程全对比 ⭐

默认 `thread_count = std::thread::hardware_concurrency()` 返回 12（宿主机核数），但容器限制 4 核。以下在**宿主机本地回环**上对比 4/6/8/10/12 五种线程配置。

#### 完整对比表（wrk -t12 -d10s，关日志）

|   并发   |  4t QPS  |  6t QPS |  8t QPS | 10t QPS | 12t QPS |  4t P99 |  6t P99  |  8t P99  |  10t P99 |  12t P99 |
| :----: | :------: | :-----: | :-----: | :-----: | :-----: | :-----: | :------: | :------: | :------: | :------: |
|   100  |  217,555 | 320,683 | 266,289 | 269,371 | 266,816 |  1.77ms |  3.30ms  |  7.88ms  | 314ms 🔴 | 488ms 🔴 |
|   200  |  241,407 | 325,826 | 270,425 | 310,877 | 391,024 |  2.02ms |  7.28ms  | 390ms 🔴 |  7.54ms  |  4.52ms  |
|   400  |  240,185 | 317,913 | 299,047 | 335,736 | 412,662 |  8.08ms | 461ms 🔴 |  7.96ms  |  8.72ms  |  7.18ms  |
|   600  |  248,294 | 344,803 | 280,225 | 320,769 | 384,055 |  6.58ms |  9.89ms  |  12.27ms | 362ms 🔴 | 533ms 🔴 |
|   800  |  247,439 | 348,250 | 299,919 | 333,119 | 420,607 |  8.29ms |  11.00ms |  12.10ms |  11.46ms |  10.04ms |
|  1000  |  242,273 | 327,538 | 264,347 | 327,239 | 376,835 | 16.79ms | 384ms 🔴 | 628ms 🔴 |  16.45ms | 583ms 🔴 |
|  1500  |  240,000 | 339,374 | 310,333 | 317,988 | 397,952 |   30ms  |  14.21ms |  13.95ms | 331ms 🔴 |  13.50ms |
|  2000  |  246,238 | 338,712 | 284,612 | 327,203 | 379,243 |  179ms  |  18.10ms | 402ms 🔴 |  16.39ms |  16.49ms |
|  3000  |  251,002 | 319,360 | 286,738 | 323,535 | 337,727 |  625ms  |  21.00ms |  28.33ms |  21.87ms | 515ms 🔴 |
|  5000  |  238,852 | 305,904 | 290,168 | 299,447 | 333,265 |  675ms  |  33.56ms |  31.15ms |  35.50ms |  33.24ms |
|  10000 |  191,447 | 266,034 | 243,835 | 263,585 | 310,254 |   62ms  | 424ms 🔴 | 389ms 🔴 |  62.80ms |  62.03ms |
|  15000 |  215,448 | 244,385 | 218,312 | 236,698 | 233,316 |   84ms  |  89.88ms |  87.97ms |  86.02ms | 382ms 🔴 |
|  20000 |  162,169 | 171,066 | 136,018 | 150,265 | 197,532 |  142ms  |   103ms  |   126ms  | 573ms 🔴 |   127ms  |
| <br /> |  <br />  |  <br /> |  <br /> |  <br /> |  <br /> |  <br /> |  <br />  |  <br />  |  <br />  |  <br />  |
| <br /> | **🔴 数** |  **0**  |  **3**  |  **4**  |  **4**  |  **5**  |  <br />  |  <br />  |  <br />  |  <br />  |

#### QPS 分析

| 指标                  |  4t  |  6t  |  8t  |  10t |  12t |
| :------------------ | :--: | :--: | :--: | :--: | :--: |
| 峰值 QPS（100\~2000）   | 251k | 348k | 310k | 335k | 420k |
| 高并发 QPS（5000～20000） | 215k | 244k | 218k | 236k | 233k |
| QPS/thread（中低并发均价）  |  60k |  55k |  36k |  32k |  29k |

- **4 线程 → 6 线程 QPS 暴增 45%**（240k → 348k），这是加线程的红利期
- **6 线程 → 8 线程 QPS 不升反降**（348k → 310k），竞争内核网络栈的开销开始超过多核收益
- **8 线程 → 10/12 线程 QPS 回升**（310k → 335k/420k），但边际收益持续递减
- **QPS/thread 从 60k 一路跌到 29k**，说明线程数增长的收益被内核竞争快速蚕食
- 高并发区（5000+）**所有配置的 QPS 差距缩小到 15% 以内**，瓶颈已完全不在容器内线程数

#### P99 稳定性分析

- **4 线程：0 个尖刺**，全线 P99 < 30ms（2000+ 除外）。**最可预测的配置**
- **6 线程：3 个尖刺**，集中在 400、1000、10000 三个点，其余 < 35ms
- **8 线程：4 个尖刺**，200、1000、2000、10000 四个点
- **10 线程：4 个尖刺**，100、600、1500、20000 四个点（点位漂移说明非固定 bug）
- **12 线程：5 个尖刺**，100、600、1000、3000、15000 五个点

尖刺会随运行轮次位移（6t 两轮尖刺点位完全不同），是**时序共振现象**而非代码缺陷。

#### 综合建议

| 需求                |     推荐    | 理由                |
| :---------------- | :-------: | :---------------- |
| 低延迟优先（P99 < 30ms） |  **4 线程** | 0 尖刺，全线干净         |
| 吞吐 + 时延平衡         |  **6 线程** | QPS 高 45%，仅 3 个尖刺 |
| 极客跑分（420k QPS）    | **12 线程** | 峰值最高，但 P99 不可预测   |
| 生产环境容器（4 核）       |  **4 线程** | 与容器核数匹配，无超额竞争     |

**结论：4 线程最稳、6 线程最平衡、12 线程封顶但代价高。6 核以上在 epoll 模型下收益递减，io\_uring 才是真正的突破方向。**

***

## 六、偶发 timeout 问题排查基线 (2026-06-10)

> **⚠️ 注意**: 以下数据为 **main 分支 2026-06-10 压测结果**，用于与历史基线对比定位回归。
>
> - 环境: **宿主机本地回环**, wrk **12 线程**, 每轮 **10s**
> - 服务: 4 线程 OLPT (REUSEPORT), 端口 8080
> - 目标: 锁定偶发 timeout 出现区间，排查回归根因

### 数据总览

|     并发    |       QPS      |      P99     |     总错误    |  timeout  |    错误率    | 状态                |
| :-------: | :------------: | :----------: | :--------: | :-------: | :-------: | :---------------- |
|    100    |     216,535    |    3.48ms    |      0     |     0     |     0%    | ✅                 |
|    200    |     226,882    |    7.14ms    |      0     |     0     |     0%    | ✅                 |
|    300    |     240,370    |    5.65ms    |      0     |     0     |     0%    | ✅                 |
|    400    |     242,321    |    4.99ms    |      0     |     0     |     0%    | ✅                 |
|    500    |     225,741    |    4.71ms    |      0     |     0     |     0%    | ✅                 |
|    600    |     248,029    |    7.67ms    |      0     |     0     |     0%    | ✅                 |
|    700    |     249,173    |    7.09ms    |      0     |     0     |     0%    | ✅                 |
|    800    |     243,920    |    6.29ms    |      0     |     0     |     0%    | ✅                 |
|    900    |     235,345    |    13.00ms   |      0     |     0     |     0%    | ✅                 |
|    1000   |     245,009    |    8.25ms    |      0     |     0     |     0%    | ✅                 |
|    1200   |     248,689    |    14.44ms   |      0     |     0     |     0%    | ✅                 |
|    1400   |     235,935    |    11.22ms   |      0     |     0     |     0%    | ✅                 |
|  **1600** | **261,524** 🔥 | **139.32ms** |  **1,595** | **1,595** | **0.06%** | 🔴 **首次 timeout** |
|    1800   |     217,349    |    64.41ms   |      0     |     0     |     0%    | ✅ timeout 消失      |
|    2000   |     121,713    |    77.27ms   |      0     |     0     |     0%    | ✅                 |
|    2200   |     236,535    |   359.71ms   |      0     |     0     |     0%    | ✅                 |
|    2400   |     242,914    |   350.75ms   |      0     |     0     |     0%    | ✅                 |
|    3000   |     251,657    |   565.22ms   |      0     |     0     |     0%    | ✅                 |
|  **4000** |   **230,858**  | **847.34ms** |   **947**  |  **947**  | **0.04%** | 🔴 timeout 复现     |
|  **5000** |   **222,755**  | **508.17ms** |  **1,203** | **1,203** | **0.05%** | 🔴                |
|  **7000** |   **218,642**  |  **44.76ms** |  **1,559** | **1,559** | **0.07%** | 🔴                |
| **10000** |   **204,720**  |  **52.29ms** |  **6,626** | **6,626** | **0.33%** | 🔴                |
| **15000** |   **232,973**  |  **80.64ms** |   **875**  |  **823**  | **0.03%** | 🔴                |
| **20000** |   **159,571**  | **137.97ms** |   **300**  |  **299**  | **0.01%** | 🔴                |
| **30000** |   **31,193**   | **199.85ms** |  **1,793** |   **22**  | **0.41%** | 🔴 过载             |
| **40000** |      **0**     |    **N/A**   | **11,767** |   **0**   |  **100%** | 💀 崩溃             |

### 关键发现

1. **零错误区间缩水**: 历史基线 0\~20000 并发零错误 → 现在仅 **100\~1400** 零错误
2. **timeout 偶发性特征**:
   - 1600 并发出现 timeout=1595，但 **QPS 反而最高 (261k)**，排除过载
   - 1800\~3000 全部消失，4000+ 再次出现
   - 提示**竞态条件或时序问题**而非资源瓶颈
3. **错误类型单一**: 绝大多数为 **timeout**，极少量 read 错误，无 write 错误

### 排查方向

根据特征指向，最可能的根因：

- 🔍 RAII close 改动副作用 — Connection 析构调用 ::close 时机变化导致 fd 复用竞态
- 🔍 epoll 事件处理遗漏 — EPOLLIN/EPOLLOUT 路径缺少 EPOLLERR/EPOLLHUP 检查
- 🔍 优雅退出 eventfd 处理路径 — 可能影响事件循环正常时序

***

## 七、map→vector\<unique\_ptr> 重构：数据结构竟成 P99 瓶颈 ⭐

> **日期：** 2026-06-30
> **环境：** 本地回环，4 线程，wrk -t4 -c${conn} -d10s --latency
> **改动：** `ConnectionManager::connections_` 从 `std::map<int, Connection>` 改为 `std::vector<std::unique_ptr<Connection>>`

### 现象

在偶发 timeout 排查过程中，发现 `std::map<int, Connection>` 在高频插入/删除场景下表现不稳定——三轮同环境压测下，同并发 QPS 差距可达 2x+、P99 波动剧烈。而在 muduo/libevent/Redis 等成熟项目中，Connection/Channel 管理要么用固定数组（Redis ae），要么用哈希索引（libevent），只有 muduo 在 Poller 层用 `map`（存裸指针，不负责生命周期）。

### 为什么流量网关场景下 vector 更优

流量网关的 fd 由内核单调递增分配，天然适合数组索引：

```
fd: 0    1    2    3    4    5  ...  100
map:   {0:C0, 1:C1, 10:C10, ...}      ← 红黑树 O(log n) 查找
vec:   [C0, C1, null, null, C10, ...]  ← O(1) 随机访问
                                  ↑ vector 用 nullptr 占位，不浪费
```

| 维度     |        map       |      vector      |    结论    |
| :----- | :--------------: | :--------------: | :------: |
| 查找     |     O(log n)     |     **O(1)**     | ✅ vector |
| 插入/删除  | O(log n) + 节点堆分配 |  **O(1) + 零分配**  | ✅ vector |
| 连续内存   |      ❌ 节点散落      |     **✅ 连续**     | ✅ vector |
| 扩容策略   |      红黑树自动平衡     |  resize(fd+1) 递增 |    持平    |
| 内存稀疏开销 |        N/A       | 8 字节/空位(nullptr) |    可忽略   |

扩容策略采用 `resize(fd+1)`，reallocation 次数 ≈ log₂(最大并发)，百万并发约 20 次，均摊成本接近零。参考业界：

| 实现             | 数据结构                 | 特点                         |
| :------------- | :------------------- | :------------------------- |
| muduo Poller   | `map<int, Channel*>` | Poller 不拥有 Channel，裸指针查映射  |
| libevent epoll | 动态数组 + 哈希索引          | 跨平台需要                      |
| **Redis ae**   | **固定数组**             | 启动时已知最大 fd                 |
| **本方案**        | `vector<unique_ptr>` | fd 单调递增，resize(fd+1) 几乎不浪费 |

<br />

#### Map 版 Round 1

```
--- 并发: 100 ---
Running 10s test @ http://localhost:8080
  4 threads and 100 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency   234.64us  417.81us  15.77ms   97.39%
    Req/Sec    87.08k    12.36k  111.60k    74.50%
  Latency Distribution
     50%  169.00us
     75%  247.00us
     90%  348.00us
     99%    1.45ms
  3465206 requests in 10.04s, 333.77MB read
Requests/sec: 345186.39
Transfer/sec:     33.25MB

--- 并发: 200 ---
Running 10s test @ http://localhost:8080
  4 threads and 200 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency   365.51us  400.06us  18.19ms   97.47%
    Req/Sec    90.96k     9.60k  122.23k    74.75%
  Latency Distribution
     50%  308.00us
     75%  416.00us
     90%  569.00us
     99%    1.03ms
  3619514 requests in 10.05s, 348.64MB read
Requests/sec: 360130.83
Transfer/sec:     34.69MB

--- 并发: 300 ---
Running 10s test @ http://localhost:8080
  4 threads and 300 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency   506.80us  240.03us   6.70ms   74.54%
    Req/Sec    89.96k     9.65k  123.75k    66.50%
  Latency Distribution
     50%  455.00us
     75%  601.00us
     90%  838.00us
     99%    1.26ms
  3580145 requests in 10.08s, 344.84MB read
Requests/sec: 355299.76
Transfer/sec:     34.22MB

--- 并发: 400 ---
Running 10s test @ http://localhost:8080
  4 threads and 400 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency   717.59us  681.12us  27.16ms   96.16%
    Req/Sec    86.61k    12.12k  124.41k    75.00%
  Latency Distribution
     50%  611.00us
     75%  779.00us
     90%    1.10ms
     99%    2.40ms
  3447958 requests in 10.10s, 332.11MB read
Requests/sec: 341458.45
Transfer/sec:     32.89MB

--- 并发: 500 ---
Running 10s test @ http://localhost:8080
  4 threads and 500 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     1.07ms  685.11us  22.63ms   88.07%
    Req/Sec    66.15k     7.37k   99.15k    77.00%
  Latency Distribution
     50%    0.91ms
     75%    1.20ms
     90%    1.79ms
     99%    3.12ms
  2634821 requests in 10.09s, 253.79MB read
Requests/sec: 261143.82
Transfer/sec:     25.15MB

--- 并发: 600 ---
Running 10s test @ http://localhost:8080
  4 threads and 600 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     1.38ms    0.96ms  33.13ms   91.36%
    Req/Sec    60.94k     6.57k   73.81k    73.74%
  Latency Distribution
     50%    1.17ms
     75%    1.50ms
     90%    2.21ms
     99%    4.37ms
  2406407 requests in 10.02s, 231.79MB read
Requests/sec: 240159.61
Transfer/sec:     23.13MB

--- 并发: 700 ---
Running 10s test @ http://localhost:8080
  4 threads and 700 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     1.47ms  622.33us   8.35ms   77.74%
    Req/Sec    63.59k    10.42k   90.13k    66.16%
  Latency Distribution
     50%    1.34ms
     75%    1.66ms
     90%    2.28ms
     99%    3.68ms
  2513239 requests in 10.03s, 242.08MB read
Requests/sec: 250592.60
Transfer/sec:     24.14MB

--- 并发: 800 ---
Running 10s test @ http://localhost:8080
  4 threads and 800 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     1.94ms    1.04ms  28.17ms   89.48%
    Req/Sec    55.68k     5.27k   67.31k    72.22%
  Latency Distribution
     50%    1.72ms
     75%    2.09ms
     90%    2.85ms
     99%    5.58ms
  2206236 requests in 10.09s, 212.51MB read
Requests/sec: 218619.84
Transfer/sec:     21.06MB

--- 并发: 900 ---
Running 10s test @ http://localhost:8080
  4 threads and 900 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     2.12ms  765.87us  18.10ms   80.94%
    Req/Sec    55.28k     3.78k   66.39k    68.43%
  Latency Distribution
     50%    1.99ms
     75%    2.36ms
     90%    2.93ms
     99%    4.88ms
  2190000 requests in 10.08s, 210.94MB read
Requests/sec: 217350.10
Transfer/sec:     20.94MB

--- 并发: 1000 ---
Running 10s test @ http://localhost:8080
  4 threads and 1000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     2.39ms    0.89ms  23.48ms   84.44%
    Req/Sec    54.40k     4.85k   66.84k    69.95%
  Latency Distribution
     50%    2.23ms
     75%    2.61ms
     90%    3.23ms
     99%    5.48ms
  2158489 requests in 10.10s, 207.91MB read
Requests/sec: 213780.57
Transfer/sec:     20.59MB

--- 并发: 1200 ---
Running 10s test @ http://localhost:8080
  4 threads and 1200 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     3.38ms    2.13ms  84.23ms   93.42%
    Req/Sec    47.72k     7.16k   62.01k    69.39%
  Latency Distribution
     50%    3.00ms
     75%    3.63ms
     90%    4.68ms
     99%   10.32ms
  1881705 requests in 10.09s, 181.25MB read
Requests/sec: 186566.59
Transfer/sec:     17.97MB

--- 并发: 1400 ---
Running 10s test @ http://localhost:8080
  4 threads and 1400 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     3.49ms    0.98ms  14.54ms   79.37%
    Req/Sec    51.34k     4.06k   61.47k    68.62%
  Latency Distribution
     50%    3.34ms
     75%    3.86ms
     90%    4.51ms
     99%    7.31ms
  2030608 requests in 10.10s, 195.59MB read
Requests/sec: 201068.69
Transfer/sec:     19.37MB

--- 并发: 1600 ---
Running 10s test @ http://localhost:8080
  4 threads and 1600 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     3.95ms    1.15ms  21.63ms   78.50%
    Req/Sec    51.81k     4.19k   62.42k    64.80%
  Latency Distribution
     50%    3.80ms
     75%    4.44ms
     90%    5.17ms
     99%    8.44ms
  2051643 requests in 10.10s, 197.62MB read
Requests/sec: 203102.44
Transfer/sec:     19.56MB

--- 并发: 1800 ---
Running 10s test @ http://localhost:8080
  4 threads and 1800 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     4.73ms    1.95ms  41.68ms   86.68%
    Req/Sec    49.48k     6.29k   61.61k    67.78%
  Latency Distribution
     50%    4.36ms
     75%    5.21ms
     90%    6.41ms
     99%   13.18ms
  1948503 requests in 10.06s, 187.68MB read
Requests/sec: 193705.58
Transfer/sec:     18.66MB

--- 并发: 2000 ---
Running 10s test @ http://localhost:8080
  4 threads and 2000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     4.99ms    1.39ms  23.27ms   83.99%
    Req/Sec    51.20k     5.02k   60.59k    62.37%
  Latency Distribution
     50%    4.76ms
     75%    5.42ms
     90%    6.24ms
     99%   10.95ms
  2017126 requests in 10.09s, 194.29MB read
Requests/sec: 200001.03
Transfer/sec:     19.26MB

--- 并发: 2200 ---
Running 10s test @ http://localhost:8080
  4 threads and 2200 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     5.52ms    1.63ms  44.41ms   86.08%
    Req/Sec    50.89k     5.01k   64.21k    69.85%
  Latency Distribution
     50%    5.29ms
     75%    6.00ms
     90%    6.84ms
     99%   12.47ms
  2010042 requests in 10.08s, 193.61MB read
Requests/sec: 199333.57
Transfer/sec:     19.20MB

--- 并发: 2400 ---
Running 10s test @ http://localhost:8080
  4 threads and 2400 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     6.17ms    1.71ms  33.97ms   86.24%
    Req/Sec    49.55k     4.76k   60.24k    76.10%
  Latency Distribution
     50%    5.89ms
     75%    6.66ms
     90%    7.59ms
     99%   13.61ms
  1950013 requests in 10.10s, 187.83MB read
Requests/sec: 193118.97
Transfer/sec:     18.60MB

--- 并发: 3000 ---
Running 10s test @ http://localhost:8080
  4 threads and 3000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     7.67ms    1.99ms  45.43ms   84.11%
    Req/Sec    49.66k     4.78k   60.38k    71.39%
  Latency Distribution
     50%    7.35ms
     75%    8.28ms
     90%    9.50ms
     99%   15.94ms
  1948083 requests in 10.10s, 187.64MB read
Requests/sec: 192911.42
Transfer/sec:     18.58MB

--- 并发: 4000 ---
Running 10s test @ http://localhost:8080
  4 threads and 4000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    10.18ms    3.21ms  70.14ms   83.89%
    Req/Sec    50.02k     5.39k   59.53k    70.24%
  Latency Distribution
     50%    9.84ms
     75%   11.46ms
     90%   13.08ms
     99%   20.86ms
  1952864 requests in 10.10s, 188.10MB read
Requests/sec: 193288.91
Transfer/sec:     18.62MB

--- 并发: 5000 ---
Running 10s test @ http://localhost:8080
  4 threads and 5000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    12.79ms    2.89ms 102.06ms   87.81%
    Req/Sec    49.29k     4.26k   57.59k    68.73%
  Latency Distribution
     50%   12.45ms
     75%   13.75ms
     90%   15.15ms
     99%   23.61ms
  1922414 requests in 10.07s, 185.17MB read
Requests/sec: 190952.57
Transfer/sec:     18.39MB
```

#### Map 版 Round 2

```
--- 并发: 100 ---
Running 10s test @ http://localhost:8080
  4 threads and 400 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     0.91ms  804.96us  24.03ms   92.79%
    Req/Sec    78.14k    13.78k  120.53k    72.61%
  Latency Distribution
     50%  700.00us
     75%    1.06ms
     90%    1.50ms
     99%    3.85ms
  3094738 requests in 10.10s, 298.09MB read
Requests/sec: 306510.73
Transfer/sec:     29.52MB
Running 10s test @ http://localhost:8080
  4 threads and 100 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     1.06ms    0.91ms  18.33ms   78.17%
    Req/Sec    24.57k    20.60k   92.12k    88.00%
  Latency Distribution
     50%    1.06ms
     75%    1.50ms
     90%    1.99ms
     99%    4.43ms
  978037 requests in 10.05s, 94.21MB read
Requests/sec:  97354.17
Transfer/sec:      9.38MB

--- 并发: 200 ---
Running 10s test @ http://localhost:8080
  4 threads and 500 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     1.72ms    0.85ms  37.94ms   76.51%
    Req/Sec    67.27k     9.17k   95.98k    69.44%
  Latency Distribution
     50%    1.55ms
     75%    2.01ms
     90%    2.80ms
     99%    4.64ms
  2651240 requests in 10.05s, 255.37MB read
Requests/sec: 263750.41
Transfer/sec:     25.40MB
Running 10s test @ http://localhost:8080
  4 threads and 200 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     1.65ms    1.22ms  24.81ms   65.39%
    Req/Sec    30.22k    21.90k  107.51k    89.00%
  Latency Distribution
     50%    1.71ms
     75%    2.30ms
     90%    2.93ms
     99%    5.42ms
  1202779 requests in 10.08s, 115.85MB read
Requests/sec: 119278.99
Transfer/sec:     11.49MB

--- 并发: 300 ---
Running 10s test @ http://localhost:8080
  4 threads and 600 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     2.41ms    1.37ms  55.58ms   83.18%
    Req/Sec    58.98k     8.87k   86.40k    70.85%
  Latency Distribution
     50%    2.22ms
     75%    2.73ms
     90%    3.70ms
     99%    6.52ms
  2336945 requests in 10.10s, 225.10MB read
Requests/sec: 231375.58
Transfer/sec:     22.29MB
Running 10s test @ http://localhost:8080
  4 threads and 300 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     2.38ms    1.65ms  39.45ms   68.19%
    Req/Sec    30.88k    17.98k   99.44k    87.37%
  Latency Distribution
     50%    2.43ms
     75%    3.21ms
     90%    4.06ms
     99%    7.20ms
  1217571 requests in 10.02s, 117.28MB read
Requests/sec: 121481.63
Transfer/sec:     11.70MB

--- 并发: 400 ---
Running 10s test @ http://localhost:8080
  4 threads and 700 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     3.03ms    2.75ms 160.29ms   96.31%
    Req/Sec    55.16k    11.70k   95.78k    80.30%
  Latency Distribution
     50%    2.92ms
     75%    3.51ms
     90%    4.47ms
     99%    7.23ms
  2174975 requests in 10.03s, 209.50MB read
Requests/sec: 216856.60
Transfer/sec:     20.89MB
Running 10s test @ http://localhost:8080
  4 threads and 400 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     2.97ms    1.79ms  87.16ms   67.64%
    Req/Sec    32.14k    15.39k   86.77k    88.13%
  Latency Distribution
     50%    3.12ms
     75%    3.89ms
     90%    4.81ms
     99%    7.75ms
  1267341 requests in 10.04s, 122.07MB read
Requests/sec: 126242.80
Transfer/sec:     12.16MB

--- 并发: 500 ---
Running 10s test @ http://localhost:8080
  4 threads and 800 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     3.55ms    1.98ms 129.15ms   79.56%
    Req/Sec    52.87k    10.96k   90.38k    80.05%
  Latency Distribution
     50%    3.52ms
     75%    4.17ms
     90%    5.18ms
     99%    8.27ms
  2085023 requests in 10.06s, 200.83MB read
Requests/sec: 207282.15
Transfer/sec:     19.97MB
Running 10s test @ http://localhost:8080
  4 threads and 500 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     3.78ms    4.11ms 216.33ms   97.86%
    Req/Sec    32.43k    13.13k   76.11k    87.75%
  Latency Distribution
     50%    3.89ms
     75%    4.69ms
     90%    5.65ms
     99%    9.00ms
  1291353 requests in 10.08s, 124.38MB read
Requests/sec: 128172.68
Transfer/sec:     12.35MB

--- 并发: 600 ---
Running 10s test @ http://localhost:8080
  4 threads and 900 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     4.21ms    2.09ms 153.93ms   75.68%
    Req/Sec    50.09k    11.06k   84.81k    76.77%
  Latency Distribution
     50%    4.24ms
     75%    4.98ms
     90%    6.03ms
     99%    9.98ms
  1974685 requests in 10.06s, 190.20MB read
Requests/sec: 196234.15
Transfer/sec:     18.90MB
Running 10s test @ http://localhost:8080
  4 threads and 600 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     4.29ms    2.43ms  65.14ms   65.58%
    Req/Sec    33.18k    15.52k   89.13k    87.63%
  Latency Distribution
     50%    4.64ms
     75%    5.51ms
     90%    6.78ms
     99%   11.34ms
  1309449 requests in 10.05s, 126.13MB read
Requests/sec: 130351.37
Transfer/sec:     12.56MB

--- 并发: 700 ---
Running 10s test @ http://localhost:8080
  4 threads and 1000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     4.97ms    3.46ms 239.86ms   88.78%
    Req/Sec    47.57k    11.21k   88.14k    78.79%
  Latency Distribution
     50%    4.93ms
     75%    5.78ms
     90%    7.11ms
     99%   12.30ms
  1876582 requests in 10.08s, 180.75MB read
Requests/sec: 186143.97
Transfer/sec:     17.93MB
Running 10s test @ http://localhost:8080
  4 threads and 700 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     5.12ms    3.21ms 209.58ms   71.21%
    Req/Sec    32.39k    13.57k   81.79k    86.87%
  Latency Distribution
     50%    5.63ms
     75%    6.59ms
     90%    7.73ms
     99%   12.32ms
  1278073 requests in 10.03s, 123.11MB read
Requests/sec: 127450.91
Transfer/sec:     12.28MB

--- 并发: 800 ---
Running 10s test @ http://localhost:8080
  4 threads and 1200 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     7.03ms   17.58ms 652.88ms   99.38%
    Req/Sec    47.17k     9.94k   79.16k    76.84%
  Latency Distribution
     50%    6.08ms
     75%    6.95ms
     90%    8.28ms
     99%   15.36ms
  1847418 requests in 10.06s, 177.95MB read
Requests/sec: 183695.73
Transfer/sec:     17.69MB
Running 10s test @ http://localhost:8080
  4 threads and 800 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     6.13ms    8.42ms 432.14ms   98.95%
    Req/Sec    32.30k    14.54k   82.71k    86.36%
  Latency Distribution
     50%    6.56ms
     75%    7.59ms
     90%    8.88ms
     99%   14.72ms
  1275245 requests in 10.07s, 122.83MB read
Requests/sec: 126607.71
Transfer/sec:     12.19MB

--- 并发: 900 ---
Running 10s test @ http://localhost:8080
  4 threads and 1400 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     7.07ms    7.33ms 418.84ms   98.16%
    Req/Sec    47.76k     9.69k   82.73k    80.36%
  Latency Distribution
     50%    7.02ms
     75%    7.98ms
     90%    9.41ms
     99%   15.78ms
  1866321 requests in 10.02s, 179.77MB read
Requests/sec: 186226.41
Transfer/sec:     17.94MB
Running 10s test @ http://localhost:8080
  4 threads and 900 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     7.78ms   18.33ms 716.36ms   99.57%
    Req/Sec    30.98k    15.22k   84.78k    87.88%
  Latency Distribution
     50%    7.80ms
     75%    9.11ms
     90%   10.59ms
     99%   16.75ms
  1222826 requests in 10.08s, 117.78MB read
Requests/sec: 121342.66
Transfer/sec:     11.69MB

--- 并发: 1000 ---
Running 10s test @ http://localhost:8080
  4 threads and 1600 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     9.33ms   19.74ms 909.29ms   99.43%
    Req/Sec    44.42k     9.33k   74.60k    80.36%
  Latency Distribution
     50%    8.60ms
     75%    9.89ms
     90%   11.51ms
     99%   21.34ms
  1737345 requests in 10.04s, 167.34MB read
Requests/sec: 173114.01
Transfer/sec:     16.67MB
Running 10s test @ http://localhost:8080
  4 threads and 1000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     9.19ms   26.36ms 987.29ms   99.32%
    Req/Sec    30.63k    14.67k   77.20k    85.35%
  Latency Distribution
     50%    8.81ms
     75%   10.02ms
     90%   11.64ms
     99%   23.75ms
  1210393 requests in 10.09s, 116.59MB read
Requests/sec: 119985.27
Transfer/sec:     11.56MB

--- 并发: 1200 ---
Running 10s test @ http://localhost:8080
  4 threads and 1800 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    13.34ms   44.38ms   1.10s    98.90%
    Req/Sec    43.81k     9.11k   72.15k    73.21%
  Latency Distribution
     50%    9.41ms
     75%   11.03ms
     90%   13.77ms
     99%   98.70ms
  1713663 requests in 10.04s, 165.06MB read
Requests/sec: 170666.28
Transfer/sec:     16.44MB
Running 10s test @ http://localhost:8080
  4 threads and 1200 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     9.39ms   12.28ms 599.66ms   99.02%
    Req/Sec    31.42k    14.06k   79.39k    86.77%
  Latency Distribution
     50%   10.08ms
     75%   11.84ms
     90%   13.68ms
     99%   21.58ms
  1231711 requests in 10.10s, 118.64MB read
Requests/sec: 121948.63
Transfer/sec:     11.75MB

--- 并发: 1400 ---
Running 10s test @ http://localhost:8080
  4 threads and 2000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    21.67ms   88.78ms   1.65s    98.02%
    Req/Sec    42.37k     9.90k   75.25k    73.72%
  Latency Distribution
     50%   10.92ms
     75%   12.84ms
     90%   15.76ms
     99%  500.26ms
  1658213 requests in 10.06s, 159.72MB read
Requests/sec: 164830.80
Transfer/sec:     15.88MB
Running 10s test @ http://localhost:8080
  4 threads and 1400 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    11.10ms   19.68ms 966.61ms   99.54%
    Req/Sec    31.73k    13.48k   76.39k    86.73%
  Latency Distribution
     50%   11.43ms
     75%   13.47ms
     90%   16.09ms
     99%   23.58ms
  1242524 requests in 10.03s, 119.68MB read
Requests/sec: 123862.88
Transfer/sec:     11.93MB

--- 并发: 1600 ---
Running 10s test @ http://localhost:8080
  4 threads and 2200 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    15.63ms   54.68ms   1.84s    99.44%
    Req/Sec    41.25k     8.36k   62.10k    71.68%
  Latency Distribution
     50%   12.35ms
     75%   14.91ms
     90%   17.38ms
     99%   30.43ms
  1616242 requests in 10.09s, 155.68MB read
Requests/sec: 160249.99
Transfer/sec:     15.44MB
Running 10s test @ http://localhost:8080
  4 threads and 1600 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    23.09ms  101.38ms   1.94s    98.27%
    Req/Sec    32.00k    12.54k   71.40k    86.11%
  Latency Distribution
     50%   12.96ms
     75%   14.88ms
     90%   17.35ms
     99%  523.70ms
  1266569 requests in 10.10s, 122.00MB read
Requests/sec: 125399.22
Transfer/sec:     12.08MB

--- 并发: 1800 ---
Running 10s test @ http://localhost:8080
  4 threads and 2400 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    14.87ms   29.83ms   1.39s    99.68%
    Req/Sec    41.09k     8.37k   68.62k    77.04%
  Latency Distribution
     50%   13.76ms
     75%   16.43ms
     90%   18.53ms
     99%   31.57ms
  1609771 requests in 10.08s, 155.05MB read
Requests/sec: 159726.17
Transfer/sec:     15.39MB
Running 10s test @ http://localhost:8080
  4 threads and 1800 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    20.17ms   76.13ms   1.99s    99.15%
    Req/Sec    29.44k    12.87k   71.67k    86.36%
  Latency Distribution
     50%   16.38ms
     75%   18.56ms
     90%   21.33ms
     99%   45.29ms
  1164599 requests in 10.10s, 112.18MB read
  Socket errors: connect 0, read 0, write 0, timeout 8
Requests/sec: 115335.91
Transfer/sec:     11.11MB

--- 并发: 2000 ---
Running 10s test @ http://localhost:8080
  4 threads and 3000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    22.46ms   78.44ms   1.99s    99.26%
    Req/Sec    41.11k     8.40k   69.33k    72.96%
  Latency Distribution
     50%   17.41ms
     75%   19.98ms
     90%   23.14ms
     99%   42.72ms
  1611968 requests in 10.07s, 155.27MB read
  Socket errors: connect 0, read 0, write 0, timeout 61
Requests/sec: 160028.99
Transfer/sec:     15.41MB
Running 10s test @ http://localhost:8080
  4 threads and 2000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    27.87ms  101.52ms   1.99s    98.40%
    Req/Sec    27.21k    10.55k   61.52k    86.73%
  Latency Distribution
     50%   19.51ms
     75%   22.22ms
     90%   24.63ms
     99%  500.33ms
  1068106 requests in 10.03s, 102.88MB read
  Socket errors: connect 0, read 0, write 0, timeout 26
Requests/sec: 106501.68
Transfer/sec:     10.26MB

--- 并发: 2200 ---
Running 10s test @ http://localhost:8080
  4 threads and 4000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    27.69ms   82.50ms   2.00s    99.08%
    Req/Sec    42.01k     8.33k   69.31k    75.77%
  Latency Distribution
     50%   21.61ms
     75%   24.31ms
     90%   28.08ms
     99%   89.78ms
  1636372 requests in 10.04s, 157.62MB read
  Socket errors: connect 0, read 0, write 0, timeout 364
Requests/sec: 162951.52
Transfer/sec:     15.70MB
Running 10s test @ http://localhost:8080
  4 threads and 2200 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    19.04ms   13.10ms 761.64ms   70.37%
    Req/Sec    27.31k    12.31k   64.69k    80.15%
  Latency Distribution
     50%   20.55ms
     75%   24.50ms
     90%   29.47ms
     99%   52.82ms
  1064992 requests in 10.05s, 102.58MB read
Requests/sec: 105953.71
Transfer/sec:     10.21MB

--- 并发: 2400 ---
Running 10s test @ http://localhost:8080
  4 threads and 5000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    30.26ms   82.30ms   2.00s    99.17%
    Req/Sec    39.98k     9.43k   65.87k    71.99%
  Latency Distribution
     50%   22.16ms
     75%   27.94ms
     90%   38.98ms
     99%   80.95ms
  1538110 requests in 10.07s, 148.15MB read
  Socket errors: connect 0, read 6, write 0, timeout 1126
Requests/sec: 152728.64
Transfer/sec:     14.71MB
Running 10s test @ http://localhost:8080
  4 threads and 2400 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    28.04ms  142.61ms   1.81s    97.67%
    Req/Sec    45.12k    16.46k   69.76k    65.12%
  Latency Distribution
     50%    5.53ms
     75%    7.66ms
     90%   22.92ms
     99%  909.54ms
  1773513 requests in 10.10s, 170.83MB read
Requests/sec: 175659.21
Transfer/sec:     16.92MB

--- 并发: 3000 ---
Running 10s test @ http://localhost:8080
  4 threads and 3000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    11.76ms    7.28ms 166.59ms   85.23%
    Req/Sec    42.12k     9.03k   59.44k    67.46%
  Latency Distribution
     50%   10.05ms
     75%   14.92ms
     90%   20.07ms
     99%   37.30ms
  1639266 requests in 10.07s, 157.90MB read
Requests/sec: 162849.69
Transfer/sec:     15.69MB

--- 并发: 4000 ---
Running 10s test @ http://localhost:8080
  4 threads and 4000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    13.76ms   14.94ms 591.50ms   96.77%
    Req/Sec    49.75k     9.97k   66.57k    72.27%
  Latency Distribution
     50%   12.14ms
     75%   16.42ms
     90%   20.06ms
     99%   45.24ms
  1931604 requests in 10.11s, 186.05MB read
Requests/sec: 191151.29
Transfer/sec:     18.41MB

--- 并发: 5000 ---
Running 10s test @ http://localhost:8080
  4 threads and 5000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    24.60ms   76.61ms   1.73s    98.17%
    Req/Sec    51.04k     4.82k   62.39k    73.40%
  Latency Distribution
     50%   15.81ms
     75%   20.48ms
     90%   23.30ms
     99%  388.26ms
  1984667 requests in 10.09s, 191.17MB read
Requests/sec: 196641.73
Transfer/sec:     18.94MB
```

#### Map 版 Round 3

```
--- 并发: 100 ---
Running 10s test @ http://localhost:8080
  4 threads and 1800 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     4.95ms    2.95ms  44.57ms   88.31%
    Req/Sec    51.40k     9.89k   72.84k    67.27%
  Latency Distribution
     50%    4.02ms
     75%    5.12ms
     90%    8.30ms
     99%   17.08ms
  2019427 requests in 10.08s, 194.51MB read
Requests/sec: 200412.79
Transfer/sec:     19.30MB
Running 10s test @ http://localhost:8080
  4 threads and 100 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     2.73ms    3.99ms  54.38ms   88.83%
    Req/Sec    15.71k    22.44k   97.85k    88.94%
  Latency Distribution
     50%    1.18ms
     75%    3.76ms
     90%    7.20ms
     99%   18.78ms
  624529 requests in 10.09s, 60.16MB read
Requests/sec:  61889.70
Transfer/sec:      5.96MB

--- 并发: 200 ---
Running 10s test @ http://localhost:8080
  4 threads and 2000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    10.21ms    7.82ms 263.54ms   88.08%
    Req/Sec    38.97k    12.24k   63.38k    62.76%
  Latency Distribution
     50%    8.27ms
     75%   12.64ms
     90%   17.95ms
     99%   36.68ms
  1520936 requests in 10.10s, 146.50MB read
Requests/sec: 150521.60
Transfer/sec:     14.50MB
Running 10s test @ http://localhost:8080
  4 threads and 200 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     4.54ms    5.90ms 196.52ms   91.52%
    Req/Sec    13.33k     7.62k   44.28k    75.00%
  Latency Distribution
     50%    3.38ms
     75%    5.45ms
     90%    9.55ms
     99%   23.14ms
  528147 requests in 10.08s, 50.87MB read
Requests/sec:  52372.65
Transfer/sec:      5.04MB

--- 并发: 300 ---
Running 10s test @ http://localhost:8080
  4 threads and 2200 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     9.23ms    5.92ms 103.55ms   83.90%
    Req/Sec    48.33k    15.49k   76.73k    63.19%
  Latency Distribution
     50%    7.70ms
     75%   10.34ms
     90%   16.19ms
     99%   32.82ms
  1870069 requests in 10.05s, 180.13MB read
Requests/sec: 186079.35
Transfer/sec:     17.92MB
Running 10s test @ http://localhost:8080
  4 threads and 300 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     4.40ms    3.90ms  44.32ms   74.63%
    Req/Sec    17.78k    16.19k   77.92k    88.38%
  Latency Distribution
     50%    4.50ms
     75%    6.39ms
     90%    8.40ms
     99%   18.09ms
  702243 requests in 10.04s, 67.64MB read
Requests/sec:  69916.28
Transfer/sec:      6.73MB

--- 并发: 400 ---
Running 10s test @ http://localhost:8080
  4 threads and 2400 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     9.83ms   15.88ms 605.60ms   98.60%
    Req/Sec    55.97k    12.02k   77.15k    71.65%
  Latency Distribution
     50%    8.34ms
     75%   10.82ms
     90%   14.02ms
     99%   30.67ms
  2180696 requests in 10.06s, 210.05MB read
Requests/sec: 216743.69
Transfer/sec:     20.88MB
Running 10s test @ http://localhost:8080
  4 threads and 400 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     4.76ms    6.01ms 307.06ms   96.54%
    Req/Sec    21.07k    18.87k   83.04k    86.87%
  Latency Distribution
     50%    5.27ms
     75%    7.19ms
     90%    8.85ms
     99%   12.89ms
  831653 requests in 10.05s, 80.11MB read
Requests/sec:  82723.96
Transfer/sec:      7.97MB

--- 并发: 500 ---
Running 10s test @ http://localhost:8080
  4 threads and 3000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    12.83ms   31.77ms   1.06s    99.32%
    Req/Sec    58.76k     7.62k   73.96k    71.65%
  Latency Distribution
     50%   10.55ms
     75%   12.87ms
     90%   15.45ms
     99%   25.53ms
  2293108 requests in 10.04s, 220.87MB read
Requests/sec: 228463.20
Transfer/sec:     22.01MB
Running 10s test @ http://localhost:8080
  4 threads and 500 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     7.16ms   15.11ms 421.33ms   98.45%
    Req/Sec    19.84k    16.80k   75.97k    87.88%
  Latency Distribution
     50%    6.76ms
     75%    9.12ms
     90%   12.14ms
     99%   27.23ms
  784301 requests in 10.06s, 75.54MB read
Requests/sec:  77937.58
Transfer/sec:      7.51MB

--- 并发: 600 ---
Running 10s test @ http://localhost:8080
  4 threads and 4000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    32.51ms  119.01ms   1.96s    97.23%
    Req/Sec    50.89k     9.96k   68.79k    71.35%
  Latency Distribution
     50%   14.49ms
     75%   17.57ms
     90%   23.38ms
     99%  728.36ms
  1984506 requests in 10.07s, 191.15MB read
Requests/sec: 197147.91
Transfer/sec:     18.99MB
Running 10s test @ http://localhost:8080
  4 threads and 600 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     7.86ms    6.95ms 173.32ms   77.49%
    Req/Sec    19.52k    17.26k   83.78k    85.97%
  Latency Distribution
     50%    8.08ms
     75%   11.48ms
     90%   15.42ms
     99%   31.75ms
  764373 requests in 10.06s, 73.63MB read
Requests/sec:  75976.55
Transfer/sec:      7.32MB

--- 并发: 700 ---
Running 10s test @ http://localhost:8080
  4 threads and 5000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    31.46ms   95.20ms   1.99s    98.42%
    Req/Sec    46.40k     9.84k   63.29k    68.88%
  Latency Distribution
     50%   19.85ms
     75%   23.76ms
     90%   32.08ms
     99%  475.34ms
  1797053 requests in 10.10s, 173.09MB read
  Socket errors: connect 0, read 0, write 0, timeout 3
Requests/sec: 177915.82
Transfer/sec:     17.14MB
Running 10s test @ http://localhost:8080
  4 threads and 700 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     2.27ms    6.05ms 414.12ms   96.25%
    Req/Sec    58.62k    15.64k   89.46k    75.57%
  Latency Distribution
     50%    1.35ms
     75%    1.93ms
     90%    4.07ms
     99%   13.44ms
  2297951 requests in 10.08s, 221.34MB read
Requests/sec: 227913.49
Transfer/sec:     21.95MB

--- 并发: 800 ---
Running 10s test @ http://localhost:8080
  4 threads and 800 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     2.67ms    2.87ms  42.11ms   89.76%
    Req/Sec    53.95k    15.25k  101.81k    67.93%
  Latency Distribution
     50%    1.64ms
     75%    2.74ms
     90%    5.62ms
     99%   14.94ms
  2139472 requests in 10.08s, 206.08MB read
Requests/sec: 212163.25
Transfer/sec:     20.44MB

--- 并发: 900 ---
Running 10s test @ http://localhost:8080
  4 threads and 900 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     3.05ms    2.99ms  49.56ms   90.64%
    Req/Sec    48.78k    11.60k   79.34k    70.92%
  Latency Distribution
     50%    2.04ms
     75%    3.14ms
     90%    5.80ms
     99%   16.18ms
  1920012 requests in 10.05s, 184.94MB read
Requests/sec: 191095.35
Transfer/sec:     18.41MB

--- 并发: 1000 ---
Running 10s test @ http://localhost:8080
  4 threads and 1000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     2.39ms    0.90ms  16.27ms   83.03%
    Req/Sec    54.84k     4.94k   67.17k    68.43%
  Latency Distribution
     50%    2.18ms
     75%    2.59ms
     90%    3.39ms
     99%    5.84ms
  2177523 requests in 10.10s, 209.74MB read
Requests/sec: 215623.51
Transfer/sec:     20.77MB

--- 并发: 1200 ---
Running 10s test @ http://localhost:8080
  4 threads and 1200 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     2.95ms    0.99ms  13.90ms   84.53%
    Req/Sec    52.78k     4.17k   63.14k    64.80%
  Latency Distribution
     50%    2.73ms
     75%    3.17ms
     90%    3.93ms
     99%    7.03ms
  2077911 requests in 10.04s, 200.15MB read
Requests/sec: 207018.61
Transfer/sec:     19.94MB

--- 并发: 1400 ---
Running 10s test @ http://localhost:8080
  4 threads and 1400 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     3.44ms    1.06ms  19.93ms   82.91%
    Req/Sec    52.31k     4.32k   63.11k    69.13%
  Latency Distribution
     50%    3.25ms
     75%    3.76ms
     90%    4.48ms
     99%    7.63ms
  2065701 requests in 10.07s, 198.97MB read
Requests/sec: 205142.70
Transfer/sec:     19.76MB

--- 并发: 1600 ---
Running 10s test @ http://localhost:8080
  4 threads and 1600 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     4.01ms    1.15ms  16.05ms   85.21%
    Req/Sec    51.13k     4.67k   61.00k    65.31%
  Latency Distribution
     50%    3.78ms
     75%    4.30ms
     90%    5.06ms
     99%    8.97ms
  2025421 requests in 10.08s, 195.09MB read
Requests/sec: 201014.37
Transfer/sec:     19.36MB

--- 并发: 1800 ---
Running 10s test @ http://localhost:8080
  4 threads and 1800 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     4.52ms    1.37ms  21.85ms   87.88%
    Req/Sec    51.34k     4.41k   60.91k    68.30%
  Latency Distribution
     50%    4.25ms
     75%    4.82ms
     90%    5.58ms
     99%   10.58ms
  2017474 requests in 10.03s, 194.33MB read
Requests/sec: 201107.70
Transfer/sec:     19.37MB

--- 并发: 2000 ---
Running 10s test @ http://localhost:8080
  4 threads and 2000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     4.98ms    1.58ms  37.21ms   88.93%
    Req/Sec    51.54k     5.35k   62.78k    71.91%
  Latency Distribution
     50%    4.68ms
     75%    5.30ms
     90%    6.18ms
     99%   11.74ms
  2029622 requests in 10.07s, 195.50MB read
Requests/sec: 201649.06
Transfer/sec:     19.42MB

--- 并发: 2200 ---
Running 10s test @ http://localhost:8080
  4 threads and 2200 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     5.47ms    1.47ms  21.43ms   80.10%
    Req/Sec    51.44k     4.42k   61.53k    64.16%
  Latency Distribution
     50%    5.30ms
     75%    6.05ms
     90%    6.89ms
     99%   11.58ms
  2017264 requests in 10.10s, 194.31MB read
Requests/sec: 199655.53
Transfer/sec:     19.23MB

--- 并发: 2400 ---
Running 10s test @ http://localhost:8080
  4 threads and 2400 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     9.98ms    6.22ms  61.82ms   79.64%
    Req/Sec    37.70k     8.62k   58.75k    66.14%
  Latency Distribution
     50%    7.85ms
     75%   13.12ms
     90%   18.17ms
     99%   31.82ms
  1478284 requests in 10.10s, 142.39MB read
Requests/sec: 146381.45
Transfer/sec:     14.10MB

--- 并发: 3000 ---
Running 10s test @ http://localhost:8080
  4 threads and 3000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    12.58ms    9.60ms 102.62ms   85.79%
    Req/Sec    37.25k    13.37k   61.71k    58.38%
  Latency Distribution
     50%    8.16ms
     75%   15.38ms
     90%   25.75ms
     99%   48.21ms
  1434257 requests in 10.09s, 138.15MB read
Requests/sec: 142087.45
Transfer/sec:     13.69MB

--- 并发: 4000 ---
Running 10s test @ http://localhost:8080
  4 threads and 4000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    10.06ms    2.86ms  58.46ms   76.01%
    Req/Sec    50.55k     5.08k   63.47k    68.88%
  Latency Distribution
     50%    9.77ms
     75%   11.49ms
     90%   13.16ms
     99%   20.59ms
  1977596 requests in 10.05s, 190.48MB read
Requests/sec: 196686.40
Transfer/sec:     18.95MB

--- 并发: 5000 ---
Running 10s test @ http://localhost:8080
  4 threads and 5000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    18.75ms   10.45ms 118.99ms   81.57%
    Req/Sec    39.01k     9.76k   69.28k    69.51%
  Latency Distribution
     50%   15.38ms
     75%   23.13ms
     90%   32.65ms
     99%   55.63ms
  1513131 requests in 10.11s, 145.75MB read
Requests/sec: 149684.46
Transfer/sec:     14.42MB
```

#### Vector 版 Round 1

```
Running 10s test @ http://localhost:8080
  4 threads and 100 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     2.13ms    4.09ms 103.16ms   90.28%
    Req/Sec    24.77k     7.28k   54.43k    71.97%
  Latency Distribution
     50%  325.00us
     75%    2.64ms
     90%    6.10ms
     99%   18.80ms
  985689 requests in 10.09s, 94.94MB read
Requests/sec:  97704.81
Transfer/sec:      9.41MB
Running 10s test @ http://localhost:8080
  4 threads and 200 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     3.69ms    6.56ms 109.39ms   90.69%
    Req/Sec    19.66k     6.75k   44.13k    72.28%
  Latency Distribution
     50%  759.00us
     75%    5.04ms
     90%    9.86ms
     99%   30.72ms
  773118 requests in 10.09s, 74.47MB read
Requests/sec:  76624.03
Transfer/sec:      7.38MB
Running 10s test @ http://localhost:8080
  4 threads and 300 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     4.20ms    5.71ms  70.48ms   86.85%
    Req/Sec    19.11k     6.18k   43.94k    70.88%
  Latency Distribution
     50%    1.43ms
     75%    6.39ms
     90%   11.44ms
     99%   25.84ms
  755443 requests in 10.10s, 72.77MB read
Requests/sec:  74816.34
Transfer/sec:      7.21MB
Running 10s test @ http://localhost:8080
  4 threads and 400 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     5.01ms    5.94ms  54.95ms   85.59%
    Req/Sec    17.05k     5.05k   50.43k    77.81%
  Latency Distribution
     50%    2.53ms
     75%    7.93ms
     90%   12.89ms
     99%   25.99ms
  670760 requests in 10.07s, 64.61MB read
Requests/sec:  66640.19
Transfer/sec:      6.42MB
Running 10s test @ http://localhost:8080
  4 threads and 500 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     1.43ms    1.75ms  49.28ms   95.21%
    Req/Sec    54.03k     8.82k   75.51k    74.24%
  Latency Distribution
     50%    1.13ms
     75%    1.41ms
     90%    2.07ms
     99%    7.65ms
  2133258 requests in 10.06s, 205.48MB read
Requests/sec: 212142.64
Transfer/sec:     20.43MB
Running 10s test @ http://localhost:8080
  4 threads and 600 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     1.59ms    1.17ms  19.29ms   89.20%
    Req/Sec    56.02k     5.46k   71.14k    73.48%
  Latency Distribution
     50%    1.31ms
     75%    1.66ms
     90%    2.63ms
     99%    6.70ms
  2214012 requests in 10.06s, 213.26MB read
Requests/sec: 220056.83
Transfer/sec:     21.20MB
Running 10s test @ http://localhost:8080
  4 threads and 700 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     2.04ms    1.98ms  54.81ms   91.85%
    Req/Sec    54.56k     8.13k   70.19k    72.41%
  Latency Distribution
     50%    1.54ms
     75%    2.03ms
     90%    3.57ms
     99%   10.85ms
  2154154 requests in 10.09s, 207.49MB read
Requests/sec: 213475.97
Transfer/sec:     20.56MB
Running 10s test @ http://localhost:8080
  4 threads and 800 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     2.08ms    1.16ms  22.76ms   84.72%
    Req/Sec    54.08k     6.90k   70.60k    62.12%
  Latency Distribution
     50%    1.83ms
     75%    2.40ms
     90%    3.26ms
     99%    6.62ms
  2141017 requests in 10.07s, 206.23MB read
Requests/sec: 212531.33
Transfer/sec:     20.47MB
Running 10s test @ http://localhost:8080
  4 threads and 900 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     2.28ms    1.50ms  33.36ms   89.33%
    Req/Sec    55.89k     7.20k   73.85k    68.18%
  Latency Distribution
     50%    1.90ms
     75%    2.53ms
     90%    3.65ms
     99%    7.90ms
  2215112 requests in 10.07s, 213.36MB read
Requests/sec: 219948.68
Transfer/sec:     21.19MB
Running 10s test @ http://localhost:8080
  4 threads and 1000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     2.64ms    1.65ms  37.42ms   86.80%
    Req/Sec    54.69k     7.21k   70.53k    65.57%
  Latency Distribution
     50%    2.18ms
     75%    3.10ms
     90%    4.41ms
     99%    8.90ms
  2166816 requests in 10.09s, 208.71MB read
Requests/sec: 214831.85
Transfer/sec:     20.69MB
Running 10s test @ http://localhost:8080
  4 threads and 1200 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     3.19ms  819.74us  10.95ms   75.15%
    Req/Sec    47.67k     4.41k   59.91k    66.58%
  Latency Distribution
     50%    3.06ms
     75%    3.59ms
     90%    4.20ms
     99%    5.89ms
  1883547 requests in 10.04s, 181.43MB read
Requests/sec: 187628.76
Transfer/sec:     18.07MB
Running 10s test @ http://localhost:8080
  4 threads and 1400 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     3.21ms    1.29ms  53.33ms   91.71%
    Req/Sec    55.66k     7.88k   69.90k    78.32%
  Latency Distribution
     50%    3.02ms
     75%    3.50ms
     90%    4.06ms
     99%    7.44ms
  2196169 requests in 10.05s, 211.54MB read
Requests/sec: 218526.28
Transfer/sec:     21.05MB
Running 10s test @ http://localhost:8080
  4 threads and 1600 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     4.38ms    1.83ms  20.36ms   68.55%
    Req/Sec    46.68k     9.61k   70.51k    69.95%
  Latency Distribution
     50%    3.77ms
     75%    5.65ms
     90%    7.19ms
     99%    9.14ms
  1832334 requests in 10.02s, 176.49MB read
Requests/sec: 182824.26
Transfer/sec:     17.61MB
Running 10s test @ http://localhost:8080
  4 threads and 1800 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     4.44ms    1.66ms  51.13ms   81.51%
    Req/Sec    51.04k     7.03k   63.72k    61.44%
  Latency Distribution
     50%    4.10ms
     75%    4.95ms
     90%    6.32ms
     99%    9.24ms
  2016626 requests in 10.10s, 194.24MB read
Requests/sec: 199669.66
Transfer/sec:     19.23MB
Running 10s test @ http://localhost:8080
  4 threads and 2000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     6.07ms    2.42ms  23.28ms   64.46%
    Req/Sec    42.21k     5.38k   54.42k    65.27%
  Latency Distribution
     50%    5.90ms
     75%    7.64ms
     90%    9.60ms
     99%   11.85ms
  1658310 requests in 10.06s, 159.73MB read
Requests/sec: 164818.45
Transfer/sec:     15.88MB
Running 10s test @ http://localhost:8080
  4 threads and 2200 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     5.47ms    2.69ms  59.60ms   91.75%
    Req/Sec    52.31k     9.18k   71.26k    60.16%
  Latency Distribution
     50%    4.91ms
     75%    5.90ms
     90%    7.56ms
     99%   15.17ms
  2048894 requests in 10.07s, 197.35MB read
Requests/sec: 203381.69
Transfer/sec:     19.59MB
Running 10s test @ http://localhost:8080
  4 threads and 2400 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     5.69ms    2.35ms  69.01ms   82.43%
    Req/Sec    53.58k     9.44k   70.33k    61.36%
  Latency Distribution
     50%    5.21ms
     75%    6.41ms
     90%    8.33ms
     99%   13.25ms
  2096680 requests in 10.08s, 201.95MB read
Requests/sec: 207929.17
Transfer/sec:     20.03MB
Running 10s test @ http://localhost:8080
  4 threads and 3000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     9.32ms    3.98ms  27.66ms   60.64%
    Req/Sec    40.97k     6.27k   60.00k    73.71%
  Latency Distribution
     50%    8.65ms
     75%   12.61ms
     90%   15.15ms
     99%   17.62ms
  1597709 requests in 10.09s, 153.89MB read
Requests/sec: 158347.50
Transfer/sec:     15.25MB
Running 10s test @ http://localhost:8080
  4 threads and 4000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    11.79ms    4.36ms  62.76ms   66.39%
    Req/Sec    42.80k     5.11k   58.89k    70.00%
  Latency Distribution
     50%   11.38ms
     75%   14.79ms
     90%   17.39ms
     99%   21.65ms
  1673256 requests in 10.08s, 161.17MB read
Requests/sec: 166017.48
Transfer/sec:     15.99MB
Running 10s test @ http://localhost:8080
  4 threads and 5000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    16.09ms    6.81ms  87.06ms   60.19%
    Req/Sec    39.74k     6.71k   59.24k    63.51%
  Latency Distribution
     50%   15.67ms
     75%   21.48ms
     90%   25.31ms
     99%   30.42ms
  1552513 requests in 10.11s, 149.54MB read
Requests/sec: 153548.14
Transfer/sec:     14.79MB
```

#### Vector 版 Round 2

```
Running 10s test @ http://localhost:8080
  4 threads and 100 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency   443.06us    0.90ms  26.30ms   96.66%
    Req/Sec    43.86k     4.03k   56.21k    79.75%
  Latency Distribution
     50%  285.00us
     75%  421.00us
     90%  632.00us
     99%    3.65ms
  1746073 requests in 10.04s, 168.18MB read
Requests/sec: 173945.52
Transfer/sec:     16.75MB
Running 10s test @ http://localhost:8080
  4 threads and 200 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency   596.83us  506.58us  18.22ms   95.63%
    Req/Sec    47.14k     4.20k   62.49k    75.50%
  Latency Distribution
     50%  521.00us
     75%  647.00us
     90%    0.87ms
     99%    1.72ms
  1876622 requests in 10.05s, 180.76MB read
Requests/sec: 186723.67
Transfer/sec:     17.99MB
Running 10s test @ http://localhost:8080
  4 threads and 300 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency   769.39us  281.07us  12.07ms   83.20%
    Req/Sec    50.80k     4.24k   65.74k    72.50%
  Latency Distribution
     50%  721.00us
     75%    0.86ms
     90%    1.05ms
     99%    1.71ms
  2022206 requests in 10.07s, 194.78MB read
Requests/sec: 200791.45
Transfer/sec:     19.34MB
Running 10s test @ http://localhost:8080
  4 threads and 400 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     0.99ms  320.19us   8.14ms   80.28%
    Req/Sec    52.06k     4.78k   71.03k    73.50%
  Latency Distribution
     50%    0.93ms
     75%    1.10ms
     90%    1.33ms
     99%    2.14ms
  2073355 requests in 10.07s, 199.71MB read
Requests/sec: 205836.33
Transfer/sec:     19.83MB
Running 10s test @ http://localhost:8080
  4 threads and 500 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     1.23ms  383.43us   7.05ms   78.14%
    Req/Sec    51.71k     5.29k   70.32k    67.93%
  Latency Distribution
     50%    1.17ms
     75%    1.39ms
     90%    1.66ms
     99%    2.60ms
  2041168 requests in 10.06s, 196.61MB read
Requests/sec: 202840.37
Transfer/sec:     19.54MB
Running 10s test @ http://localhost:8080
  4 threads and 600 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     1.41ms  570.31us  21.32ms   89.26%
    Req/Sec    54.54k     5.80k   72.62k    67.42%
  Latency Distribution
     50%    1.34ms
     75%    1.58ms
     90%    1.86ms
     99%    3.01ms
  2153651 requests in 10.05s, 207.44MB read
Requests/sec: 214187.77
Transfer/sec:     20.63MB
Running 10s test @ http://localhost:8080
  4 threads and 700 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     1.61ms  430.12us   8.97ms   77.47%
    Req/Sec    55.41k     5.82k   75.51k    69.44%
  Latency Distribution
     50%    1.55ms
     75%    1.79ms
     90%    2.08ms
     99%    3.15ms
  2192024 requests in 10.07s, 211.14MB read
Requests/sec: 217727.95
Transfer/sec:     20.97MB
Running 10s test @ http://localhost:8080
  4 threads and 800 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     1.94ms    1.02ms  43.08ms   93.79%
    Req/Sec    53.55k     6.37k   73.21k    68.69%
  Latency Distribution
     50%    1.80ms
     75%    2.11ms
     90%    2.54ms
     99%    4.84ms
  2121379 requests in 10.07s, 204.33MB read
Requests/sec: 210566.69
Transfer/sec:     20.28MB
Running 10s test @ http://localhost:8080
  4 threads and 900 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     2.10ms    0.90ms  17.38ms   83.36%
    Req/Sec    57.45k     5.69k   71.34k    63.20%
  Latency Distribution
     50%    1.93ms
     75%    2.32ms
     90%    3.01ms
     99%    5.77ms
  2264383 requests in 10.09s, 218.11MB read
Requests/sec: 224330.02
Transfer/sec:     21.61MB
Running 10s test @ http://localhost:8080
  4 threads and 1000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     3.49ms    2.82ms  42.04ms   86.14%
    Req/Sec    46.93k     9.30k   65.55k    67.60%
  Latency Distribution
     50%    2.53ms
     75%    4.35ms
     90%    6.88ms
     99%   14.12ms
  1848076 requests in 10.08s, 178.01MB read
Requests/sec: 183387.79
Transfer/sec:     17.66MB
Running 10s test @ http://localhost:8080
  4 threads and 1200 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     2.95ms    2.01ms  45.48ms   92.80%
    Req/Sec    55.65k    12.41k   71.88k    76.41%
  Latency Distribution
     50%    2.50ms
     75%    2.90ms
     90%    3.75ms
     99%   12.66ms
  2180304 requests in 10.06s, 210.01MB read
Requests/sec: 216675.12
Transfer/sec:     20.87MB
Running 10s test @ http://localhost:8080
  4 threads and 1400 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     4.10ms    2.55ms  36.27ms   83.05%
    Req/Sec    50.29k     8.87k   73.45k    66.41%
  Latency Distribution
     50%    3.18ms
     75%    5.19ms
     90%    7.28ms
     99%   13.49ms
  1979395 requests in 10.09s, 190.66MB read
Requests/sec: 196098.55
Transfer/sec:     18.89MB
Running 10s test @ http://localhost:8080
  4 threads and 1600 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     4.81ms    2.80ms  39.76ms   80.42%
    Req/Sec    48.73k     8.14k   64.35k    63.66%
  Latency Distribution
     50%    3.79ms
     75%    6.24ms
     90%    8.46ms
     99%   14.45ms
  1911420 requests in 10.04s, 184.11MB read
Requests/sec: 190444.31
Transfer/sec:     18.34MB
Running 10s test @ http://localhost:8080
  4 threads and 1800 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     3.86ms    1.32ms  46.09ms   92.17%
    Req/Sec    59.23k     6.89k   73.47k    79.12%
  Latency Distribution
     50%    3.71ms
     75%    4.19ms
     90%    4.72ms
     99%    8.49ms
  2321410 requests in 10.05s, 223.60MB read
Requests/sec: 231014.55
Transfer/sec:     22.25MB
Running 10s test @ http://localhost:8080
  4 threads and 2000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     5.71ms    2.29ms  22.83ms   57.71%
    Req/Sec    44.44k     3.58k   53.45k    58.44%
  Latency Distribution
     50%    5.66ms
     75%    7.64ms
     90%    8.72ms
     99%   10.47ms
  1750079 requests in 10.06s, 168.57MB read
Requests/sec: 174048.58
Transfer/sec:     16.76MB
Running 10s test @ http://localhost:8080
  4 threads and 2200 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     5.37ms    1.70ms  20.86ms   76.77%
    Req/Sec    52.09k     6.65k   70.10k    63.64%
  Latency Distribution
     50%    5.04ms
     75%    6.06ms
     90%    7.57ms
     99%   10.82ms
  2044495 requests in 10.06s, 196.93MB read
Requests/sec: 203260.67
Transfer/sec:     19.58MB
Running 10s test @ http://localhost:8080
  4 threads and 2400 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     6.68ms    2.76ms  24.30ms   63.94%
    Req/Sec    45.90k     8.30k   68.35k    74.02%
  Latency Distribution
     50%    5.80ms
     75%    8.96ms
     90%   10.86ms
     99%   13.05ms
  1809120 requests in 10.10s, 174.26MB read
Requests/sec: 179088.75
Transfer/sec:     17.25MB
Running 10s test @ http://localhost:8080
  4 threads and 3000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     9.18ms    4.16ms  59.63ms   61.73%
    Req/Sec    41.68k     7.70k   70.82k    83.38%
  Latency Distribution
     50%    8.45ms
     75%   12.60ms
     90%   14.94ms
     99%   17.88ms
  1633208 requests in 10.08s, 157.31MB read
Requests/sec: 161954.84
Transfer/sec:     15.60MB
Running 10s test @ http://localhost:8080
  4 threads and 4000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    13.23ms    5.63ms  32.70ms   57.80%
    Req/Sec    38.60k     3.82k   56.61k    82.56%
  Latency Distribution
     50%   13.15ms
     75%   18.06ms
     90%   20.84ms
     99%   23.83ms
  1511716 requests in 10.10s, 145.61MB read
Requests/sec: 149659.66
Transfer/sec:     14.42MB
Running 10s test @ http://localhost:8080
  4 threads and 5000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    15.35ms    6.50ms 116.86ms   77.76%
    Req/Sec    42.71k     8.30k   58.69k    66.03%
  Latency Distribution
     50%   13.50ms
     75%   17.72ms
     90%   24.76ms
     99%   35.94ms
  1655028 requests in 10.07s, 159.41MB read
Requests/sec: 164294.19
Transfer/sec:     15.82MB
```

#### Vector 版 Round 3

```
Running 10s test @ http://localhost:8080
  4 threads and 100 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     1.90ms    2.97ms  54.81ms   86.75%
    Req/Sec    24.00k     6.35k   51.25k    70.45%
  Latency Distribution
     50%  336.00us
     75%    2.70ms
     90%    5.80ms
     99%   12.85ms
  955563 requests in 10.08s, 92.04MB read
Requests/sec:  94815.70
Transfer/sec:      9.13MB
Running 10s test @ http://localhost:8080
  4 threads and 200 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     2.77ms    4.38ms 114.36ms   87.90%
    Req/Sec    22.67k     6.15k   62.00k    74.87%
  Latency Distribution
     50%  637.00us
     75%    4.18ms
     90%    7.91ms
     99%   16.45ms
  895416 requests in 10.04s, 86.25MB read
Requests/sec:  89195.96
Transfer/sec:      8.59MB
Running 10s test @ http://localhost:8080
  4 threads and 300 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     2.58ms    4.48ms  84.81ms   89.32%
    Req/Sec    33.44k    15.21k   65.09k    59.85%
  Latency Distribution
     50%  780.00us
     75%    2.79ms
     90%    7.37ms
     99%   19.91ms
  1314101 requests in 10.08s, 126.58MB read
Requests/sec: 130396.81
Transfer/sec:     12.56MB
Running 10s test @ http://localhost:8080
  4 threads and 400 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     0.97ms  399.30us  15.45ms   89.28%
    Req/Sec    53.65k     4.32k   65.07k    64.75%
  Latency Distribution
     50%    0.91ms
     75%    1.07ms
     90%    1.28ms
     99%    2.15ms
  2135854 requests in 10.08s, 205.73MB read
Requests/sec: 211968.84
Transfer/sec:     20.42MB
Running 10s test @ http://localhost:8080
  4 threads and 500 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     1.22ms  350.29us   7.62ms   80.48%
    Req/Sec    52.35k     4.57k   71.43k    73.80%
  Latency Distribution
     50%    1.17ms
     75%    1.35ms
     90%    1.58ms
     99%    2.54ms
  2071671 requests in 10.10s, 199.55MB read
Requests/sec: 205189.99
Transfer/sec:     19.76MB
Running 10s test @ http://localhost:8080
  4 threads and 600 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     1.40ms  393.54us   9.02ms   79.97%
    Req/Sec    54.46k     4.79k   70.73k    68.94%
  Latency Distribution
     50%    1.35ms
     75%    1.56ms
     90%    1.81ms
     99%    2.86ms
  2151544 requests in 10.03s, 207.24MB read
Requests/sec: 214530.61
Transfer/sec:     20.66MB
Running 10s test @ http://localhost:8080
  4 threads and 700 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     1.61ms  468.31us   9.39ms   78.38%
    Req/Sec    55.35k     6.73k   75.83k    66.41%
  Latency Distribution
     50%    1.54ms
     75%    1.80ms
     90%    2.12ms
     99%    3.27ms
  2188832 requests in 10.07s, 210.83MB read
Requests/sec: 217337.22
Transfer/sec:     20.93MB
Running 10s test @ http://localhost:8080
  4 threads and 800 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     1.85ms  805.85us  23.18ms   91.91%
    Req/Sec    55.79k     5.71k   72.63k    69.70%
  Latency Distribution
     50%    1.76ms
     75%    2.03ms
     90%    2.37ms
     99%    3.97ms
  2208903 requests in 10.07s, 212.76MB read
Requests/sec: 219389.59
Transfer/sec:     21.13MB
Running 10s test @ http://localhost:8080
  4 threads and 900 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     2.03ms  565.45us   8.71ms   73.17%
    Req/Sec    55.44k     5.65k   69.53k    63.38%
  Latency Distribution
     50%    1.97ms
     75%    2.32ms
     90%    2.72ms
     99%    3.84ms
  2198999 requests in 10.07s, 211.81MB read
Requests/sec: 218312.22
Transfer/sec:     21.03MB
Running 10s test @ http://localhost:8080
  4 threads and 1000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     2.34ms    0.89ms  25.37ms   77.87%
    Req/Sec    54.04k     7.92k   76.28k    65.91%
  Latency Distribution
     50%    2.18ms
     75%    2.72ms
     90%    3.40ms
     99%    4.81ms
  2145114 requests in 10.06s, 206.62MB read
Requests/sec: 213278.44
Transfer/sec:     20.54MB
Running 10s test @ http://localhost:8080
  4 threads and 1200 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     3.01ms    1.20ms  23.84ms   77.57%
    Req/Sec    50.96k     7.14k   70.94k    71.17%
  Latency Distribution
     50%    2.83ms
     75%    3.55ms
     90%    4.39ms
     99%    6.33ms
  2013232 requests in 10.10s, 193.92MB read
Requests/sec: 199357.08
Transfer/sec:     19.20MB
Running 10s test @ http://localhost:8080
  4 threads and 1400 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     3.71ms    1.12ms  14.77ms   76.59%
    Req/Sec    47.72k     5.27k   61.05k    63.68%
  Latency Distribution
     50%    3.50ms
     75%    4.17ms
     90%    5.12ms
     99%    7.47ms
  1884973 requests in 10.10s, 181.56MB read
Requests/sec: 186697.55
Transfer/sec:     17.98MB
Running 10s test @ http://localhost:8080
  4 threads and 1600 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     4.70ms    1.97ms  20.08ms   58.24%
    Req/Sec    42.54k     3.17k   51.75k    64.60%
  Latency Distribution
     50%    4.60ms
     75%    6.33ms
     90%    7.34ms
     99%    8.83ms
  1680321 requests in 10.10s, 161.85MB read
Requests/sec: 166326.06
Transfer/sec:     16.02MB
Running 10s test @ http://localhost:8080
  4 threads and 1800 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     3.97ms    1.45ms  47.47ms   87.49%
    Req/Sec    57.66k     6.60k   71.97k    75.13%
  Latency Distribution
     50%    3.81ms
     75%    4.45ms
     90%    5.16ms
     99%    8.17ms
  2274997 requests in 10.10s, 219.13MB read
Requests/sec: 225296.44
Transfer/sec:     21.70MB
Running 10s test @ http://localhost:8080
  4 threads and 2000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     5.66ms    2.25ms  18.34ms   64.20%
    Req/Sec    45.10k     7.53k   75.00k    80.99%
  Latency Distribution
     50%    5.24ms
     75%    7.40ms
     90%    8.78ms
     99%   11.16ms
  1775632 requests in 10.08s, 171.03MB read
Requests/sec: 176147.76
Transfer/sec:     16.97MB
Running 10s test @ http://localhost:8080
  4 threads and 2200 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     6.15ms    2.85ms  50.55ms   67.37%
    Req/Sec    45.21k    10.00k   68.96k    72.51%
  Latency Distribution
     50%    5.22ms
     75%    8.27ms
     90%   10.36ms
     99%   13.06ms
  1773797 requests in 10.10s, 170.85MB read
Requests/sec: 175699.02
Transfer/sec:     16.92MB
Running 10s test @ http://localhost:8080
  4 threads and 2400 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     7.61ms    3.13ms  19.78ms   57.33%
    Req/Sec    40.16k     3.42k   46.76k    63.85%
  Latency Distribution
     50%    7.58ms
     75%   10.24ms
     90%   11.73ms
     99%   14.05ms
  1576656 requests in 10.09s, 151.87MB read
Requests/sec: 156298.64
Transfer/sec:     15.05MB
Running 10s test @ http://localhost:8080
  4 threads and 3000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     9.00ms    4.23ms  47.91ms   78.52%
    Req/Sec    45.40k     7.83k   60.10k    62.37%
  Latency Distribution
     50%    7.57ms
     75%   10.81ms
     90%   14.95ms
     99%   23.13ms
  1786475 requests in 10.09s, 172.08MB read
Requests/sec: 176975.73
Transfer/sec:     17.05MB
Running 10s test @ http://localhost:8080
  4 threads and 4000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    17.63ms    9.20ms  91.34ms   75.95%
    Req/Sec    30.88k     9.47k   57.25k    62.22%
  Latency Distribution
     50%   15.40ms
     75%   22.09ms
     90%   29.72ms
     99%   48.82ms
  1188845 requests in 10.09s, 114.51MB read
Requests/sec: 117802.40
Transfer/sec:     11.35MB
Running 10s test @ http://localhost:8080
  4 threads and 5000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    15.98ms   10.24ms 144.25ms   88.44%
    Req/Sec    41.28k    15.54k   64.15k    61.97%
  Latency Distribution
     50%   12.71ms
     75%   15.47ms
     90%   28.93ms
     99%   57.88ms
  1561427 requests in 10.09s, 150.40MB read
Requests/sec: 154755.49
Transfer/sec:     14.91MB
```

---

## 八、日志系统优化：fwrite 攒批，write syscall 直降 79.7% ⭐

> **日期：** 2026-07-04
> **背景：** 项目初期所有日志使用 `std::cout` / `std::cerr` 直接输出，无日志级别控制、无时间戳、无编译期开关。每次压测日志必然打到终端，与 wrk 结果混杂，且 Release 模式下也无法关闭。

### 为什么要做日志系统？

1. **面试项目需要专业度** — 一套完整的日志系统（级别、时间戳、文件名行号）是 C++ 后端项目的标配
2. **压测场景需要零开销** — 旧的 cout 在 Release 下仍然输出，无法干净压测
3. **性能不能降** — 加了详细日志（时间戳、级别、文件名行号、可变参数格式化），如果性能下降明显，不如不加

### 设计原则

- **编译期宏开关**：`#if LOG_LEVEL >= LOG_LEVEL_INFO` 在预处理阶段决定代码去留，Release 下 `LOG_LEVEL_NONE` → 日志代码完全消失
- **fwrite 攒批，而非 fflush 逐条刷**：利用 stdio 用户态缓冲区（默认 4096 字节），fwrite 只做 memcpy，不满缓冲区不触发 write() syscall
- **vsnprintf 栈上格式化**：一次性格式化，避免 C++ ostringstream 的多次 << 开销和潜在的堆分配
- **单头文件实现**：所有逻辑在 `include/logger.h` 中，编译器全量内联，无额外链接开销

### 优化前：cout/cerr

压测期间（wrk -t12 -c300 -d10s），对服务端进程 attach `strace -c` 统计：

```
% time     seconds  usecs/call     calls    errors  syscall
------ ----------- ----------- --------- --------- ----------------
  ...      ...          ...        2270             write
```

**write() syscall 共 2,270 次**——每次 cout << 都触发一次系统调用。

### 优化后：fwrite 攒批

同一压测条件（wrk -t12 -c300 -d10s），strace -c 统计：

```
% time     seconds  usecs/call     calls    errors  syscall
------ ----------- ----------- --------- --------- ----------------
  ...      ...          ...         460             write
```

**write() syscall 仅 460 次，↓79.7% 🚀**

### wrk 压测对比

完整阶梯压测（wrk -t4 -d10s --latency，宿主机本地回环）：

|   并发  | 新版有日志（Debug） | 旧版有日志（cout） | 新版无日志（Release） | 说明 |
| :---: | :--------------: | :-------------: | :---------------: | :--- |
|  100  |      17,321      |     11,656      |      11,131      | 新版低并发优势明显 |
|  200  |    **44,025**    |     11,319      |      15,720      | cout 锁竞争剧烈 |
|  300  |    **45,890**    |     26,221      |      46,176      | **全场峰值 🏆** |
|  400  |      44,883      |     43,575      |      45,845      | 三者趋近 |
|  500  |      44,533      |     42,601      |      44,858      | 同上 |
|  600  |      45,718      |     41,803      |      43,864      | 新版稳居前列 |
|  700  |      44,949      |     44,849      |      43,988      | 持平 |
| 1000  |      43,897      |     43,998      |      42,835      | 持平 |
| 2000  |      41,722      |     40,593      |      39,912      | 持平 |
| 4000  |      39,396      |     38,962      |      40,192      | 持平 |
| 10000 |      38,872      |     37,436      |      33,379      | 持平 |

### 核心结论

| 指标 | 旧版 cout | 新版 fwrite 攒批 | 变化 |
| :--- | :------: | :------------: | :-- |
| **write() syscall 次数** | 2,270 | 460 | **↓79.7%** 🚀 |
| **QPS 峰值** | 44,025 | 45,890 | **↑4.2%** |
| **低并发 QPS @100** | 11,656 | 17,321 | **↑48.6%** 🚀 |
| **低并发 QPS @200** | 11,319 | 44,025 | **↑288.9%** 🚀🔥 |
| **Release 下日志** | 仍然输出 ❌ | 编译期消失 ✅ | **零运行时开销** |

1. **fwrite 攒批有效性 proven** — write() syscall 砍掉近 8 成，不满 4096 字节的内核态上下文切换被彻底消除
2. **低并发下新版碾压旧版** — cout 的 `mutex` 锁竞争在低并发下反而更突出，新版 fwrite 无锁，200 并发时 QPS 是旧版的 **3.8 倍**
3. **高并发下三者趋近** — 400+ 并发后瓶颈转移到 epoll 事件处理本身，日志开销占比很小
4. **编译期零开销达成** — Release 下有日志 ≈ 无日志（峰值差 < 3%），宏开关确实让日志代码编译期消失
5. **面试价值** — 有日志 ≈ 无日志的性能表现，是编译期零开销 + 用户态缓冲区攒批的最佳实证 💪

