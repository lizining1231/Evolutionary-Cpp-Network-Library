# SELECT 阶段性能报告

## 最终性能呈现
```bash
并发数 | 总请求 | QPS      | 平均延迟 | P99      | 错误率 | 状态
---------|----------|----------|----------|----------|---------|------
100     | 852883   | 85114.74 | 1.46ms   | 3.59ms   | 0%      | \033[0;32m✓ 正常\033[0m
200     | 851277   | 84696.75 | 2.27ms   | 5.11ms   | 0%      | \033[0;32m✓ 正常\033[0m
400     | 793976   | 79012.59 | 5.14ms   | 10.95ms  | 0%      | \033[0;32m✓ 正常\033[0m
600     | 775766   | 76905.12 | 33.92ms  | 1.02s    | 0%      | \033[1;33m△ 偶发错误\033[0m
800     | 741978   | 73722.94 | 31.15ms  | 934.98ms | 0%      | \033[1;33m△ 偶发错误\033[0m
1000    | 736781   | 73173.02 | 27.92ms  | 720.68ms | 0%      | \033[1;33m△ 偶发错误\033[0m
1200    | 387667   | 38438.00 | 23.73ms  | 595.06ms | 1.8%    | \033[1;33m⚠ 过载\033[0m
1400    | 0        | 0        | N/A      | N/A      | 100%    | \033[0;31m✗ 崩溃\033[0m
1600    | 0        | 0        | N/A      | N/A      | 100%    | \033[0;31m✗ 崩溃\033[0m
```

## 演进/排查/优化过程
1. 经大量排查解决QPS暴跌95%问题,100并发下 QPS: 8500 → 470 → 10000
2. 添加Connection类与Buffer类降低P99延迟, 100并发下 P99: 随机1000ms → 14ms
3. 将HTTP响应尾部由短连接设置改为长连接, 100并发下 QPS: 10000 → 8.5w, P99: 14ms → 3.59ms

### 排查报告一：QPS从8500暴跌至400问题分析

#### 排查过程简介
阶段一：排除程序本身问题
阶段二：排除docker-proxy正常损耗
阶段三：CPU资源分析
阶段四：CPU限制配置验证
阶段五：docker-proxy状态检查
阶段六：系统状态清理
阶段七：隔天性能再次暴跌

