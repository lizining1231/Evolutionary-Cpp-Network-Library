#!/bin/bash
# 压测脚本 benchmark.sh
# 使用方法: ./benchmark.sh [环境] [端口]

set -e  # 出错停止

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# ==================== 环境切换 ====================
ENV=${1:-"test"}  # 默认使用 test 环境
PORT=${2:-8080}

echo -e "${BLUE}========================================${NC}"
echo -e "${GREEN}压测脚本 - 环境: $ENV, 端口: $PORT${NC}"
echo -e "${BLUE}========================================${NC}"

# 查找环境配置文件（支持多个可能的位置）
POSSIBLE_PATHS=(
    ".env.$ENV"
    ".docker/.env/.env.$ENV"
    "config/.env.$ENV"
    "env/.env.$ENV"
    "../.env.$ENV"
)

ENV_FILE=""
for path in "${POSSIBLE_PATHS[@]}"; do
    if [ -f "$path" ]; then
        ENV_FILE="$path"
        echo -e "${GREEN}✓ 找到配置文件: $path${NC}"
        break
    fi
done

if [ -z "$ENV_FILE" ]; then
    echo -e "${RED}错误: 找不到环境配置文件 .env.$ENV${NC}"
    echo "查找位置:"
    printf '  - %s\n' "${POSSIBLE_PATHS[@]}"
    exit 1
fi

# 复制配置文件
echo -e "${YELLOW}切换到 $ENV 环境...${NC}"
cp "$ENV_FILE" .env

# 读取环境变量
if [ -f ".env" ]; then
    # 读取宿主机端口 (EXTERNAL_PORT)
    HOST_PORT=$(grep -E '^EXTERNAL_PORT=' .env | cut -d= -f2 | tr -d '"' | tr -d "'")
    HOST_PORT=${HOST_PORT:-$PORT}
    
    # 读取容器内端口 (PORT)
    CONTAINER_PORT=$(grep -E '^PORT=' .env | cut -d= -f2 | tr -d '"' | tr -d "'")
    CONTAINER_PORT=${CONTAINER_PORT:-8080}
else
    HOST_PORT=$PORT
    CONTAINER_PORT=8080
fi

echo -e "${GREEN}✓ 宿主机端口: $HOST_PORT, 容器内端口: $CONTAINER_PORT${NC}"

# 重启容器使用新环境
echo -e "${YELLOW}重启容器应用新环境...${NC}"
docker compose down 2>/dev/null || true
docker compose up -d

# 等待容器启动
echo -e "${YELLOW}等待容器启动...${NC}"
sleep 5

# 检查服务是否自动启动（只检查进程，不检查端口）
if docker exec im-gateway-$ENV pgrep TCPserver > /dev/null; then
    SERVER_PID=$(docker exec im-gateway-$ENV pgrep TCPserver | head -1)
    echo -e "${GREEN}✓ TCPserver 已自动启动 (PID: $SERVER_PID)${NC}"
else
    echo -e "${RED}✗ TCPserver 未自动启动，查看日志:${NC}"
    docker logs im-gateway-$ENV --tail 50
    exit 1
fi

# ==================== 压测配置 ====================
URL="http://localhost:$HOST_PORT"
DURATION=30  # 每次测试30秒
THREADS=$(nproc)  # 自动获取CPU核心数
TOTAL_ROUNDS=20   # 总测试轮数
CURRENT_ROUND=1
WAIT_TIME=180  # 轮次间隔时间（秒），3分钟 = 180秒
REPORT_FILE="benchmark_report_${ENV}_$(date +%Y%m%d_%H%M%S).txt"

echo -e "${BLUE}========================================${NC}"
echo -e "${GREEN}高性能压测脚本 - 循环测试模式${NC}"
echo -e "${BLUE}========================================${NC}"
echo "测试环境: $ENV"
echo "测试目标: $URL"
echo "CPU核心数: $THREADS"
echo "每轮时长: ${DURATION}s"
echo "测试轮数: ${TOTAL_ROUNDS}"
echo "轮次间隔: ${WAIT_TIME}s (3分钟)"
echo "报告文件: $REPORT_FILE"
echo -e "${BLUE}========================================${NC}\n"

