#!/bin/bash
# integration_test.sh — Linux 集成测试
# 验证程序启动、配置、日志、内存
# 用法: DISPLAY=:99 ./integration_test.sh

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/../build"
BINARY="$BUILD_DIR/dock_wmac"
CONFIG_FILE="$HOME/.config/Dock_WMac/config.json"
LOG_FILE="$HOME/.local/share/Dock_WMac/dock.log"
PASS=0
FAIL=0

GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'
pass() { echo -e "${GREEN}✓ $1${NC}"; PASS=$((PASS+1)); }
fail() { echo -e "${RED}✗ $1${NC}"; FAIL=$((FAIL+1)); }

APP_PID=""
cleanup() {
    if [ -n "$APP_PID" ]; then
        kill "$APP_PID" 2>/dev/null || true
        wait "$APP_PID" 2>/dev/null || true
    fi
}
trap cleanup EXIT

echo "=== Dock_WMac 集成测试 ==="

# 1. 二进制
if [ -x "$BINARY" ]; then
    pass "二进制文件存在"
else
    fail "二进制不存在"
    exit 1
fi

# 2. 检查显示器
if [ -z "$DISPLAY" ]; then
    fail "DISPLAY 未设置"
    exit 1
else
    pass "使用显示器 $DISPLAY"
fi

# 3. 清理残留共享内存
python3 -c "
import subprocess, re
out = subprocess.check_output(['ipcs', '-m'], text=True)
for line in out.splitlines():
    if 'Dock_WMac' in line or '65542' in line:
        parts = line.split()
        if len(parts) >= 2:
            subprocess.run(['ipcrm', '-m', parts[1]], stderr=subprocess.DEVNULL)
" 2>/dev/null || true

# 4. 启动程序
rm -f "$LOG_FILE"
"$BINARY" &
APP_PID=$!
sleep 2

# 5. 进程存活
if kill -0 "$APP_PID" 2>/dev/null; then
    pass "程序启动成功 (PID=$APP_PID)"
else
    fail "程序退出"
    [ -f "$LOG_FILE" ] && cat "$LOG_FILE"
    exit 1
fi

# 6. 配置文件
if [ -f "$CONFIG_FILE" ]; then
    pass "配置文件已创建"
else
    fail "配置文件缺失"
fi

# 7. JSON 格式
if [ -f "$CONFIG_FILE" ] && python3 -m json.tool "$CONFIG_FILE" >/dev/null 2>&1; then
    pass "JSON 格式正确"
else
    fail "JSON 格式错误"
fi

# 8. 日志文件
if [ -f "$LOG_FILE" ]; then
    pass "日志文件已创建"
    if grep -q "启动" "$LOG_FILE"; then
        pass "日志含启动信息"
    else
        fail "日志无启动信息"
    fi
else
    fail "日志文件缺失"
fi

# 9. 内存（Qt6 GUI 应用合理范围 < 150MB）
RSS_KB=$(ps -o rss= -p "$APP_PID" 2>/dev/null || echo 0)
RSS_MB=$((RSS_KB / 1024))
if [ "$RSS_MB" -lt 150 ]; then
    pass "内存 ${RSS_MB}MB < 150MB"
else
    fail "内存 ${RSS_MB}MB >= 150MB"
fi

# 10. 停止程序
kill "$APP_PID" 2>/dev/null
wait "$APP_PID" 2>/dev/null
APP_PID=""

# 11. 单元测试
echo ""
cd "$BUILD_DIR"
if ctest --output-on-failure 2>&1; then
    pass "单元测试通过"
else
    fail "单元测试失败"
fi

echo ""
echo "=== 结果: ${PASS} 通过, ${FAIL} 失败 ==="
if [ "$FAIL" -eq 0 ]; then
    exit 0
else
    exit 1
fi
