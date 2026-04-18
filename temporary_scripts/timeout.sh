#!/bin/bash
# timeout_test_final.sh

echo "=== TIME-WAIT 数量测试（最终版）==="
echo ""

cleanup_port() {
    echo "清理端口 8888..."
    # 杀掉所有占用8888的进程
    sudo fuser -k 8888/tcp 2>/dev/null
    # 等待TIME-WAIT自然消失
    sleep 65
    # 确认端口空闲
    while ss -tan sport = :8888 | grep -q 8888; do
        echo "等待端口释放..."
        sleep 5
    done
}

test_with_timeout() {
    local timeout=$1
    echo "----------------------------------------"
    echo "测试 tcp_fin_timeout = ${timeout} 秒"
    echo "----------------------------------------"
    
    # 每次测试前彻底清理
    cleanup_port
    
    # 设置参数
    sudo sysctl -w net.ipv4.tcp_fin_timeout=$timeout > /dev/null
    echo "当前 tcp_fin_timeout = $(sysctl -n net.ipv4.tcp_fin_timeout) 秒"
    
    # 启动服务器（用nc，更简单）
    nc -l 8888 &
    NC_PID=$!
    sleep 2
    
    # 创建100个连接
    echo "创建100个连接..."
    for i in {1..100}; do
        # 每个连接发送数据后立即关闭
        echo "x" | nc localhost 8888 &
        sleep 0.01
    done
    
    # 等待所有连接完成
    sleep 3
    
    # 关闭服务器
    kill $NC_PID 2>/dev/null
    wait $NC_PID 2>/dev/null
    
    # 等待连接进入稳定状态
    sleep 3
    
    # 统计（只统计连接到8888的）
    echo ""
    echo "连接状态统计:"
    
    TW_COUNT=$(ss -tan state time-wait sport = :8888 | tail -n +2 | wc -l)
    FW2_COUNT=$(ss -tan state fin-wait-2 sport = :8888 | tail -n +2 | wc -l)
    TOTAL=$(ss -tan sport = :8888 | tail -n +2 | wc -l)
    
    echo "TIME-WAIT 数量: $TW_COUNT"
    echo "FIN-WAIT-2 数量: $FW2_COUNT"
    echo "总连接数: $TOTAL"
    
    echo "----------------------------------------"
    echo ""
}

# 保存原始值
ORIGINAL=$(sysctl -n net.ipv4.tcp_fin_timeout)

# 每次测试前先清理一次
cleanup_port

# 测试不同配置
test_with_timeout 60
test_with_timeout 30
test_with_timeout 10

# 恢复
sudo sysctl -w net.ipv4.tcp_fin_timeout=$ORIGINAL > /dev/null
echo "测试完成，已恢复原始值: $ORIGINAL 秒"

# 最后清理
cleanup_port