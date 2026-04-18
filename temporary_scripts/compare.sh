#!/bin/bash
# 压测脚本 benchmark.sh
# 使用方法: ./benchmark.sh [环境] [端口]

set -e  # 出错停止

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PINK='\033[0;95m'
LIGHT_PINK='\033[1;95m'
HOT_PINK='\033[0;91m'
MAGENTA='\033[0;35m'
BOLD_PINK='\033[1;95m'
BG_PINK='\033[45m'
NC='\033[0m' # No Color

# 判断是否为第二轮测试
IS_SECOND_ROUND=${IS_SECOND_ROUND:-"false"}
FIRST_ROUND_REPORT=${FIRST_ROUND_REPORT:-""}

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
TOTAL_ROUNDS=5   # 总测试轮数（改为5轮）
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
while ! curl -s -o /dev/null -w "%{http_code}" http://localhost:$HOST_PORT | grep -q "200"; do
    RETRY_COUNT=$((RETRY_COUNT + 1))
    if [ $RETRY_COUNT -ge $MAX_RETRIES ]; then
        echo -e "${RED}错误: 服务启动失败 (HTTP 非200响应)${NC}"
        echo -e "\n${YELLOW}最后一次响应:${NC}"
        curl -v http://localhost:$HOST_PORT 2>&1 || true
        echo -e "\n${YELLOW}容器日志:${NC}"
        docker logs im-gateway-$ENV --tail 20
        exit 1
    fi
    echo -e "${YELLOW}等待服务启动... ($RETRY_COUNT/$MAX_RETRIES)${NC}"
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

【${TOTAL_ROUNDS}轮测试统计】
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
- ${TOTAL_ROUNDS}轮测试中，P99≤20ms 出现 $LOW_COUNT 次
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

# 判断是否是第二轮测试
if [ "$IS_SECOND_ROUND" = "true" ] && [ -n "$FIRST_ROUND_REPORT" ]; then
    echo -e "\n${GREEN}✓ 函数指针方案已执行完毕${NC}"
    
    # 生成方案对比报告
    COMPARE_REPORT="comparison_report_${ENV}_$(date +%Y%m%d_%H%M%S).txt"
    
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}生成方案对比报告...${NC}"
    echo -e "${BLUE}========================================${NC}"
    
    # 从第一轮报告中提取数据
    FIRST_AVG_QPS=$(grep "平均 QPS:" "$FIRST_ROUND_REPORT" | awk '{print $3}')
    FIRST_AVG_P99=$(grep "平均 P99:" "$FIRST_ROUND_REPORT" | awk '{print $3}' | sed 's/ms//')
    FIRST_BEST_P99=$(grep "最佳 P99:" "$FIRST_ROUND_REPORT" | awk '{print $3}' | sed 's/ms//')
    FIRST_WORST_P99=$(grep "最差 P99:" "$FIRST_ROUND_REPORT" | awk '{print $3}' | sed 's/ms//')
    FIRST_LOW_COUNT=$(grep "P99 ≤ 20ms:" "$FIRST_ROUND_REPORT" | awk '{print $3}')
    FIRST_CRITICAL_COUNT=$(grep "P99 ≥ 500ms:" "$FIRST_ROUND_REPORT" | awk '{print $3}')
    
    # 从第二轮报告中提取数据（当前报告）
    SECOND_AVG_QPS=$(grep "平均 QPS:" "$REPORT_FILE" | awk '{print $3}')
    SECOND_AVG_P99=$(grep "平均 P99:" "$REPORT_FILE" | awk '{print $3}' | sed 's/ms//')
    SECOND_BEST_P99=$(grep "最佳 P99:" "$REPORT_FILE" | awk '{print $3}' | sed 's/ms//')
    SECOND_WORST_P99=$(grep "最差 P99:" "$REPORT_FILE" | awk '{print $3}' | sed 's/ms//')
    SECOND_LOW_COUNT=$(grep "P99 ≤ 20ms:" "$REPORT_FILE" | awk '{print $3}')
    SECOND_CRITICAL_COUNT=$(grep "P99 ≥ 500ms:" "$REPORT_FILE" | awk '{print $3}')
    
    # 计算差异
    QPS_DIFF=$(echo "scale=2; $SECOND_AVG_QPS - $FIRST_AVG_QPS" | bc)
    QPS_PERCENT=$(echo "scale=2; ($QPS_DIFF / $FIRST_AVG_QPS) * 100" | bc)
    P99_DIFF=$(echo "scale=2; $SECOND_AVG_P99 - $FIRST_AVG_P99" | bc)
    P99_PERCENT=$(echo "scale=2; ($P99_DIFF / $FIRST_AVG_P99) * 100" | bc)
    BEST_P99_DIFF=$(echo "scale=2; $SECOND_BEST_P99 - $FIRST_BEST_P99" | bc)
    WORST_P99_DIFF=$(echo "scale=2; $SECOND_WORST_P99 - $FIRST_WORST_P99" | bc)
    LOW_COUNT_DIFF=$((SECOND_LOW_COUNT - FIRST_LOW_COUNT))
    CRITICAL_COUNT_DIFF=$((SECOND_CRITICAL_COUNT - FIRST_CRITICAL_COUNT))
    
    # 生成对比报告
    cat > $COMPARE_REPORT << EOF
