#!/usr/bin/env bash
# sas-screen 一键安装 (KDE Plasma 专属版)
# 说明: 编译 + 安装 + 屏蔽 systemd 重启 + 清理旧 KWin 脚本 + 输出快捷键绑定指南
# 用法: sudo ./deploy/install.sh

set -uo pipefail

SCRIPT_PATH="$(readlink -f "$0")"
cd "$(dirname "$SCRIPT_PATH")/.." || { echo "无法进入项目根目录" >&2; exit 1; }
ROOT_DIR="$(pwd)"

GREEN=$'\033[32m'; YELLOW=$'\033[33m'; RED=$'\033[31m'; NC=$'\033[0m'
say()  { printf "${GREEN}[sas]${NC} %s\n" "$*"; }
warn() { printf "${YELLOW}[sas!]${NC} %s\n" "$*"; }
err()  { printf "${RED}[sas!]${NC} %s\n" "$*" >&2; }

BIN_NAME="sas-screen"
USER_HOME="${HOME:-/root}"

# ---------- 1. 编译并安装 ----------
system_install() {
    command -v cmake >/dev/null 2>&1 || { err "未找到 cmake，请先安装: sudo pacman -S cmake 或 sudo apt install cmake"; exit 1; }
    
    say "正在编译项目..."
    cmake -S "$ROOT_DIR" -B "$ROOT_DIR/build" -DCMAKE_BUILD_TYPE=Release >/dev/null
    cmake --build "$ROOT_DIR/build" -j"$(nproc)" >/dev/null || { err "编译失败"; exit 1; }

    if [ "$(id -u)" -ne 0 ]; then
        err "错误：请使用 sudo 运行此脚本以完成系统级安装！"
        exit 1
    fi

    say "安装到 /usr/local..."
    cmake --install "$ROOT_DIR/build" >/dev/null || exit 1
    mkdir -p "/usr/local/share/sas-screen"
    [ -f "$ROOT_DIR/config/sas-screen.json" ] && cp "$ROOT_DIR/config/sas-screen.json" "/usr/local/share/sas-screen/"
    
    mask_cad
}

# ---------- 2. 屏蔽系统底层 CAD ----------
mask_cad() {
    say "屏蔽 systemd 的 Ctrl+Alt+Delete 默认重启行为..."
    systemctl mask ctrl-alt-del.target 2>/dev/null || true
    systemctl daemon-reload 2>/dev/null || true
    say "  ctrl-alt-del.target 已 mask"
}

# ---------- 3. KDE 配置 ----------
config_kde() {
    say "KDE Plasma 检测到: 清理旧版 KWin 脚本残留，并准备快捷键配置..."
    
    # 1. 彻底清理之前可能残留的 KWin 脚本（因为现在全靠 C++ 实现了）
    local old_script="$USER_HOME/.local/share/kwin/scripts/sas-screen"
    if [ -d "$old_script" ]; then
        rm -rf "$old_script"
        say "  已移除旧的 KWin 脚本目录"
    fi

    # 2. 输出绑定指南
    echo ""
    echo "=================================================================="
    say "⚠️ 请在 KDE 系统设置中完成最后一步绑定："
    echo "  1. 打开【系统设置 -> 快捷键】"
    echo "  2. 搜索 'Ctrl+Alt+Delete'，找到『显示注销提示』并清空其快捷键"
    echo "  3. 点击右下角【添加 -> 命令或脚本】"
    echo "  4. 触发器设为: Ctrl+Alt+Delete"
    echo "  5. 动作填写:   env QT_IM_MODULE=fcitx /usr/local/bin/sas-screen --show"
    echo "=================================================================="
}

# ---------- 4. 用户级环境检测与执行 ----------
user_install() {
    # 简单校验环境
    if [[ "${XDG_CURRENT_DESKTOP:-}" =~ (KDE|Plasma) ]]; then
        config_kde
    else
        warn "警告：当前环境似乎不是 KDE Plasma！"
        warn "本项目目前仅针对 KDE 做了快捷键和依赖适配。"
    fi

    echo ""
    say "安装完成 ✓"
    echo "  程序:      /usr/local/bin/$BIN_NAME"
    echo "  触发键:    Ctrl+Alt+Delete（Esc/取消 关闭界面）"
    echo "  按钮命令:  ~/.config/sas-screen/config.json（可选）"
}

# ---------- 5. 主流程 ----------
main() {
    case "${1:-}" in
        --user-only)
            user_install ;;
        *)
            system_install
            if [ -n "${SUDO_USER:-}" ]; then
                say "系统级安装完成，正在为当前用户 (${SUDO_USER}) 应用 KDE 配置..."
                # 将环境变量传给普通用户执行，确保能正确识别 KDE 环境
                sudo -H -u "$SUDO_USER" \
                    XDG_CURRENT_DESKTOP="${XDG_CURRENT_DESKTOP:-KDE}" \
                    bash "$SCRIPT_PATH" --user-only
            else
                user_install
            fi ;;
    esac
}

main "$@"