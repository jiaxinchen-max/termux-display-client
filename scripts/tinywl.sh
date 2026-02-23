#!/bin/bash
export XDG_RUNTIME_DIR=$PREFIX/tmp/runtime-$USER
mkdir -p $XDG_RUNTIME_DIR
chmod 700 $XDG_RUNTIME_DIR

export WLR_TERMUX_WIDTH=1024
export WLR_TERMUX_HEIGHT=768

# 创建日志文件，带时间戳
LOG_FILE="$HOME/tinywl_$(date +%Y%m%d_%H%M%S).log"
echo "Starting tinywl, logging to: $LOG_FILE"

# 重定向所有输出到日志文件，同时在终端显示
WLR_BACKENDS=termux WLR_RENDER=pixman tinywl -s "${1:-mousepad}" 2>&1 | tee "$LOG_FILE"