===============================================================================
                    方案对比差异报告 - $ENV 环境
===============================================================================
对比时间: $(date '+%Y-%m-%d %H:%M:%S')
测试配置: 5轮压测，每轮30秒，100并发
===============================================================================

一、核心性能指标对比
-------------------------------------------------------------------------------
指标                    function方案        函数指针方案        差值        变化率
-------------------------------------------------------------------------------
平均 QPS               $FIRST_AVG_QPS        $SECOND_AVG_QPS        $QPS_DIFF        ${QPS_PERCENT}%
平均 P99 (ms)          ${FIRST_AVG_P99}ms     ${SECOND_AVG_P99}ms     ${P99_DIFF}ms     ${P99_PERCENT}%
最佳 P99 (ms)          ${FIRST_BEST_P99}ms    ${SECOND_BEST_P99}ms    ${BEST_P99_DIFF}ms    -
最差 P99 (ms)          ${FIRST_WORST_P99}ms   ${SECOND_WORST_P99}ms   ${WORST_P99_DIFF}ms    -
-------------------------------------------------------------------------------

二、稳定性指标对比
-------------------------------------------------------------------------------
指标                    function方案        函数指针方案        差值
-------------------------------------------------------------------------------
P99 ≤ 20ms (优秀轮次)   $FIRST_LOW_COUNT 轮     $SECOND_LOW_COUNT 轮     $LOW_COUNT_DIFF 轮
P99 ≥ 500ms (严重轮次)  $FIRST_CRITICAL_COUNT 轮  $SECOND_CRITICAL_COUNT 轮  $CRITICAL_COUNT_DIFF 轮
-------------------------------------------------------------------------------

三、详细性能分析
-------------------------------------------------------------------------------

【QPS 分析】
- function方案平均QPS: $FIRST_AVG_QPS
- 函数指针方案平均QPS: $SECOND_AVG_QPS
- QPS提升: $(if (( $(echo "$QPS_DIFF > 0" | bc -l) )); then echo "+$QPS_DIFF (性能提升)"; else echo "$QPS_DIFF (性能下降)"; fi)

【延迟分析】
- function方案平均P99: ${FIRST_AVG_P99}ms
- 函数指针方案平均P99: ${SECOND_AVG_P99}ms
- P99延迟: $(if (( $(echo "$P99_DIFF < 0" | bc -l) )); then echo "降低 ${P99_DIFF#-}ms (延迟改善)"; else echo "增加 $P99_DIFF ms (延迟恶化)"; fi)

【极值分析】
- 最佳P99对比: 函数指针方案 $(if (( $(echo "$SECOND_BEST_P99 < $FIRST_BEST_P99" | bc -l) )); then echo "优于"; else echo "劣于"; fi) function方案 (${BEST_P99_DIFF}ms)
- 最差P99对比: 函数指针方案 $(if (( $(echo "$SECOND_WORST_P99 < $FIRST_WORST_P99" | bc -l) )); then echo "优于"; else echo "劣于"; fi) function方案 (${WORST_P99_DIFF}ms)

四、结论与建议
-------------------------------------------------------------------------------

$(if (( $(echo "$QPS_DIFF > 0" | bc -l) && $(echo "$P99_DIFF < 0" | bc -l) )); then
    echo "✅ 函数指针方案表现更优："
    echo "   - QPS提升 ${QPS_PERCENT}%"
    echo "   - P99延迟降低 ${P99_DIFF#-}ms"
    echo "   - 建议采用函数指针方案"
elif (( $(echo "$QPS_DIFF > 0" | bc -l) )); then
    echo "⚠️  函数指针方案QPS更高，但延迟也增加："
    echo "   - QPS提升 ${QPS_PERCENT}%"
    echo "   - P99延迟增加 $P99_DIFF ms"
    echo "   - 需要根据业务需求权衡"
elif (( $(echo "$P99_DIFF < 0" | bc -l) )); then
    echo "⚠️  函数指针方案延迟更低，但QPS也下降："
    echo "   - QPS下降 ${QPS_PERCENT#-}%"
    echo "   - P99延迟降低 ${P99_DIFF#-}ms"
    echo "   - 建议在延迟敏感场景使用"
else
    echo "📊 两个方案性能相近："
    echo "   - QPS差异 ${QPS_PERCENT}%"
    echo "   - P99延迟差异 $P99_DIFF ms"
    echo "   - 可任选其一，或考虑其他因素"
fi)

