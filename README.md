# SAS-For-Linux-KDE-GIT
在Linux还原Windows的安全选项（SAS）(For KDE 6)
=======
# SAS for Linux（安全注意屏幕）

用 C++ / Qt 6 实现的 Windows 11 风格 **Ctrl+Alt+Delete 安全选项屏幕**（Secure Attention Sequence）。
按下组合键后弹出全屏锁定层：锁定 / 切换用户 / 注销 / 更改密码 / 任务管理器，
右下角提供网络、无障碍与电源按钮，Esc 或「取消」退出。

支持 **X11 与 Wayland**（KDE）。

```
┌────────────────────────────────────────────────────┐
│  （毛玻璃/暗色全屏遮罩）                            │
│                安全选项                              │
│                ┌──────────────┐                     │
│                │   锁定        │                     │
│                │   切换用户     │   ← 居中菜单列      │
│                │   注销        │     （JSON 可配置） │
│                │   更改密码    │                    │
│                │   任务管理器   │                    │
│                └──────────────┘                    │
│  [取消]                 [输入法][网络][无障碍][电源]   │
└────────────────────────────────────────────────────┘
```

## 依赖软件

SAS 本体只依赖 **Qt 6（Widgets/Network/DBus）+ libxcb**；下列为**默认按钮动作**用到的外部程序，
均带 fallback 回退链——缺少某个也没关系，程序会自动尝试下一项，或由你在 `config.json` 中替换。

| 功能 | 用到的软件（按尝试顺序） | 包名示例（Arch / Debian） |
|---|---|---|
| 任务管理器 | **missioncenter** → gnome-system-monitor → ksysguard → xfce4-taskmanager | `missioncenter` / `mission-center` |
| 更改密码 | `passwd` | `xdg-utils`、`kitty`… / 同上 |
| 锁定 / 切换用户 / 注销 | `loginctl`（systemd） | `systemd`（系统自带） |
| 注销（fallback） | qdbus6/qdbus（KDE KSMServer）→ gnome-session-quit → niri msg → swaymsg → hyprctl | 随对应桌面环境安装 |
| 电源（睡眠/关机/重启） | `systemctl`（systemd） | `systemd`（系统自带） |
| 网络面板 | **NetworkManager**（`nmcli`，首选）或 **iwd**（D-Bus） | `networkmanager` / `iwd` |
| 无障碍 | 目前是滚木 |  |

