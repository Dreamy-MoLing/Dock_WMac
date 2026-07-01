# DIAGNOSIS_REPORT — Dock_WMac v0.2.4

> 生成时间: 2026-06-16 | 扫描模式: 全域诊断

---

## 健康评分: 78/100 — **良好**

### 评分结构
| 维度 | 分数 | 说明 |
|------|------|------|
| 项目结构 | 85 | 三层架构清晰，UI/Core/System 边界定义明确 |
| 代码质量 | 75 | 中上，但仍有边界违规和死代码残留 |
| 测试覆盖 | 70 | 11 套测试，但 UI 层和 IconProvider 空白 |
| CI/CD | 85 | GitHub Actions 完整，build+test+package+release |
| 文档 | 80 | README/CLAUDE/AGENTS 齐全，但有注释过期 |
| 依赖管理 | 75 | Qt6 + GTest FetchContent，但 CMakePresets 硬编码本地路径 |

---

## 项目概况

| 属性 | 值 |
|------|-----|
| 描述 | macOS 风格应用停靠栏，替代 Windows 原生任务栏 |
| 技术栈 | C++17 + Qt6.11.1 + MSVC + Google Test v1.14.0 |
| 架构 | 三层: UI → Core → System (Win32/COM/DWM) |
| 代码量 | ~8,175 行 (52 源文件，不含 build) |
| 测试 | 11 个测试可执行文件，部分含 89+ 用例 |
| Git | master 分支，7 个 tag (v0.2.0–0.2.4)，最近 30+ commits 活跃 |
| CI | GitHub Actions，windows-latest + 动态 MSVC 检测 |
| 许可 | MIT |

---

## 技术债清单

### P0 — 必须修复（功能缺陷或运行时风险）

| # | 文件 | 问题 |
|---|------|------|
| 1 | `src/core/WindowCache.cpp:337-351` | `keybd_event` → `SendInput` 已修复（最后 commit），但 Win+Tab 在安全软件/游戏模式下可能被拦截，需备选方案 |
| 2 | `src/core/DockManager.cpp:124-128` | `updateWindowCount` 对 pinned/transient 各遍历一次，O(2n) 可合并为 O(n) |
| 3 | `src/core/ClickStateMachine.cpp` | **架构边界违规**：直接调 `ShowWindow`/`SetForegroundWindow`（绕过 SysHelper）。上轮修复已将部分逻辑迁移到 SysHelper，但 ClickStateMachine 仍有独立 Win32 调用 |
| 4 | `src/ui/OverflowPanel.cpp` | 弹出菜单没有 Windows 11 圆角（`DWMWA_WINDOW_CORNER_PREFERENCE`），`WindowPreviewPanel` 有，风格不一致 |

### P1 — 应在下次迭代处理（代码异味/潜在问题）

| # | 文件 | 问题 |
|---|------|------|
| 5 | `src/ui/DockWindow.cpp:343` | `enterEvent` 已被清为空桩（dock hide 后 mouse event 到不了 widget，原逻辑已删除）。空覆写无实际作用，纯代码噪声 |
| 6 | `src/ui/DockWindow.cpp` | `relayoutItems()` 内部已调 `updatePosition()`，但 `onItemAdded` 后又额外调了一次，重复调用 |
| 7 | `src/core/IconProvider.cpp:32-36` | ✅ 已修复：`resolveShortcut` 委托给 `SysHelper::resolveShortcut()`，无重复代码 |
| 8 | `src/core/ConfigManager.cpp:119-123` | **Debounce bug**：`set()` 同时调了 `save()`（立即写盘）+ `m_saveTimer->start()`，导致 timer 到点后重复调 `save()`。应将 `save()` 移出 `set()`，仅由 timer 触发 |
| 9 | `src/core/ConfigManager.h:57` | `QCache<QString, QPixmap>` LRU 缓存 128 上限 — 无自动清理回调，内存峰值不可控（每个 QPixmap 可能数 MB） |
| 10 | `src/core/DockManager.cpp` | DWM Peek 用 `SetWindowPos(HWND_TOP)` — 全屏看视频时预览聊天窗口会把聊天窗口推到最前，破坏沉浸体验 |
| 11 | `include/core/ConfigManager.h` | 注释写 `%APPDATA%/Dock_WMac/`，实际走便携模式 `./data/`，注释过期 |

### P2 — 低优先级/未来优化

| # | 文件 | 问题 |
|---|------|------|
| 11 | `src/main.cpp` | 极小，仅 14 行，但无命令行参数处理（如 `--help`, `--version`, `--config-path`） |
| 12 | `src/ui/DockWindow.cpp` | `nativeEvent` 中包含 `xcb_generic_event_t` 分支 — Windows-only 项目的死代码（XCB 是 Linux/X11 的） |
| 13 | `tests/` | 测试覆盖缺口：`IconProvider`(5 级回退链)、`DockItem`(paint 逻辑)、`DockWindow`(UI 事件流)、`WindowPreviewPanel`(DWM 缩略图生命周期)、`Logger`(轮转/格式) |
| 14 | `CMakePresets.json` | `default` preset 硬编码 `C:/Program Files/Microsoft Visual Studio/18/Insiders` — 换机/他人构建必须用 `ci` preset 或手动修改 |
| 15 | `src/core/Logger.cpp` | 日志轮转仅保留一份 `.old` 备份，无时间戳归档或自动清理策略 |
| 16 | `include/core/Types.h` | `DockItemData` 缺少 `==` 或 `qHash` 重载（无法用于 QSet 或作为 QHash key） |
| 17 | 全局 | 无异常处理封装（Win32 API 调用无 SEH / 错误恢复路径） |
| 18 | 全局 | 无 crash dump 或远程日志上报（用户环境问题难以诊断） |