===============================================================================
详细报告文件:
- function方案报告: $FIRST_ROUND_REPORT
- 函数指针方案报告: $REPORT_FILE
- 对比报告: $COMPARE_REPORT
===============================================================================

EOF

    # 显示粉色对比报告
    echo -e "\n${PINK}╔═══════════════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${PINK}║${BOLD_PINK}                       方案对比差异报告 - $ENV 环境                        ${PINK}║${NC}"
    echo -e "${PINK}╚═══════════════════════════════════════════════════════════════════════╝${NC}"
    echo -e "${LIGHT_PINK}对比时间: $(date '+%Y-%m-%d %H:%M:%S')${NC}"
    echo -e "${LIGHT_PINK}测试配置: 5轮压测，每轮30秒，100并发${NC}"
    echo -e ""
    
    echo -e "${HOT_PINK}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${BOLD_PINK}  一、核心性能指标对比${NC}"
    echo -e "${HOT_PINK}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    printf "${PINK}%-25s %-18s %-18s %-15s %s${NC}\n" "指标" "function方案" "函数指针方案" "差值" "变化率"
    echo -e "${LIGHT_PINK}----------------------------------------------------------------------${NC}"
    printf "${MAGENTA}%-25s ${LIGHT_PINK}%-18s ${LIGHT_PINK}%-18s ${PINK}%-15s ${PINK}%s${NC}\n" "平均 QPS" "$FIRST_AVG_QPS" "$SECOND_AVG_QPS" "$QPS_DIFF" "${QPS_PERCENT}%"
    printf "${MAGENTA}%-25s ${LIGHT_PINK}%-18s ${LIGHT_PINK}%-18s ${PINK}%-15s ${PINK}%s${NC}\n" "平均 P99 (ms)" "${FIRST_AVG_P99}ms" "${SECOND_AVG_P99}ms" "${P99_DIFF}ms" "${P99_PERCENT}%"
    printf "${MAGENTA}%-25s ${LIGHT_PINK}%-18s ${LIGHT_PINK}%-18s ${PINK}%-15s${NC}\n" "最佳 P99 (ms)" "${FIRST_BEST_P99}ms" "${SECOND_BEST_P99}ms" "${BEST_P99_DIFF}ms"
    printf "${MAGENTA}%-25s ${LIGHT_PINK}%-18s ${LIGHT_PINK}%-18s ${PINK}%-15s${NC}\n" "最差 P99 (ms)" "${FIRST_WORST_P99}ms" "${SECOND_WORST_P99}ms" "${WORST_P99_DIFF}ms"
    echo -e ""
    
    echo -e "${HOT_PINK}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${BOLD_PINK}  二、稳定性指标对比${NC}"
    echo -e "${HOT_PINK}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    printf "${PINK}%-35s %-18s %-18s %-15s${NC}\n" "指标" "function方案" "函数指针方案" "差值"
    echo -e "${LIGHT_PINK}----------------------------------------------------------------------${NC}"
    printf "${MAGENTA}%-35s ${LIGHT_PINK}%-18s ${LIGHT_PINK}%-18s ${PINK}%-15s${NC}\n" "P99 ≤ 20ms (优秀轮次)" "$FIRST_LOW_COUNT 轮" "$SECOND_LOW_COUNT 轮" "$LOW_COUNT_DIFF 轮"
    printf "${MAGENTA}%-35s ${LIGHT_PINK}%-18s ${LIGHT_PINK}%-18s ${PINK}%-15s${NC}\n" "P99 ≥ 500ms (严重轮次)" "$FIRST_CRITICAL_COUNT 轮" "$SECOND_CRITICAL_COUNT 轮" "$CRITICAL_COUNT_DIFF 轮"
    echo -e ""
    
    echo -e "${HOT_PINK}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${BOLD_PINK}  三、详细性能分析${NC}"
    echo -e "${HOT_PINK}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    
    # QPS分析
    if (( $(echo "$QPS_DIFF > 0" | bc -l) )); then
        QPS_RESULT="${PINK}✓ QPS提升: ${BOLD_PINK}+$QPS_DIFF${NC} ${PINK}(性能提升)${NC}"
    else
        QPS_RESULT="${PINK}✗ QPS下降: ${BOLD_PINK}$QPS_DIFF${NC} ${PINK}(性能下降)${NC}"
    fi
    echo -e "${MAGENTA}【QPS 分析】${NC}"
    echo -e "${PINK}  - function方案平均QPS: ${LIGHT_PINK}$FIRST_AVG_QPS${NC}"
    echo -e "${PINK}  - 函数指针方案平均QPS: ${LIGHT_PINK}$SECOND_AVG_QPS${NC}"
    echo -e "  $QPS_RESULT"
    echo -e ""
    
    # 延迟分析
    if (( $(echo "$P99_DIFF < 0" | bc -l) )); then
        P99_RESULT="${PINK}✓ P99延迟改善: ${BOLD_PINK}降低 ${P99_DIFF#-}ms${NC}"
    else
        P99_RESULT="${PINK}✗ P99延迟恶化: ${BOLD_PINK}增加 $P99_DIFF ms${NC}"
    fi
    echo -e "${MAGENTA}【延迟分析】${NC}"
    echo -e "${PINK}  - function方案平均P99: ${LIGHT_PINK}${FIRST_AVG_P99}ms${NC}"
    echo -e "${PINK}  - 函数指针方案平均P99: ${LIGHT_PINK}${SECOND_AVG_P99}ms${NC}"
    echo -e "  $P99_RESULT"
    echo -e ""
    
    # 极值分析
    if (( $(echo "$SECOND_BEST_P99 < $FIRST_BEST_P99" | bc -l) )); then
        BEST_RESULT="${PINK}优于${NC}"
    else
        BEST_RESULT="${PINK}劣于${NC}"
    fi
    if (( $(echo "$SECOND_WORST_P99 < $FIRST_WORST_P99" | bc -l) )); then
        WORST_RESULT="${PINK}优于${NC}"
    else
        WORST_RESULT="${PINK}劣于${NC}"
    fi
    echo -e "${MAGENTA}【极值分析】${NC}"
    echo -e "${PINK}  - 最佳P99对比: 函数指针方案 ${BOLD_PINK}$BEST_RESULT${NC} ${PINK}function方案 (${BEST_P99_DIFF}ms)${NC}"
    echo -e "${PINK}  - 最差P99对比: 函数指针方案 ${BOLD_PINK}$WORST_RESULT${NC} ${PINK}function方案 (${WORST_P99_DIFF}ms)${NC}"
    echo -e ""
    
    echo -e "${HOT_PINK}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${BOLD_PINK}  四、结论与建议${NC}"
    echo -e "${HOT_PINK}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    
    if (( $(echo "$QPS_DIFF > 0" | bc -l) && $(echo "$P99_DIFF < 0" | bc -l) )); then
        echo -e "${BG_PINK}${BOLD_PINK}  ✅ 函数指针方案表现更优：${NC}"
        echo -e "${PINK}     - QPS提升 ${QPS_PERCENT}%${NC}"
        echo -e "${PINK}     - P99延迟降低 ${P99_DIFF#-}ms${NC}"
        echo -e "${BOLD_PINK}     - 建议采用函数指针方案${NC}"
    elif (( $(echo "$QPS_DIFF > 0" | bc -l) )); then
        echo -e "${BG_PINK}${BOLD_PINK}  ⚠️  函数指针方案QPS更高，但延迟也增加：${NC}"
        echo -e "${PINK}     - QPS提升 ${QPS_PERCENT}%${NC}"
        echo -e "${PINK}     - P99延迟增加 $P99_DIFF ms${NC}"
        echo -e "${PINK}     - 需要根据业务需求权衡${NC}"
    elif (( $(echo "$P99_DIFF < 0" | bc -l) )); then
        echo -e "${BG_PINK}${BOLD_PINK}  ⚠️  函数指针方案延迟更低，但QPS也下降：${NC}"
        echo -e "${PINK}     - QPS下降 ${QPS_PERCENT#-}%${NC}"
        echo -e "${PINK}     - P99延迟降低 ${P99_DIFF#-}ms${NC}"
        echo -e "${PINK}     - 建议在延迟敏感场景使用${NC}"
    else
        echo -e "${BG_PINK}${BOLD_PINK}  📊 两个方案性能相近：${NC}"
        echo -e "${PINK}     - QPS差异 ${QPS_PERCENT}%${NC}"
        echo -e "${PINK}     - P99延迟差异 $P99_DIFF ms${NC}"
        echo -e "${PINK}     - 可任选其一，或考虑其他因素${NC}"
    fi
    
    echo -e ""
    echo -e "${HOT_PINK}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${PINK}📁 详细报告文件:${NC}"
    echo -e "${LIGHT_PINK}  - function方案报告: $FIRST_ROUND_REPORT${NC}"
    echo -e "${LIGHT_PINK}  - 函数指针方案报告: $REPORT_FILE${NC}"
    echo -e "${LIGHT_PINK}  - 对比报告: $COMPARE_REPORT${NC}"
    echo -e "${HOT_PINK}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    
    # 保存纯文本版本
    sed 's/\x1b\[[0-9;]*m//g' $COMPARE_REPORT > "${COMPARE_REPORT%.txt}_plain.txt"
    echo -e "${PINK}✓ 纯文本报告已保存至: ${COMPARE_REPORT%.txt}_plain.txt${NC}"
    
    echo -e "\n${BOLD_PINK}✨ 方案对比完成！✨${NC}"
    
    # 清理环境变量
    unset IS_SECOND_ROUND
    unset FIRST_ROUND_REPORT
    exit 0
