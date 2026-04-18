## 第一阶段echo服务器流程图

```mermaid
graph TD
    Start[echo服务器启动] --> Bind[bind端口8080]
    Bind --> Listen[listen监听]
    Listen --> Loop[开始主循环]
    
    subgraph "阻塞链：一个客户端卡住全部"
        Loop --> Accept[accept等待新连接]
        Accept -->|客户端1连接| Recv[recv等待数据]
        Recv -->|收到数据| Send[send回显]
        Send -->|while循环| Recv
        Recv -->|无数据| Blocked[❌ 永久阻塞]
    end
    
    Accept -.->|客户端2连接| Stuck[❌ accept被卡住<br>连接建立但服务器不知道]
    Blocked --> Result[结果：只能服务一个客户端]
```

## 函数调用流程
Echo服务器调用树
├──1. 构造函数（编译器生成的代码自动调用）
├──2. start()（用户调用）
│   ├── setupSocket()
│   │   ├── socket()                创建套接字
│   │   ├── setsockopt(SO_REUSEADDR) 地址重用
│   │   ├── htons()                 端口字节序转换
│   │   ├── bind()                  绑定地址
│   │   └── listen()                开始监听
│   │
│   ├── acceptClient()
│   │   ├── accept()                接受连接（阻塞）
│   │   ├── inet_ntop()             IP转字符串
│   │   └── ntohs()                 端口转换
│   │
│   └── handleClient()
│       └── while循环
│           ├── recv()              接收数据
│           └── send()              发送数据
│
├
│
└──3. 析构函数（自动）── stop()
                        └── cleanup()
                            ├── close(client_socket)   关闭客户端连接
                            └── close(server_socket)   关闭服务器