---

## 遗留问题盲区

以下能力是当前项目缺失但桌面软件必备的：

| 能力 | 说明 |
|------|------|
| **错误边界处理** | Win32 API (OpenProcess/EnumWindows/CoCreateInstance) 调用无结构化异常处理（SEH），API 失败后可能静默崩溃 |
| **多显示器支持** | `hideNativeTaskbar` 已修复副屏（`Shell_SecondaryTrayWnd`），但 `DockManager::monitorIndex` 切换逻辑在运行时是否动态响应显示器插拔？未测试 |
| **日志聚合能力** | Logger 只有文件 + stderr 输出，无远程日志 / 指标上报通道。生产环境黑盒诊断困难 |
| **自动更新** | 无更新检查机制。用户必须手动访问 GitHub Releases 下载新版 |
| **性能 profiler** | `ProcessMonitor` 每 2s `CreateToolhelp32Snapshot`，`WindowCache` 的 `refresh` 全量 `EnumWindows` — 在大量窗口/低配机上可能产生可感知的卡顿 |
| **高 DPI 适配状态** | Qt6 自带高 DPI 支持，但 Dock 的鱼眼缩放在 4K+ 高分屏上的像素对齐未验证 |
| **安全考虑** | 无任何安全边界：Dock 运行在当前用户上下文，不做输入过滤。这不是问题但应明确记录为设计选择 |

---

## 用户画像匹配度

**推断的用户场景**：Windows 用户，不满足于原生任务栏的交互，希望获得 macOS Dock 式的鱼眼缩放 + 自动隐藏 + 窗口预览体验。

**核心使用路径**：
1. 启动 → 自动隐藏原生任务栏 → 显示 Dock
2. 鼠标触底/悬停 → Dock 唤醒 → 鱼眼放大
3. 点击图标 → 启动应用 / 切换窗口 / 最小化（5 状态状态机）
4. 窗口预览 → 悬停 500ms → DWM 缩略图 → 再悬停 Peek
5. 右键菜单 → 固定/取消固定/退出

**功能匹配评估**：
| 高频需求 | 实现状态 | 评估 |
|-----------|----------|------|
| 启动常用应用 | ✅ 固定项 + 系统任务栏导入 | 成熟 |
| 切换运行中的窗口 | ✅ 5 状态点击机 | 成熟 |
| 鱼眼缩放视觉效果 | ✅ DockAnimation 高斯缩放 | 成熟 |
| 自动隐藏/唤醒 | ✅ 全屏检测 + 3s 延迟 + Win 键冷却 | 成熟 |
| 窗口预览缩略图 | ✅ DWM DwmRegisterThumbnail | 成熟 |
| 亮暗主题跟随 | ✅ 注册表每 5s 轮询 | 成熟 |
| 拖拽排序 | ✅ DockWindow_itemmanager | 成熟 |
| 溢出面板 | ✅ OverflowPanel | 成熟 |
| 运行指示器/角标 | ✅ DockItem paint | 成熟 |
| **缺少的高频需求** | | |
| 应用右键菜单 | ❌ 未实现（关闭窗口、新窗口等） | 用户预期缺口 |
| 日期/时间显示 | ❌ 原生任务栏有此功能 | 用户可能怀念 |
| 系统托盘图标 | ❌ | 退出/设置入口缺失 |
| 触摸屏支持 | ❌ | 小众人群，可押后 |

---

## 最近迭代轨迹

| 周期 | 版本 | 主要变化 |
|------|------|----------|
| 2026-06-14 | v0.2.4 | 便携模式 + 图标管道重写 + 固定项管理重写 + CI 全面工具化 |
| 2026-06-15+ | code review | 14 项修复：enterEvent 死代码删除、架构边界(ClickStateMachine→SysHelper)、ConfigManager debounce、UWP 路径修复、副屏任务栏支持 |
| 2026-06-16 | 当前 | DockAnimation 抽取 + 截图模式 + 视觉参数可配置化 |

---

## 推荐的下一原子任务

根据代码审查遗留问题和用户高频需求，建议下一迭代从以下之一切入：

1. **P0: OverflowPanel Windows 11 圆角** — 简单，1h 内，`DwmSetWindowAttribute(DWMWA_WINDOW_CORNER_PREFERENCE)` 调用已有先例
2. **P1: ClickStateMachine Win32 边界验收测试** — 架构边界落地的最后一块，验证上次 commit 是否彻底
3. **测试补全: test_icon_provider.cpp** — 5 级回退链边界条件，补齐覆盖缺口

---

*状态标记: INIT_DIAG — 等待进入阶段 1 循环*