fi

# 如果不是第二轮，则进行第一轮后的处理
echo -e "\n${YELLOW}提示: 容器仍在运行，可以继续使用:${NC}"
echo "  docker exec -it im-gateway-$ENV bash"
echo "  docker logs im-gateway-$ENV"

# 保存第一轮测试的报告文件
FIRST_REPORT_FILE="$REPORT_FILE"
echo -e "${BLUE}第一轮(function方案)报告已保存: $FIRST_REPORT_FILE${NC}"

# 显示function方案完成信息
echo -e "\n${GREEN}✓ function方案已执行完毕${NC}"

# 冷却180秒
echo -e "${YELLOW}⏰ 冷却 180 秒...${NC}"
for ((i=180; i>=1; i--)); do
    if [ $i -le 10 ] || [ $i -eq 180 ] || [ $i -eq 120 ] || [ $i -eq 60 ]; then
        echo -ne "\r${BLUE}冷却倒计时: ${i} 秒剩余...${NC}   "
    fi
    sleep 1
done
echo -e "\n"

# 退出 temporary_scripts 文件夹
cd ..

# 修改 TCPserver.h
echo -e "${YELLOW}开始修改 src/server/TCPserver.h...${NC}"

# 删除 #include<functional> 行
sed -i '/#include<functional>/d' src/server/TCPserver.h
echo -e "${GREEN}✓ 已删除 #include<functional>${NC}"