# 检查wrk是否安装
if ! command -v wrk &> /dev/null; then
    echo -e "${RED}错误: wrk 未安装${NC}"
    echo "请执行: sudo apt-get install wrk"
    exit 1
fi

# 检查服务是否可用（使用curl测试实际响应）
echo -e "${YELLOW}检查服务状态...${NC}"
MAX_RETRIES=10
RETRY_COUNT=0
while true; do
    # 先把状态码保存到变量，避免管道问题
    HTTP_CODE=$(curl -s -o /dev/null -w "%{http_code}" http://localhost:$HOST_PORT 2>/dev/null)
    
    # 检查是否返回200
    if [ "$HTTP_CODE" = "200" ]; then
        echo -e "${GREEN}✓ 服务运行正常 (HTTP 200)${NC}\n"
        break
    fi
    
    # 不是200，就继续重试
    RETRY_COUNT=$((RETRY_COUNT + 1))
    if [ $RETRY_COUNT -ge $MAX_RETRIES ]; then
        echo -e "${RED}错误: 服务启动失败 (HTTP 返回码: $HTTP_CODE)${NC}"
        echo -e "\n${YELLOW}最后一次响应:${NC}"
        curl -v http://localhost:$HOST_PORT 2>&1 || true
        echo -e "\n${YELLOW}容器日志:${NC}"
        docker logs im-gateway-$ENV --tail 20
        exit 1
    fi
    echo -e "${YELLOW}等待服务启动... ($RETRY_COUNT/$MAX_RETRIES) HTTP状态: $HTTP_CODE${NC}"
    sleep 2
done
echo -e "${GREEN}✓ 服务运行正常 (HTTP 200)${NC}\n"

# 初始化结果文件
cat > $REPORT_FILE << EOF
===============================================================================
                    压测报告 - $ENV 环境 (循环测试)
===============================================================================
测试环境: $ENV
测试目标: $URL
CPU核心数: $THREADS
每轮时长: ${DURATION}s
测试轮数: ${TOTAL_ROUNDS}
轮次间隔: ${WAIT_TIME}s (3分钟)
测试时间: $(date '+%Y-%m-%d %H:%M:%S')
===============================================================================

一、测试结果汇总
-------------------------------------------------------------------------------
轮次      并发数    QPS      平均延迟   P99延迟    错误数    状态    触发采集
-------------------------------------------------------------------------------
EOF

# 1. 预热
echo -e "${YELLOW}[预热] 预热环境 (60秒)${NC}"
wrk -t$THREADS -c100 -d60s $URL > /dev/null 2>&1 || true
echo -e "${GREEN}✓ 预热完成${NC}\n"
sleep 2

# 2. 基准测试（仅显示，不计入轮次）
echo -e "${YELLOW}[基准] 基准测试 (100并发 10秒)${NC}"
echo -e "\n${GREEN}=== 基准测试结果 (100并发) ===${NC}"
wrk -t$THREADS -c100 -d10s --latency $URL
echo -e "${GREEN}================================${NC}\n"
sleep 2

# 采集状态信息的函数
collect_status() {
    local round=$1
    local p99=$2
    local reason=$3
    local timestamp=$(date '+%Y-%m-%d %H:%M:%S')
    local status_file="status_${ENV}_round${round}_${reason}_$(date +%Y%m%d_%H%M%S).txt"
    
    echo -e "\n${YELLOW}>>> 触发状态采集 (轮次: $round, P99: ${p99}ms, 原因: $reason)${NC}"
    
    {
        echo "==============================================================================="
        echo "状态采集报告 - 轮次 $round"
        echo "采集时间: $timestamp"
        echo "触发原因: P99延迟 = ${p99}ms ($reason)"
        echo "==============================================================================="
        echo ""
        
        echo "1. 当前时间"
        echo "------------------------------------------------------------------------------"
        date
        echo ""
        
        echo "2. 服务状态 (容器: im-gateway-$ENV)"
        echo "------------------------------------------------------------------------------"
        # 获取进程PID
        PID=$(docker exec im-gateway-$ENV pgrep TCPserver | head -1)
        if [ -n "$PID" ]; then
            echo "进程 PID: $PID"
            echo ""
            echo "进程状态:"
            docker exec im-gateway-$ENV cat /proc/$PID/status | grep -E "(VmRSS|VmData|VmPeak|VmSize)" || echo "无法读取进程状态"
            echo ""
            echo "文件描述符数量:"
            docker exec im-gateway-$ENV ls /proc/$PID/fd 2>/dev/null | wc -l || echo "无法读取FD数量"
        else
            echo "未找到 TCPserver 进程"
        fi
        
        # 尝试curl status接口（如果有）
        echo ""
        echo "Status 接口:"
        curl -s http://localhost:$HOST_PORT/status 2>&1 || echo "无 status 接口或访问失败"
        echo ""
        
        echo "3. 系统状态"
        echo "------------------------------------------------------------------------------"
        echo "TIME_WAIT 连接数:"
        ss -tan | grep TIME_WAIT | wc -l
        echo ""
        echo "CPU 频率信息:"
        cat /proc/cpuinfo | grep MHz | head -1 || echo "无法读取CPU频率"
        echo ""
        echo "系统负载:"
        uptime
        echo ""
        echo "内存使用:"
        free -h
        echo ""
        
        echo "4. 容器状态"
        echo "------------------------------------------------------------------------------"
        docker stats im-gateway-$ENV --no-stream
        echo ""
        
        echo "==============================================================================="
    } | tee -a "$status_file"
    
    echo -e "${GREEN}状态信息已保存至: $status_file${NC}\n"
}

# 3. 循环测试
echo -e "${YELLOW}[循环测试] 开始 ${TOTAL_ROUNDS} 轮压测 (每轮 ${DURATION}秒，间隔 ${WAIT_TIME}秒)${NC}\n"

# 存储每轮结果用于最终汇总
declare -a ROUND_RESULTS

while [ $CURRENT_ROUND -le $TOTAL_ROUNDS ]; do
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}第 $CURRENT_ROUND / $TOTAL_ROUNDS 轮测试${NC}"
    echo -e "${BLUE}========================================${NC}"
    
    # 显示开始时间
    START_TIME=$(date '+%Y-%m-%d %H:%M:%S')
    echo -e "${GREEN}开始时间: $START_TIME${NC}"
    
    # 执行测试并捕获输出
    OUTPUT=$(wrk -t$THREADS -c100 -d${DURATION}s --latency $URL 2>&1)
    
    # 显示完整原始输出
    echo -e "\n${GREEN}=== 第 ${CURRENT_ROUND} 轮完整原始输出 ===${NC}"
    echo "$OUTPUT"
    echo -e "${GREEN}========================================${NC}\n"
    
    # 提取数据
    QPS=$(echo "$OUTPUT" | grep "Requests/sec" | awk '{print $2}')
    LATENCY=$(echo "$OUTPUT" | grep "Latency" | head -1 | awk '{print $2}' | sed 's/ms//')
    P99=$(echo "$OUTPUT" | grep "99%" | awk '{print $2}' | sed 's/ms//')
    ERRORS=$(echo "$OUTPUT" | grep "Socket errors" | awk '{print $4}' | cut -d',' -f1)
    ERRORS=${ERRORS:-0}
    
    # 判断状态
    TRIGGERED=""
    if [ -n "$P99" ] && [ $(echo "$P99 <= 20" | bc) -eq 1 ] 2>/dev/null; then
        STATUS="${GREEN}优秀${NC}"
        STATUS_TEXT="优秀"
        TRIGGERED="是(P99≤20ms)"
        # 触发状态采集
        collect_status $CURRENT_ROUND "$P99" "p99_low"
    elif [ -n "$P99" ] && [ $(echo "$P99 >= 500" | bc) -eq 1 ] 2>/dev/null; then
        STATUS="${RED}严重延迟${NC}"
        STATUS_TEXT="严重延迟"
        TRIGGERED="是(P99≥500ms)"
        # 触发状态采集
        collect_status $CURRENT_ROUND "$P99" "p99_high"
    elif [ $(echo "$LATENCY > 100" | bc) -eq 1 ] 2>/dev/null; then
        STATUS="${RED}过载${NC}"
        STATUS_TEXT="过载"
    elif [ $(echo "$LATENCY > 50" | bc) -eq 1 ] 2>/dev/null; then
        STATUS="${YELLOW}瓶颈${NC}"
        STATUS_TEXT="瓶颈"
    else
        STATUS="${GREEN}正常${NC}"
        STATUS_TEXT="正常"
    fi
    
    # 显示结果摘要
    END_TIME=$(date '+%Y-%m-%d %H:%M:%S')
    echo -e "${YELLOW}结束时间: $END_TIME${NC}"
    printf "  📊 摘要: QPS: %8s | 平均延迟: %5sms | P99: %5sms | 错误: %3s | 状态: %b\n" \
           "$QPS" "$LATENCY" "$P99" "$ERRORS" "$STATUS"
    
    # 保存结果到文件
    printf "%-6s %-8s %-8s %-8s %-8s %-8s %s\n" \
           "$CURRENT_ROUND" "100" "$QPS" "${LATENCY}ms" "${P99}ms" "$ERRORS" "${STATUS_TEXT}${TRIGGERED:+, $TRIGGERED}" >> $REPORT_FILE
    
    # 存储结果用于最终统计
    ROUND_RESULTS+=("$CURRENT_ROUND:$QPS:$LATENCY:$P99:$ERRORS")
    
    # 如果不是最后一轮，等待指定时间
    if [ $CURRENT_ROUND -lt $TOTAL_ROUNDS ]; then
        echo -e "\n${YELLOW}⏰ 等待 ${WAIT_TIME} 秒 (3分钟) 后开始下一轮测试...${NC}"
        echo -e "${YELLOW}按 Ctrl+C 可提前结束测试${NC}\n"
        
        # 倒计时显示
        for ((i=$WAIT_TIME; i>=1; i--)); do
            if [ $i -le 10 ] || [ $((i % 30)) -eq 0 ]; then
                echo -ne "\r${BLUE}倒计时: ${i} 秒剩余...${NC}   "
            fi
            sleep 1
        done
        echo -e "\n"
    fi
    
    CURRENT_ROUND=$((CURRENT_ROUND + 1))
