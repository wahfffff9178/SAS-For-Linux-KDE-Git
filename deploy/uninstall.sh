#!/usr/bin/env bash
# sas-screen 一键卸载 (KDE Plasma 专属版)
# 说明: 停止进程、移除二进制、清理旧版 KWin 脚本残留、恢复 systemd 默认行为。
# 用法: ./deploy/uninstall.sh
#       若当初用 root 安装到 /usr/local，请加 sudo: sudo ./deploy/uninstall.sh
set -uo pipefail

cd "$(dirname "$0")/.."

GREEN=$'\033[32m'; YELLOW=$'\033[33m'; RED=$'\033[31m'; NC=$'\033[0m'
say()  { printf "${GREEN}[sas]${NC} %s\n" "$*"; }
warn() { printf "${YELLOW}[sas!]${NC} %s\n" "$*"; }
err()  { printf "${RED}[sas-x]${NC} %s\n" "$*"; }

USER_HOME="${HOME:-/root}"

say "1/6 停止正在运行的实例并清理 IPC 套接字..."
pkill -x sas-screen 2>/dev/null || true
rm -f /tmp/sas-screen-*.sock

say "2/6 删除可执行文件与数据文件..."
rm -f "$USER_HOME/.local/bin/sas-screen" 2>/dev/null
rm -rf "$USER_HOME/.local/share/sas-screen" 2>/dev/null
rm -f "$USER_HOME/.config/autostart/sas-screen.desktop" 2>/dev/null

if [ -f /usr/local/bin/sas-screen ]; then
    if [ -w /usr/local/bin ]; then
        rm -f /usr/local/bin/sas-screen /usr/local/share/sas-screen/sas-screen.json
        rmdir /usr/local/share/sas-screen 2>/dev/null || true
    else
        warn "检测到系统级安装(/usr/local)，请再执行一次: sudo ./deploy/uninstall.sh"
    fi
fi

say "3/6 备份并清理用户配置文件..."
if [ -d "$USER_HOME/.config/sas-screen" ]; then
    if [ -f "$USER_HOME/.config/sas-screen/config.json" ]; then
        mv -v "$USER_HOME/.config/sas-screen/config.json" \
              "$USER_HOME/.config/sas-screen/config.json.bak" 2>/dev/null \
        && say "自定义 config.json 已备份为 config.json.bak"
    fi
    rm -rf "$USER_HOME/.config/sas-screen"
fi

say "4/6 清理历史遗留的 KWin 脚本..."
# 仅清理旧版本可能留下的 KWin 脚本目录（新版 C++ 已不需要）
rm -rf "$USER_HOME/.local/share/kwin/scripts/sas-screen"
# 触发 KWin 重载，让残留的快捷键彻底消失
qdbus6 org.kde.KWin /KWin reconfigure 2>/dev/null || qdbus org.kde.KWin /KWin reconfigure 2>/dev/null || true

say "5/6 恢复 systemd 默认行为..."
read -rp "是否恢复 Ctrl+Alt+Delete 的原默认行为（重启）？[y/N] " ans
case "$ans" in
    y|Y|yes|YES)
        if [ "$(id -u)" -eq 0 ]; then
            systemctl unmask ctrl-alt-del.target 2>/dev/null || true
        else
            sudo systemctl unmask ctrl-alt-del.target 2>/dev/null || \
                warn "未恢复(需要 sudo): 可稍后运行 sudo systemctl unmask ctrl-alt-del.target"
        fi
        say "已恢复: Ctrl+Alt+Delete 将重新触发重启"
        ;;
    *) say "已跳过: ctrl-alt-del.target 保持 mask 状态";;
esac

say "6/6 完成。"
echo
say "卸载完毕 ✓  残留检查: command -v sas-screen 应无输出"
warn "注意: 你在 KDE 系统设置中手动绑定的 'Ctrl+Alt+Delete' 快捷键需要手动前往【系统设置 -> 快捷键】中删除。"