本排查报告为**AI精炼版本**，手写版含具体过程、详细细节、面经打假见技术博客:
[《连续两次QPS暴跌95%：面经纠错+第一次性能排查连踩七个坑？》](https://mp.weixin.qq.com/s/Kn-uA6J0l83c6KVfErxMQQ)
#### 测试环境说明
**测试方法**：控制变量、预热60秒、三次压测取中位，每次间隔时间（TCP_TIME_WAIT +5）秒

**环境信息**：详见报告末尾“测试环境、方法说明”

#### 数据现象
1. 宿主机本地回环测试（正常）
```test
Requests/sec:   9329.73
12线程、100连接、60秒测试

平均延迟：10.42ms

Socket错误：timeout 190
```
2. 容器内本地回环测试（正常）
```test
Requests/sec:   13233.67
12线程、100连接、30秒测试

平均延迟：7.54ms

Socket错误：timeout 94
```
3. 宿主机压测容器服务（异常）
```test
Requests/sec:    437.56
12线程、100连接、30秒测试

平均延迟：13.25ms，P99：20.28ms

Socket错误：read 7840, timeout 96

性能下降约20-30倍
```

#### 排查过程
##### 阶段一：排除程序本身问题
观察：宿主机本地回环和容器内本地回环测试均正常（QPS > 9000）

结论：程序本身无问题。

##### 阶段二：排除docker-proxy正常损耗
观察：docker-proxy正常情况下的性能损失应为5-10%，当前性能损失约2000%

结论：非普通环境数据损耗。

##### 阶段三：CPU资源分析
3.1 CPU使用率观察
方法：watch -n 1 'docker exec im-gateway-test top -bn1 | head -20'

数据（3秒采样间隔）：
```bash
时间	us%	进程CPU%	进程名
17:55:22	0.0	0.0	TCPserver
17:55:26	1.1	0.0	TCPserver
17:55:30	3.3	0.0	TCPserver
17:55:34	17.3	6.7	TCPserver
17:55:38	16.4	86.7	TCPserver
17:55:41	11.8	68.8	TCPserver
17:55:45	3.4	27.8	TCPserver
17:55:49	4.4	20.0	TCPserver
17:55:50	3.3	33.3	TCPserver
17:55:54	2.2	31.2	TCPserver
17:55:58	4.3	25.0	TCPserver
```
分析：

容器限制4核CPU，服务为单线程架构

系统CPU使用率（us%）始终 ≤ 25%（理论最大值）

进程CPU使用率（%CPU）可达86.7%，符合单线程跑满1核的预期

中间结论：进程未受CPU限制（后续验证此为错误结论）

##### 阶段四：CPU限制配置验证
**4.1 检查配置文件**
.env.test：

```bash
CPU_LIMIT=4
MEMORY_LIMIT=2G
CPU_RESERVATION=2
MEMORY_RESERVATION=1G
```
docker-compose.yml（deploy方式）：
```yaml
deploy:
  resources:
    limits:
      cpus: ${CPU_LIMIT:-0}
      memory: ${MEMORY_LIMIT:-0}
    reservations:
      cpus: ${CPU_RESERVATION:-0}
      memory: ${MEMORY_RESERVATION:-0}
```
发现：deploy.resources配置仅在Swarm模式下生效（docker stack deploy），使用docker compose up -d时被忽略且不报错。

**4.2 修改配置方式**
修改docker-compose.yml：

```yaml
# 资源限制
cpus: 4
mem_limit: 2g
mem_reservation: 1g
```
**4.3 验证CPU限制是否生效**
方法1 - docker inspect：

```bash
docker inspect im-gateway-test | grep -A 30 "HostConfig"
```
结果：HostConfig中无NanoCpus、Memory字段

方法2 - 直接读取cgroup v2：

```bash
docker exec im-gateway-test cat /sys/fs/cgroup/cpu.max
```
输出：400000 100000  → 4核，生效
```bash
docker exec im-gateway-test cat /sys/fs/cgroup/memory.max
```
输出：2147483648 → 2GB，生效
```bash
docker exec im-gateway-test cat /sys/fs/cgroup/memory.current
```
输出：4866048 → 当前占用4.8MB
结论：CPU限制已生效，docker inspect不显示是因cgroup v1/v2 API不兼容。

**4.4 修改配置后压测**
运行.scripts/benchmark.sh：

```test
基准测试 (100并发) QPS: 1596.71
阶梯加压测试 (100并发) QPS: 487.55
```
数据未恢复，排除CPU限制因素。

##### 阶段五：docker-proxy状态检查
**5.1 检查docker-proxy进程**
bash
ps aux | grep docker-proxy
结果：无docker-proxy进程

**5.2 检查端口映射**
```bash
docker port im-gateway-test
```
结果：无输出（容器未运行）

**5.3 重建容器**
```bash
docker compose down --remove-orphans
docker compose up -d --force-recreate
```
验证1 - docker-proxy进程：

```test
root  26562  /usr/bin/docker-proxy -proto tcp -host-ip 0.0.0.0 -host-port 18080 -container-ip 172.18.0.2 -container-port 8080
root  26569  /usr/bin/docker-proxy -proto tcp -host-ip :: -host-port 18080 -container-ip 172.18.0.2 -container-port 8080
```
验证2 - 端口映射：

```test
8080/tcp -> 0.0.0.0:18080
8080/tcp -> [::]:18080
```
##### 阶段六：系统状态清理
在执行压测前，执行了以下操作：

```bash
sudo sysctl -w net.ipv4.tcp_tw_reuse=1
sudo sysctl -w net.ipv4.tcp_fin_timeout=30
sudo systemctl restart docker
docker compose up -d
```
**6.1 重建后压测结果**
```test
基准测试 (100并发) QPS: 10755.12
阶梯加压测试 (100并发) QPS: 10210.94
```
数据恢复至1w+

##### 阶段七：隔天性能再次暴跌
**7.1 数据现象**
```test
基准测试 (100并发) QPS: 1762.03
阶梯加压测试 (100并发) QPS: 410.56
```
**7.2 排查步骤**
验证1 - docker-proxy状态：正常（两个进程运行中）

验证2 - CPU限制：仍生效（cpu.max输出400000 100000）

验证3 - 内核参数：

```bash
sysctl net.ipv4.tcp_tw_reuse
# 输出：net.ipv4.tcp_tw_reuse = 2（默认值）

sysctl net.ipv4.tcp_fin_timeout
# 输出：net.ipv4.tcp_fin_timeout = 60（默认值）
```
内核参数在重启后恢复默认值。

**7.3 重新设置内核参数**
```bash
sudo sysctl -w net.ipv4.tcp_tw_reuse=1
sudo sysctl -w net.ipv4.tcp_fin_timeout=30
```
**7.4 恢复后压测结果与两组对照试验**
```test
基准测试 (100并发) QPS: 11106.67
阶梯加压测试 (100并发) QPS: 10455.17
数据再次恢复至1w+
```
结论：根本原因为内核参数tcp_tw_reuse和tcp_fin_timeout的配置。

**核心分析：tcp_tw_reuse参数的作用机制**
参数定义
tcp_tw_reuse为三态参数（Linux 4.4+）：

0：禁用，不允许重用TIME-WAIT状态的socket

1：全局启用，允许所有新出站连接重用TIME-WAIT socket

2：仅限回环，仅允许回环连接重用（系统默认值）

利特尔定律推导QPS上限
公式：最大可持续QPS = 可用端口数 ÷ TIME-WAIT持续时间

可用端口范围：32768-60999（共28232个）

TIME-WAIT持续时间：60秒

**理论最大QPS = 28232 ÷ 60 ≈ 470**

验证实验 - tcp_tw_reuse=2时：

```test
Requests/sec: 192.09
符合470上限以内的预期（因额外检查环回连接增加开销，实际低于470）
```
验证实验 - tcp_tw_reuse=0时：
```test
Requests/sec: 1570.98
```
验证实验 - tcp_tw_reuse=1时：
```test
Requests/sec: 10795.68
```
**环境对照实验**

固定参数：tcp_tw_reuse=1, tcp_fin_timeout=60

| 测试场景 | QPS | 说明 |
|----------|-----|------|
| 宿主机本地回环 | 8568.11 | 本地回环路径 |
| 容器内本地回环 | 1074.50 | cgroup限流导致时间戳检查失败 |
| 宿主机→容器服务 | 10795.68 | 正常 |

容器内本地回环性能差的原因：

五元组完全一致 + 新连接时间戳 > 旧连接最后时间戳才可复用

cgroup限流可能导致进程暂停，时间戳停止增长

可能出现新连接时间戳 ≤ 旧连接最后时间戳，复用条件失败

#### 错误与修正总结

##### 排查错误

| 错误 | 说明 | 修正方法 |
|------|------|----------|
| 混淆系统CPU和进程CPU使用率 | 将进程%CPU误认为系统us% | 学习top输出各指标含义 |
| 归因错误（docker-proxy） | 同时修改两项配置，误判根因 | 控制变量，每次只改一项 |
| 未验证AI提供参数的默认值 | 以为1和30是默认值 | 修改前先sysctl 参数名查看当前值 |
| 测试数据不严谨 | 采用单次数据而非多次稳定值 | 多次取中位，记录完整报告 |

##### 配置错误

| 错误配置 | 正确配置 |
|----------|----------|
| 使用`deploy.resources`（仅在Swarm模式生效） | 单机使用`cpus`/`mem_limit` |
| 依赖`docker inspect`查看cgroup v2配置 | 直接读取`/sys/fs/cgroup/`下的文件 |

**最终解决方案**
将内核参数持久化：

```bash
# 创建 /etc/sysctl.d/99-local.conf
net.ipv4.tcp_tw_reuse=1
net.ipv4.tcp_fin_timeout=30
```
修改docker-compose.yml为标准资源配置*

#### 补充：测试环境详情

##### 硬件环境

| 组件 | 配置 |
|------|------|
| CPU | Intel Core i7-10750H @ 2.60GHz（6物理/12逻辑） |
| 内存 | 7.6GB（WSL2动态分配） |

##### 软件环境

| 组件 | 版本 |
|------|------|
| 宿主机 | Windows 10/11 + WSL2 |
| Linux发行版 | Ubuntu 24.04.1 LTS |
| Linux内核 | 5.15.x |
| Docker | 29.2.1 |
| 测试工具 | wrk 4.2.0 |

##### 容器配置

| 配置项 | 值 |
|------|------|
| CPU限制 | 4 cores |
| 内存限制 | 2GB |
| 内存预留 | 1GB |
| 网络模式 | bridge |
| 端口映射 | 18080:8080 |

##### 关键内核参数默认值

| 参数 | 默认值 |
|------|--------|
| tcp_tw_reuse | 2 |
| tcp_fin_timeout | 60 |
| ip_local_port_range | 32768-60999 |

### 排查/架构演进报告二：P99延迟崩塌问题分析与重构

#### 问题现象
测试场景：wrk -t12 -c100 -d30s

延迟分布：

```text
50%    8.54ms
75%    9.52ms
90%   10.62ms  ← P90正常
99%  954.74ms  ← P99崩塌
```
连接数递增表现：

100连接：P99抖动剧烈（10ms-1000ms）

200-1800连接：P99稳定在1000ms

QPS：始终稳定在10000+（与P99表现无关）

结论：QPS未受影响，但少量请求耗时异常高 → 调度不均问题

#### 排查过程
##### 阶段一、select阻塞机制分析
**1.1 问题假设**
select的工作机制：

收到新请求时遍历所有FD

筛选出有请求的FD并逐一处理

处理完成后继续阻塞，等待新请求

关键缺陷：在处理请求期间无法识别新到达的请求，被忽略的请求需等待下一轮select唤醒才能被处理。

**1.2 解决方案尝试**
设置select超时1ms：

```cpp
timeval tv;
tv.tv_sec = 0;
tv.tv_usec = 1000;
int activity = select(max_fd+1, &read_fds, NULL, NULL, &tv);
```
结果：未缓解，P99表现与修改前一致

##### 二、系统调用耗时分析
**2.1 strace数据**
```text
% time     seconds  usecs/call     calls    syscall
 32.80   23.421204          37    629488    write
 20.35   14.533408          69    209842    close
 17.83   12.729731          60    209816    sendto
 13.48    9.628342          22    419658    recvfrom
  8.12    5.800347          27    209828    accept
  7.42    5.297787          25    210343    pselect6
```
分析：

close单次69微秒，write调用62.9万次

若close是瓶颈，应每轮都耗时相同，但P99表现不一致

QPS稳定在11000，处理能力未变，但少量请求耗时异常

结论：非系统调用瓶颈，锁定调度不均问题

##### 三、调度不均问题定位
**3.1 问题分析**
调度三要素：

**单次处理时长**

**调度条件**

**调度顺序**

| 策略 | 描述 | 可控性 |
|------|------|--------|
| 每次处理n个活跃连接的所有请求 | n < 总活跃连接数 | 差（无法预知n个连接的总耗时） |
| 每次处理所有活跃连接的m个请求 | m < 单连接总请求数 | 好（限制单连接处理量） |

**选型**：方案二，限制每次处理的请求量

**选型**：方案二，限制每次处理的请求量

**3.2 前置问题：TCP粘包**
要限制请求量，必须先定义“一个请求”的边界。TCP为流式传输，当前代码无请求边界概念。

解决方案：建立缓冲区，使用find()查找边界（HTTP协议使用\r\n\r\n）

**3.3 第一阶段修复：添加recv_buffer**
修改内容：

Socket类添加std::string recv_buffer成员

临时缓冲区改为char temp_buffer[1024]

数据流：temp_buffer → append到recv_buffer → 拆分请求 → erase已处理数据

结果：

100连接P99：从抖动（10-1000ms）稳定至11-12ms

200-1800连接：P99仍稳定在1000ms

QPS：10000+，400连接时达峰值11800

部分成功：低连接数P99修复，高连接数仍有问题

##### 四、性能回退与不稳定现象
**4.1 意外回退**
修改压测脚本（仅添加输出）后，100连接P99从12ms飙升至1000ms，QPS从1w+降至7k+。

三轮数据：
```
轮次	P99	QPS
1	636.76ms	7578.63
2	1.02s	8040.22
3	626.13ms	7474.11
```
回滚测试：回滚脚本修改后P99仍在1000ms → 排除脚本问题

**4.2 环境变量发现**
触发条件：电脑进入省电模式/未插电

验证：

充满电+插充电器：QPS=11163.43, P99=11.96ms ✅

30分钟后再次测试（仍插电）：P99回升至1000ms

结论：电量影响QPS（1w→7k），但不是P99根本原因

##### 五、Buffer类创建
**5.1 问题定位**
当前recv_buffer为TCPServer的全局成员，所有连接共享同一个缓冲区，append()、find()、erase()操作相互干扰。

需要：每个连接独立缓冲区

**5.2 Buffer类实现**
修改内容：

创建Buffer类，封装缓冲区操作

appendData()：数据追加

takeData()：数据拆分（find + erase）

分离Socket类与Buffer类的职责

20轮压测结果（每轮间歇180s）：
```
结果类型	轮次	P99
正常	3轮	~11ms
异常	17轮	~1000ms
```
结论：未修复，但揭示了问题本质——需要每个连接独立缓冲区，而非全局共享

##### 六、Connection类创建
**6.1 核心修改**
创建Connection类，实现fd到私有Buffer的一一映射：

```cpp
// 修改前（全局共享）
recv_buffer.append(data);

// 修改后（每连接独立）
connections[client_fd].recv_buffer.append(data);
```
架构变化：

```text
TCPServer → map<fd, Connection> → Connection → Buffer成员
```
6.2 20轮压测结果
```
轮次	QPS	平均延迟	P99	状态
1	11079.90	8.54ms	11.77ms	✅
2	11855.00	7.99ms	10.56ms	✅
3	11935.55	7.95ms	10.55ms	✅
4	11932.81	7.94ms	10.46ms	✅
5	11951.62	7.93ms	10.38ms	✅
6	11785.63	8.05ms	11.69ms	✅
7	11601.94	8.28ms	14.43ms	✅
8	11735.09	8.07ms	11.02ms	✅
9	11834.53	8.01ms	10.58ms	✅
10	11944.28	7.94ms	10.31ms	✅
11	11615.68	8.16ms	10.77ms	✅
12	10644.17	8.90ms	13.46ms	✅
13	11875.74	7.97ms	11.37ms	✅
14	11930.97	7.94ms	10.45ms	✅
15	11845.91	7.99ms	11.06ms	✅
16	11953.20	7.92ms	10.90ms	✅
17	12026.35	7.88ms	10.57ms	✅
18	11053.29	8.57ms	13.45ms	✅
19	11414.04	8.30ms	10.84ms	✅
20	11099.49	8.55ms	12.97ms	✅
```
结果：20轮测试中P99全部稳定在10-14ms范围内

重构前后对比
**架构对比**

| 特性 | 旧版 | 新版 |
|------|------|------|
| 缓冲区类型 | 局部char buffer[4096] | 每Connection一个Buffer对象 |
| 数据持久化 | ❌ 每次recv后覆盖 | ✅ appendData累积存储 |
| 粘包处理 | ❌ 无法处理 | ✅ takeData按\r\n\r\n分隔 |
| 半包处理 | ❌ 数据不完整即丢失 | ✅ 不完整则等待下次recv |
| 连接管理 | vector<int> client_fds | map<int, Connection> |
| 单一职责 | ❌ handleClientData做所有事 | ✅ Buffer/Connection各司其职 |
| 调度均匀性 | ❌ P99在10-1000ms抖动 | ✅ 20轮P99稳定10-14ms |

类关系
重构后：

```text
TCPServer
  └── map<int, Connection> connections
        └── Connection
              └── Buffer recv_buffer
```
职责分离：

TCPServer：连接管理、事件循环

Connection：单连接状态、缓冲区持有

Buffer：数据累积、粘包/半包处理

#### Buffer选型决策记录
**Buffer选型方案对比**

| 库/方案 | 数据结构 | 零拷贝 | 可扩展 | 复杂度 |
|---------|----------|--------|--------|--------|
| Muduo | vector | 否(memmove) | 是 | 中 |
| Asio | 用户提供 | 是 | 取决于用户 | 高 |
| Netty | 环形+池化 | 部分(视图) | 是 | 高 |
| libevent | 链表 | 是 | 是 | 高 |
| string | 连续内存 | 否 | 否 | 低 |

**最终决策**：先使用string快速验证

**决策理由**

| 考量因素 | 说明 |
|----------|------|
| 验证目标 | 确认粘包→调度不均→P99崩塌的因果链 |
| 当前场景 | HTTP响应<4KB，小数据量 |
| 性能评估 | erase()的O(N)开销在小数据下可接受 |
| 开发成本 | 复杂度最低，快速验证 |

**已知限制（后期优化）**

| 场景 | 问题 |
|------|------|
| 大数据(几MB) | erase() O(N)开销过大 |
| 频繁append | 容量不足时重新分配+拷贝 |
| 高并发 | string非线程安全 |

##### 根因总结
**问题链：**

TCP流式传输 → 无请求边界 → 无法控制单次处理量

全局共享缓冲区 → 多连接数据操作相互干扰

调度不均 → 少量请求等待时间过长

P99崩塌（QPS正常，但长尾延迟高）

**解决方案链：**

引入Buffer类 → 定义请求边界(\r\n\r\n)

每连接独立Buffer → 消除操作干扰

控制单次处理请求量 → 调度均匀

P99稳定至10-14ms

关键代码变更：

```cpp
// 错误：全局共享
class TCPServer {
    std::string recv_buffer;  // 所有连接共用
};

// 正确：每连接独立
class Connection {
    Buffer recv_buffer;  // 私有缓冲区
};
class TCPServer {
    std::map<int, Connection> connections;
};
```