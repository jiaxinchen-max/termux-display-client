#!/bin/bash
export XDG_RUNTIME_DIR=$PREFIX/tmp/runtime-$USER
mkdir -p $XDG_RUNTIME_DIR
chmod 700 $XDG_RUNTIME_DIR

export WLR_TERMUX_WIDTH=1024
export WLR_TERMUX_HEIGHT=768

# 启用更多调试信息
#export WLR_DEBUG=1
#export SWAY_DEBUG=1

# 强制软件cursor
export WLR_NO_HARDWARE_CURSORS=1

# 创建日志文件，带时间戳
LOG_FILE="$HOME/sway_debug_$(date +%Y%m%d_%H%M%S).log"
echo "Starting sway with debug logging, logging to: $LOG_FILE"

# 重定向所有输出到日志文件，同时在终端显示
WLR_BACKENDS=termux WLR_RENDER=pixman sway -d 2>&1 | tee "$LOG_FILE"