done

# 4. 统计分析
echo -e "\n${YELLOW}[统计分析] 计算整体性能指标${NC}"

# 计算统计数据
TOTAL_QPS=0
TOTAL_LAT=0
TOTAL_P99=0
MIN_P99=999999
MAX_P99=0
BEST_ROUND=0
WORST_ROUND=0

for R in "${ROUND_RESULTS[@]}"; do
    IFS=':' read -r ROUND QPS LAT P99 ERR <<< "$R"
    TOTAL_QPS=$(echo "$TOTAL_QPS + $QPS" | bc)
    TOTAL_LAT=$(echo "$TOTAL_LAT + $LAT" | bc)
    TOTAL_P99=$(echo "$TOTAL_P99 + $P99" | bc)
    
    if (( $(echo "$P99 < $MIN_P99" | bc -l) )); then
        MIN_P99=$P99
        BEST_ROUND=$ROUND
    fi
    if (( $(echo "$P99 > $MAX_P99" | bc -l) )); then
        MAX_P99=$P99
        WORST_ROUND=$ROUND
    fi
done

AVG_QPS=$(echo "scale=2; $TOTAL_QPS / $TOTAL_ROUNDS" | bc)
AVG_LAT=$(echo "scale=2; $TOTAL_LAT / $TOTAL_ROUNDS" | bc)
AVG_P99=$(echo "scale=2; $TOTAL_P99 / $TOTAL_ROUNDS" | bc)

