# Dockerfile.dev - 开发环境专用（挂载模式）
FROM ubuntu:24.04

# ===== 修改说明：移除多阶段构建，改为单阶段镜像 =====
# 原因：开发模式下通过挂载本地编译的二进制文件运行，不需要分离编译和运行环境

# ===== 运行时依赖 =====
RUN sed -i 's@archive.ubuntu.com@mirrors.aliyun.com@g' /etc/apt/sources.list && \
    sed -i 's@security.ubuntu.com@mirrors.aliyun.com@g' /etc/apt/sources.list && \
    apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y \
    libssl3 \
    ca-certificates \
    netcat-openbsd \
    valgrind \
    && rm -rf /var/lib/apt/lists/*

# ===== 编译依赖（开发需要） =====
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    libssl-dev \
    gdb \
    && rm -rf /var/lib/apt/lists/*

# ===== 创建用户 =====
RUN groupadd -r appuser && useradd -r -g appuser -s /bin/bash -d /home/appuser -m appuser

# ===== 创建 entrypoint 脚本 =====
RUN mkdir -p /app && \
    echo '#!/bin/bash\n\
# 开发模式：检查挂载的二进制文件是否存在\n\
if [ ! -f /app/bin/TCPserver ]; then\n\
    echo "ERROR: TCPserver not found in /app/bin/"\n\
    echo "Please run: cd build && cmake .. && ninja"\n\
    exit 1\n\
fi\n\
\n\
# 如果是 sleep 或其他系统命令，直接执行\n\
if [ "$1" = "sleep" ] || [ "$1" = "bash" ] || [ "$1" = "sh" ] || [ "$1" = "cat" ] || [ "$1" = "ls" ]; then\n\
    exec "$@"\n\
elif [ "$1" = "valgrind" ]; then\n\
    shift\n\
    exec valgrind --leak-check=full /app/bin/TCPserver "$@"\n\
else\n\
    # 没有参数或参数是数字，才运行 TCPserver\n\
    if [ $# -eq 0 ] || [[ "$1" =~ ^[0-9]+$ ]]; then\n\
        exec /app/bin/TCPserver "$@"\n\
    else\n\
        # 其他未知命令也直接执行\n\
        exec "$@"\n\
    fi\n\
fi' > /app/entrypoint.sh && chmod +x /app/entrypoint.sh

# ===== 切换用户并设置工作目录 =====
USER appuser
WORKDIR /app
EXPOSE 8080

ENTRYPOINT ["/app/entrypoint.sh"]
CMD ["8080"]