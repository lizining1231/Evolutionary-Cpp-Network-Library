#!/bin/bash

# 创建归档目录（带时间戳）
ARCHIVE_DIR="benchmark_archive_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$ARCHIVE_DIR"

# 移动所有压测相关文件
echo "整理压测文件到 $ARCHIVE_DIR ..."

# 移动 wrk 输出文件
mv wrk_result*.txt "$ARCHIVE_DIR/" 2>/dev/null
mv wrk_*.txt "$ARCHIVE_DIR/" 2>/dev/null

# 移动 benchmark 报告
mv benchmark_report_*.txt "$ARCHIVE_DIR/" 2>/dev/null
mv benchmark_*.txt "$ARCHIVE_DIR/" 2>/dev/null

# 移动临时测试文件
mv test_*.txt "$ARCHIVE_DIR/" 2>/dev/null
mv test_*.log "$ARCHIVE_DIR/" 2>/dev/null


echo "整理完成！文件已移动到: $ARCHIVE_DIR"
ls -la "$ARCHIVE_DIR"