# 写入完整摘要
cat >> $REPORT_FILE << EOF

二、完整测试摘要
-------------------------------------------------------------------------------

【基准测试结果】
$(wrk -t$THREADS -c100 -d10s $URL 2>&1)

【20轮测试统计】
- 测试轮数: $TOTAL_ROUNDS
- 平均 QPS: $AVG_QPS
- 平均延迟: ${AVG_LAT}ms
- 平均 P99: ${AVG_P99}ms
- 最佳 P99: ${MIN_P99}ms (第 $BEST_ROUND 轮)
- 最差 P99: ${MAX_P99}ms (第 $WORST_ROUND 轮)

【P99延迟分布】
EOF

# 统计P99分布
LOW_COUNT=0
MEDIUM_COUNT=0
HIGH_COUNT=0
CRITICAL_COUNT=0

for R in "${ROUND_RESULTS[@]}"; do
    IFS=':' read -r ROUND QPS LAT P99 ERR <<< "$R"
    if (( $(echo "$P99 <= 20" | bc -l) )); then
        LOW_COUNT=$((LOW_COUNT + 1))
    elif (( $(echo "$P99 <= 100" | bc -l) )); then
        MEDIUM_COUNT=$((MEDIUM_COUNT + 1))
    elif (( $(echo "$P99 <= 500" | bc -l) )); then
        HIGH_COUNT=$((HIGH_COUNT + 1))
    else
        CRITICAL_COUNT=$((CRITICAL_COUNT + 1))
    fi