> 任务管理器建议安装 [Mission Center](https://missioncenter.io)：系统级实时监控，功能最接近 Windows 任务管理器。
> 网络面板二选一后端即可：两者都有时优先 NetworkManager（Arch 上常见组合为 NM 主控 + iwd 驱动 Wi-Fi，仍走 nmcli）。

---

## 功能特性

| 特性 | 实现方式 |
|---|---|
| 全屏置顶覆盖 | `Qt::FramelessWindowHint + WindowStaysOnTopHint + showFullScreen()` + `raise()`；X11 加 `X11BypassWindowManagerHint`；KDE 写 `keepabove` 窗口规则，确保盖住桌面与所有窗口 |
| 毛玻璃背景 | X11：截取当前屏幕 → `QGraphicsBlurEffect` 模糊 + 暗色遮罩；KDE/Hyprland 可用混成器原生 blur（见下文） |
| 按键拦截 | X11：`QWidget::grabKeyboard()` 全键盘抢占；Wayland：全屏窗口 + 混成器焦点（协议限制，见 FAQ） |
| 全局热键 | X11：`xcb_grab_key` 独立连接监听；Wayland：混成器快捷键转发 `sas-screen --show`（冷启动） |
| 应用内网络面板 | 右下角「网络」打开内置 Wi-Fi 切换面板：自动识别 NetworkManager（nmcli）或 iwd（D-Bus），列出附近网络、输密码连接、断开 |
| 退出 | Esc 或「取消」按钮 |
| 单实例 | QLocalServer / QLocalSocket IPC，重复触发由同一进程弹窗 |
| 外部命令解耦 | `config/sas-screen.json` 定义按钮 → 命令映射，`QProcess::startDetached` 执行 |

---

## 一键安装 / 卸载（自动检测 DE/WM）

**一条命令完成全部安装与绑定**：编译 → 安装 → mask systemd target → 自动检测当前
桌面环境/窗口管理器并完成对应的快捷键与毛玻璃配置。**不写入任何自启动项**——快捷键统一
绑定 `sas-screen --show` 按需冷启动，登录后不会自动打开界面。

```bash
sudo ./deploy/install.sh   # 系统级：安装到 /usr/local（用户级配置自动交给原用户执行）
```

安装脚本自动识别以下环境并直接配置，**无需再手动操作**：

| 检测到 | 自动完成 |
|---|---|
| X11| 无需配置 —— 运行中的实例自动 `xcb_grab_key` 注册全局热键 |
| KDE Plasma | 写入 `kwinrulesrc` 毛玻璃 + **keepabove 置顶**规则 + 启用脚本 |

检测依据：`XDG_CURRENT_DESKTOP` / `XDG_SESSION_TYPE` → 进程名（`gnome-shell`、`plasmashell`、
`Hyprland`、`sway`、`niri`）→ 配置文件存在性（`~/.config/{hypr,sway,niri}`）。配置块带
`# >>> SAS Screen <<<` 标记（Niri 用 KDL 格式，为 `// >>> SAS Screen <<<`），重复安装自动替换、卸载自动清除。脚本可重复执行（幂等）。

手动编译安装：

```bash
# 编译
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
# 运行
./build/sas-screen --help
# 手动安装（不写自启：按快捷键时 --show 冷启动）
cmake --install build --prefix ~/.local
mkdir -p ~/.local/share/sas-screen
cp config/sas-screen.json ~/.local/share/sas-screen/
```

### 卸载

```bash
# 移除一切：二进制、WM 配置块
sudo ./deploy/uninstall.sh   # 若当初用 root 安装
```

卸载脚本还会：
- 将你自定义的 `~/.config/sas-screen/config.json` 备份为 `config.json.bak`；
- 询问是否恢复 `ctrl-alt-del.target`（Ctrl+Alt+Delete 重新触发重启），默认不恢复。

> 依赖：**CMake ≥ 3.16、Qt 6（Widgets + Network）、libxcb**（X11 会话需要；Wayland 仅用 Qt 时可省略）。

---

## 屏蔽 systemd 默认行为

Linux 内核把 Ctrl+Alt+Delete 转成 SIGINT 发给 PID 1，systemd 默认响应为**重启**。
不屏蔽的话，即使界面弹出，松开按键也会触发重启：

```bash
sudo systemctl mask ctrl-alt-del.target
sudo systemctl daemon-reload
# 恢复：sudo systemctl unmask ctrl-alt-del.target
```

或直接运行 `sudo ./deploy/deny-systemd-cad.sh`。

---

## 会话适配总览

| 会话 | 触发方式 | 焦点抢占 | 毛玻璃 |
|---|---|---|---|
| **X11** | 程序自动 `xcb_grab_key`（零配置） | `grabKeyboard()` 全键盘 | 内置截屏模糊 |
| **KDE (Wayland/X11)** | KWin 脚本或系统设置快捷键 | 全屏窗口 | KWin Blur 窗口规则 |

> Wayland 无全局键抓取协议（global shortcuts 协议尚未普及），因此一律由混成器把 Ctrl+Alt+Delete
> 转成 `sas-screen --show`；程序内部已做单实例，重复触发只弹窗、不重复启动。
> 全屏 + 置顶：KDE 通过 `keepabove` 窗口规则、Hyprland 通过 `windowrule = keepabove` 强制盖住
> 桌面与所有窗口；Sway/Niri 的全屏窗口天然置顶。

---

## X11 会话（零配置）

X11 下无需任何配置：运行中的实例自动在 root 窗口注册 Ctrl+Alt+Delete 被动抓取，
按下即弹窗；界面上 `grabKeyboard()` 抢占全键盘（Alt+Tab 等 WM 快捷键也会被锁住）。
未识别 DE 时不写自启，请把 `sas-screen` 加进你自己的登录启动器；DE 场景下快捷键即点即开。

```bash
sas-screen                 # 后台运行（注册 X11 全局热键）
sas-screen --show          # 手动触发一次（未运行时也会自行启动并弹窗）
```

注意事项：

- 若桌面环境占用了 Ctrl+Alt+Delete（如 KDE 默认的注销），需先在系统设置中删除该快捷键，否则抓取失败（启动日志会提示）。
- 已 mask `ctrl-alt-del.target` 后，该组合键不会再有副作用。

---

### 其他平铺 WM（通用思路）

凡是能"全局快捷键 → 执行命令"的组合键机制都能接入，要点只有一个：
**绑定 `sas-screen --show`**（冷启动自带单实例与弹窗，无需常驻进程）。
例如 river / wayfire / labwc 等均可照此办理。

---

## 配置：按钮 → 命令映射

配置文件查找顺序：`--config <file>` → `~/.config/sas-screen/config.json`
→ `<安装目录>/../share/sas-screen/sas-screen.json` → 内置默认值。

```json
{
    "buttons": {
        "锁定":       { "command": "loginctl", "args": ["lock-session"], "exitAfter": true },
        "切换用户":   { "command": "loginctl", "args": ["lock-session"], "exitAfter": true },
        "注销":       { "command": "loginctl", "args": ["terminate-session", "self"],
                        "fallback": [
                            { "command": "qdbus6", "args": ["org.kde.ksmserver", "/KSMServer", "logout", "0", "0", "0"] },
                            { "command": "gnome-session-quit", "args": ["--logout", "--no-prompt"] },
                            { "command": "niri", "args": ["msg", "action", "quit"] },
                            { "command": "swaymsg", "args": ["exit"] },
                            { "command": "hyprctl", "args": ["dispatch", "exit"] }
                        ], "exitAfter": true },
        "更改密码":   { "command": "xdg-terminal-exec", "args": ["passwd"],
                        "fallback": [
                            { "command": "kitty", "args": ["-e", "passwd"] },
                            { "command": "gnome-terminal", "args": ["--", "passwd"] },
                            { "command": "konsole", "args": ["-e", "passwd"] }
                        ], "exitAfter": true },
        "任务管理器": { "command": "missioncenter", "args": [],
                        "fallback": [
                            { "command": "gnome-system-monitor", "args": [] },
                            { "command": "ksysguard", "args": [] },
                            { "command": "xfce4-taskmanager", "args": [] }
                        ], "exitAfter": true }
    },
    "footer": {
        "network":       { "command": "", "args": [],
                           "注释": "空命令 → 打开应用内 Wi-Fi 切换面板（自动识别 NetworkManager/iwd）" },
        "accessibility": { "command": "plasma-open-settings", "args": ["accessibility"],
                           "fallback": [
                               { "command": "systemsettings", "args": ["kcm_access"] },
                               { "command": "gnome-control-center", "args": ["universal-access"] }
                           ] }
    }
}
```

- `command` 为空 → 该按钮不显示（`footer.network` 例外：空命令表示打开应用内网络面板）；
- `fallback`：主命令不存在时按顺序尝试备用命令；
- `exitAfter`：点击后是否自动关闭 SAS 界面（默认 `true`）；
- 「更改密码」未配置时自动使用**用户默认终端**：`$TERMINAL` → `xdg-terminal-exec` → kitty / alacritty / wezterm / foot / gnome-terminal / konsole / xfce4-terminal / xterm；
- 电源菜单（睡眠/关机/重启）为内置动作，执行 `systemctl suspend|poweroff|reboot`，不可配置。

---

## 架构

```
src/
├── main.cpp          入口：平台检测、单实例 IPC、装配
├── saswindow.h/.cpp  全屏 UI：毛玻璃背景、居中菜单、底部栏、Esc/取消
├── networkdialog.h/.cpp  应用内 Wi-Fi 切换面板（NetworkManager/iwd 后端自动识别）
├── commandmap.h/.cpp JSON 配置加载 + QProcess 外部命令执行（含 fallback）
└── x11grabber.h/.cpp X11 独立 xcb 连接：xcb_grab_key 监听 Ctrl+Alt+Delete
deploy/
├── install.sh                 一键安装：编译+安装+DE/WM 自动检测配置
├── uninstall.sh               一键卸载：清理所有痕迹
└── deny-systemd-cad.sh        mask ctrl-alt-del.target（独立脚本）
```

触发链路：

```
X11:   Ctrl+Alt+Delete ──(xcb_grab_key)──► X11Grabber::activated ─┐
                                                                   ├─► SasWindow::showSas
Wayland: 混成器快捷键 ──(spawn sas-screen --show)──► IPC ────────┘
```

模态抓取说明：X11 下用 `QWidget::grabKeyboard()`（在 Qt 自己的 X 连接上执行，
事件才会正常流入 Qt 事件循环）；热键监听则使用独立的 xcb 连接（两个连接互不干扰）。

---

## 常见问题

**Q: 按 Ctrl+Alt+Delete 没反应（X11）**
检查是否有其它程序占用了该组合键（如 KDE 注销快捷键）；日志会输出失败原因（`journalctl --user -u sas-screen` 或直接运行观察 stderr）。

**Q: 界面上毛玻璃不生效**
- KDE：确认 Blur 特效（系统设置 → 桌面特效）已启用；

**Q: 「任务管理器」点了没反应**

`missioncenter` / `gnome-system-monitor` / `ksysguard` / `xfce4-taskmanager` 均未安装 → 在 `config/sas-screen.json` 中换成你实际安装的工具，或填写你的假任务管理器命令。

**Q: 「网络」点了没反应**

正常情况下点「网络」会打开**应用内 Wi-Fi 面板**（列表、连接、断开、刷新），不依赖外部设置程序。
若面板提示"未检测到网络后端"，说明系统里既没有 NetworkManager 也没有 iwd，请安装其一后重启面板。
纯有线用户可用：面板始终可用，断开/连接仅针对 Wi-Fi。

**Q: 「注销」点了没反应**

默认链依次尝试：`loginctl terminate-session self` → KDE `ksmserver logout` → GNOME `gnome-session-quit`
→ `niri msg action quit` → `swaymsg exit` → `hyprctl dispatch exit`。
若仍无效，多半是会话管理器特殊（或你在容器/嵌套会话里），可在 `config.json` 的「注销」中改成你 DE 的退出命令。

**Q: 「无障碍」点了没反应**

`plasma-open-settings` / `systemsettings kcm_access` / `gnome-control-center universal-access` 均不可用时，
在 `config/sas-screen.json` 的 `footer.accessibility` 中换成你 DE 的无障碍设置入口。

**Q: 关机会弹 Polkit 授权？**
`systemctl poweroff` 走 Polkit，这是系统策略而非程序行为；可在 `config.json` 中把电源动作改为你偏好的命令。

**Q: Wayland 下无法真正"锁住"别的窗口**
Wayland 协议不允许应用抓取全局键盘/指针（只有混成器可以）。本程序靠全屏窗口独占，已满足绝大多数场景；这是 Wayland 的架构性限制，非 bug。

---

## 安全说明

本程序是**界面级还原**，不是真正的 Secure Attention Sequence：

- 真正的 Windows SAS 由内核（Winlogon）截获，任何用户态程序都无法伪造；
- 本程序的 Ctrl+Alt+Delete 监听运行于用户态，恶意进程理论上可模仿；
- 锁定/关机等动作依赖 `loginctl`/`systemctl` 与 Polkit 授权，请勿把它当作安全边界使用。
>>>>>>> master