# 替换 using MessageCallback=std::function<std::string (char const* msg,ssize_t len)>; 行
sed -i 's/using MessageCallback=std::function<std::string (char const\* msg,ssize_t len)>;/using MessageCallback=std::string (*)(char const* msg,ssize_t len);/' src/server/TCPserver.h
echo -e "${GREEN}✓ 已替换 MessageCallback 定义${NC}"

# 保存文件（sed已经自动保存）
echo -e "${GREEN}✓ 文件已保存${NC}"

# cd build 并编译
echo -e "${YELLOW}进入 build 目录...${NC}"
cd build
echo -e "${GREEN}✓ 已进入 build 目录${NC}"

echo -e "${YELLOW}执行 cmake ..${NC}"
cmake ..
echo -e "${GREEN}✓ cmake 执行完成${NC}"

echo -e "${YELLOW}执行 make${NC}"
make
echo -e "${GREEN}✓ make 执行完成${NC}"

# 返回项目根目录
cd ..

# 重新构建镜像
echo -e "${YELLOW}# 1. 重新构建镜像${NC}"
docker compose build --no-cache
echo -e "${GREEN}✓ 镜像构建完成${NC}"

# 强制重建容器
echo -e "${YELLOW}# 2. 强制重建容器${NC}"
docker compose up -d --force-recreate
echo -e "${GREEN}✓ 容器重建完成${NC}"

# 等待新容器启动
echo -e "${YELLOW}等待新容器启动...${NC}"
sleep 10

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}代码修改和容器重建完成！${NC}"
echo -e "${GREEN}准备开始第二轮压测（函数指针方案）...${NC}"
echo -e "${GREEN}========================================${NC}"

# 重新执行脚本进行第二轮测试，传递环境参数，并标记为第二轮
export IS_SECOND_ROUND="true"
export FIRST_ROUND_REPORT="$FIRST_REPORT_FILE"
exec $0 $ENV $PORT