done

cat >> $REPORT_FILE << EOF
- P99 ≤ 20ms: $LOW_COUNT 轮 (优秀)
- P99 20-100ms: $MEDIUM_COUNT 轮 (正常)
- P99 100-500ms: $HIGH_COUNT 轮 (瓶颈)
- P99 ≥ 500ms: $CRITICAL_COUNT 轮 (严重)

四、测试结论
-------------------------------------------------------------------------------
$(date '+%Y-%m-%d %H:%M:%S') 完成压测

系统性能总结:
- 平均吞吐量: $AVG_QPS QPS @ 100并发
- 平均延迟: ${AVG_LAT}ms
- 平均 P99: ${AVG_P99}ms
- 最优 P99: ${MIN_P99}ms
- 最差 P99: ${MAX_P99}ms

稳定性评估:
- 20轮测试中，P99≤20ms 出现 $LOW_COUNT 次
- P99≥500ms 出现 $CRITICAL_COUNT 次
- 系统稳定性: $(if [ $CRITICAL_COUNT -eq 0 ]; then echo "良好"; else echo "需要关注"; fi)

建议:
1. 关注 P99 延迟波动情况
2. 当 P99 超过 500ms 时，检查系统资源状态
3. 当 P99 低于 20ms 时，记录优化配置供参考

五、各轮详细数据
-------------------------------------------------------------------------------
EOF

# 添加各轮详细数据
for R in "${ROUND_RESULTS[@]}"; do
    IFS=':' read -r ROUND QPS LAT P99 ERR <<< "$R"
    printf "第%2s轮: QPS=%8s, 平均延迟=%6sms, P99=%6sms, 错误数=%3s\n" \
           "$ROUND" "$QPS" "$LAT" "$P99" "$ERR" >> $REPORT_FILE
done

cat >> $REPORT_FILE << EOF

===============================================================================
EOF

# 清理测试环境（但不停止容器）
echo -e "\n${YELLOW}清理测试环境...${NC}"
docker exec im-gateway-$ENV pkill TCPserver 2>/dev/null || true

echo -e "\n${GREEN}✓ 测试完成！${NC}"
echo -e "${BLUE}报告已保存至: $REPORT_FILE${NC}"
echo -e "\n${BLUE}报告摘要:${NC}"
echo "----------------------------------------"
echo "总测试轮数: $TOTAL_ROUNDS"
echo "平均 QPS: $AVG_QPS"
echo "平均 P99: ${AVG_P99}ms"
echo "最佳 P99: ${MIN_P99}ms (第 $BEST_ROUND 轮)"
echo "最差 P99: ${MAX_P99}ms (第 $WORST_ROUND 轮)"
echo "P99分布: ≤20ms: $LOW_COUNT轮, 20-100ms: $MEDIUM_COUNT轮, 100-500ms: $HIGH_COUNT轮, ≥500ms: $CRITICAL_COUNT轮"
echo "----------------------------------------"

echo -e "\n${YELLOW}提示: 容器仍在运行，可以继续使用:${NC}"
echo "  docker exec -it im-gateway-$ENV bash"
echo "  docker logs im-gateway-$ENV"