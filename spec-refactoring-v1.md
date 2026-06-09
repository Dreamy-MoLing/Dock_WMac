# Dock_WMac — Refactoring Specification v1.0

> **分支:** `hermes`（已创建）
> **目标:** 修复 3 个 UX 问题，精简视觉元素，改进 Dock 显隐逻辑
> **实现工具:** Claude Code（Anthropic 编码 Agent）

---

## 总览

本 spec 定义了三个独立但相互关联的修改模块。每个模块有自己的分析、目标状态和任务清单。

| # | 模块 | 难度 | 涉及文件数 | 风险 |
|---|------|------|-----------|------|
| 1 | 窗口预览列表过滤 | 低 | 1-2 | 低 |
| 2 | 多窗口指示器精简 | 低 | 1 | 低 |
| 3 | Dock 显隐逻辑重写 | 中-高 | 3-4 | 中 |

---

## 模块 1: 窗口预览列表过滤

### 问题描述

悬停 Dock 图标时，预览面板弹出多个窗口缩略图，但实际上该应用"只有一个窗口是存在的"。根源在 `WindowCache::getWindowsForPreview()` 的过滤条件太宽松。

### 根因分析

**文件:** `src/core/WindowCache.cpp:278-287`

```cpp
WindowList WindowCache::getWindowsForPreview(const QString &wmClass)
{
    // ...
    std::copy_if(it->begin(), it->end(), std::back_inserter(result),
        [](const CachedWindowInfo &w) { return !w.isToolWindow && IsWindow(w.hwnd); });
    return result;
}
```

**问题:** 只过滤了 `isToolWindow` 和无效 HWND，**没有过滤真正对用户有意义的可见窗口**。导致：

1. **后台辅助窗口**被包含（无标题、不可见但 `!isToolWindow` 的窗口）
2. **子窗口**被包含（某些应用创建多个不可见子窗口）
3. **已隐藏但未销毁的窗口**仍然出现

### 目标状态

预览面板只显示用户"真正关心"的窗口：
- 已显示的窗口（`isVisible == true`）
- 已最小化的窗口（`isMinimized == true`）
- **排除:** 无标题窗口、不可见且非最小化的窗口、ToolWindow

### 修改

#### 文件: `src/core/WindowCache.cpp`

**修改 `getWindowsForPreview()` 的第 284 行的 lambda 过滤条件**

```
旧条件: !w.isToolWindow && IsWindow(w.hwnd)
新条件: !w.isToolWindow && !w.title.trimmed().isEmpty() && (w.isVisible || w.isMinimized) && IsWindow(w.hwnd)
```

**验证:**
1. 启动一个只有单个窗口的应用（如记事本）→ 预览面板只显示 1 个缩略图
2. 启动有后台进程的应用（如 Chrome，可能有多个辅助窗口）→ 预览面板只显示可见/最小化的选项卡窗口
3. 最小化窗口 → 预览面板仍然显示它（可点击恢复）

---

## 模块 2: 多窗口指示器精简

### 问题描述

"有激活窗口的应用图标旁的'多视窗'图标碍眼"。当前实现在 `DockItem` 右下角绘制多层堆叠矩形。

### 根因分析

**文件:** `src/ui/DockItem.cpp:238-257`

```cpp
if (m_windowCount > 1) {
    int layers = qMin(m_windowCount - 1, 3);
    int layerW = drawSize / 3;
    int layerH = drawSize / 4;
    int offsetX = 4;
    int baseX = width() - layerW - 2;
    int baseY = height() - layerH - 8;
    // 绘制 3 层半透明白框
    for (int i = layers - 1; i >= 0; --i) {
        // ... draw layered rectangles
    }
}
```

它在图标右下角画了多层半透明白色小矩形。用户反馈：
- 视觉上过于显眼/杂乱
- 与 Dock 整体极简风格不匹配

### 设计决策

| 方案 | 描述 | 选择 |
|------|------|------|
| A. 完全移除 | 不显示任何多窗口指示器 | ✅ **选择** |
| B. 改用数字角标 | 在 badge 区域显示 "x3" | ❌ 与未读徽章冲突 |
| C. 微缩为小点 | 把矩形换成更小的圆点 | ❌ 仍然视觉杂乱 |

**选择方案 A** — 完全移除多窗口堆叠指示器。理由：
- 运行指示灯（绿点）已足够表示"应用在运行"
- 预览面板（hover 500ms）才是查看多个窗口的正确入口
- 移除后图标更干净，符合 macOS 风格哲学

### 修改

#### 文件: `src/ui/DockItem.cpp`

**删除第 238-257 行（整个 `if (m_windowCount > 1)` 块）**

同时也可考虑清理相关但不再必要的成员：

#### 文件: `src/ui/DockItem.h`

**可选清理:** `m_windowCount` 字段仍然需要用于 `triggerInteractionIndicator()` 和 `setWindowCount()`，但 `paintEvent` 中不再绘制堆叠效果。

#### 文件: `src/core/WindowCache.cpp`

**`getWindowCount()` 的逻辑保持不变** — `m_windowCount` 仍需供 `setWindowCount(int)` 信号使用（click state machine 等可能依赖），只是 UI 层不再绘制它。

### 验证

1. 启动多个应用 → 多窗口图标应该变得干净，不再有右下角堆叠矩形
2. 右下角只有运行指示灯（绿色渐隐圆点）或未读徽章（红色）
3. 多窗口的功能（预览面板、点击切换）不受影响

---

## 模块 3: Dock 显隐逻辑重写

### 问题描述

当前的逻辑"反常识"——用户期望：

```
┌─────────────────────────────────────────────────────────────┐
│ 条件                            │ Dock 行为                  │
├─────────────────────────────────────────────────────────────┤
│ 所有窗口非最大化/全屏            │ 置顶显示，绝不隐藏         │
│ 主屏幕存在最大化/全屏窗口        │ 默认隐藏                   │
│ 从"有全屏"切换到"无全屏"        │ 自动弹出显示               │
│ 从"无全屏"切换到"有全屏"        │ 自动隐藏                   │
└─────────────────────────────────────────────────────────────┘
```

### 根因分析

**当前问题:**

1. `SysHelper::getForegroundWindowState()` 只检查**前台窗口** — 但用户期望检查**主屏幕上是否有任何最大化的窗口**
2. `DockManager::onForegroundWindowChanged(bool)` — 布尔参数太粗糙，丢失了上下文信息
3. 隐藏后需要手动（鼠标触碰底部 / Win 键）才能唤回 — 用户期望状态变化时自动弹出

**当前调用链:**

```
WinEvent (EVENT_SYSTEM_FOREGROUND / MAXIMIZESTART)
  → SysHelper::WinEventProc
    → emit foregroundWindowChanged(isMaximized)
      → DockManager::onForegroundWindowChanged(bool)
        → 如果最大化 && Docked → Hidden
        → 如果非最大化 && Hidden → Docked
```

### 新架构设计

```
┌─────────────────────────────────────────────────────────────────┐
│                    新状态机逻辑                                    │
│                                                                   │
│   idle (Docked) ──── 有窗口最大化/全屏 ────→ hidden               │
│   ↑                                              │               │
│   └─────── 所有最大窗口都恢复 ───────────────────┘               │
│                                                                   │
│   hidden ──── Win键/鼠标触底 ────→ Docked（手动唤回）              │
└─────────────────────────────────────────────────────────────────┘
```

**核心变化:**
- 显隐判断条件从"前台窗口是否最大化"改为"主屏幕是否存在最大化窗口"
- 补充 `SysHelper` 接口以检测主屏幕上是否有任何最大化/全屏窗口
- 移除 `isTaskbarAutoHideEnabled()` 的依赖（新逻辑不依赖任务栏设置）

### 详细修改方案

#### 3.1 新增系统查询能力

**文件:** `src/core/SysHelper.h` 和 `src/core/SysHelper.cpp`

新增方法:

```cpp
// SysHelper.h
/**
 * @brief 检查主屏幕（或指定屏幕）上是否存在最大化或全屏的窗口
 * @param monitorIndex -1 表示主屏幕，0+ 表示指定显示器
 * @return true 如果存在任何最大化/全屏窗口
 */
bool hasMaximizedOrFullscreenWindowOnMonitor(int monitorIndex = -1);
```

```cpp
// SysHelper.cpp 实现
bool SysHelper::hasMaximizedOrFullscreenWindowOnMonitor(int monitorIndex)
{
    // 1. 确定目标显示器
    HMONITOR hMonitor = nullptr;
    if (monitorIndex < 0) {
        // 主显示器
        POINT pt = {0, 0};
        hMonitor = MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY);
    } else {
        // 按索引查找显示器（略复杂，可用 MonitorFromWindow + EnumDisplayMonitors）
        // 简化：使用 EnumDisplayMonitors 找到第 monitorIndex 个显示器
    }

    // 如果没有指定显示器或找不到，回退到主显示器
    if (!hMonitor) {
        POINT pt = {0, 0};
        hMonitor = MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY);
    }

    // 获取显示器尺寸（rcMonitor，不是 rcWork）
    MONITORINFO mi;
    mi.cbSize = sizeof(MONITORINFO);
    if (!GetMonitorInfo(hMonitor, &mi)) return false;
    int screenW = mi.rcMonitor.right - mi.rcMonitor.left;
    int screenH = mi.rcMonitor.bottom - mi.rcMonitor.top;

    // 2. EnumWindows 检查所有窗口
    bool found = false;
    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        auto *result = reinterpret_cast<bool *>(lParam);
        if (!IsWindowVisible(hwnd) || IsIconic(hwnd)) return TRUE;

        // 检查窗口是否在目标显示器上
        HMONITOR winMonitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONULL);
        if (winMonitor != reinterpret_cast<HMONITOR>(lParam)) {
            // 实际上我们需要传递 monitor 作为第二个参数...要重构
            return TRUE;
        }

        // 检查最大化状态
        WINDOWPLACEMENT wp;
        wp.length = sizeof(WINDOWPLACEMENT);
        if (GetWindowPlacement(hwnd, &wp) && wp.showCmd == SW_SHOWMAXIMIZED) {
            *result = true;
            return FALSE;  // 找到即停止
        }

        // 检查全屏
        // ...
        return TRUE;
    }, reinterpret_cast<LPARAM>(&found));

    return found;
}
```

> ⚠️ **注意:** 上面的 EnumWindows 需要把 HMONITOR 传进回调。需要用 struct 包装两个参数（bool *found 和 HMONITOR hMonitor），避免闭包转换问题。

#### 3.2 简化 DockManager 状态机

**文件:** `src/core/DockManager.h` 和 `src/core/DockManager.cpp`

**移除** `onForegroundWindowChanged(bool)` 方法

**新增** 两个独立方法:

```cpp
// DockManager.h — 新增
public slots:
    /** @brief 处理主屏幕最大化状态变化（新信号源） */
    void onFullscreenStateChanged(bool anyMaximizedOnPrimary);

    /** @brief 处理 Win 键按下事件（保持不变） */
    void onWinKeyPressed();
```

```cpp
// DockManager.cpp — 新实现
void DockManager::onFullscreenStateChanged(bool anyMaximizedOnPrimary)
{
    if (anyMaximizedOnPrimary && m_currentState == DockState::Docked) {
        // 存在最大化窗口 → 隐藏
        m_sysHelper->installKeyboardHook();
        m_currentState = DockState::Hidden;
        emit stateChanged(DockState::Hidden);

    } else if (!anyMaximizedOnPrimary && m_currentState == DockState::Hidden) {
        // 所有最大化窗口都已恢复 → 自动弹出
        m_sysHelper->uninstallKeyboardHook();
        m_currentState = DockState::Docked;
        emit stateChanged(DockState::Docked);
    }
}
```

#### 3.3 修改 SysHelper 窗口事件回调

**文件:** `src/core/SysHelper.cpp`

**问题:** 当前回调 `WinEventProc` 在 `EVENT_SYSTEM_FOREGROUND / MINIMIZESTART / MAXIMIZESTART` 时检查**前台窗口**状态。需要改为检查**主屏幕全局**状态。

高频方案（推荐）:
```
方案 A: 状态变化时延迟触发全局检查
  任何 WINDOW MAXIMIZESTART / MINIMIZESTART / RESTORE 事件
    → 延迟 200ms（防抖）
    → 调用 hasMaximizedOrFullscreenWindowOnMonitor()
    → 发射新信号

方案 B: 轮询（不推荐，资源浪费）
```

**具体修改:**

1. 将 `SysHelper` 的信号从 `foregroundWindowChanged(bool)` 替换为 `fullscreenStateChanged(bool anyMaximizedOnPrimary)`
   - 或者保留旧信号并新增一个信号（向后兼容，但注意 DockManager 需要切换连接）

2. 修改 `WinEventProc`:

```cpp
// WinEventProc 中处理事件聚合 + 防抖
static QTimer *g_debounceTimer = nullptr;  // 或者作为 SysHelper 成员

case EVENT_SYSTEM_MAXIMIZESTART:
case EVENT_SYSTEM_MINIMIZESTART:
case EVENT_SYSTEM_FOREGROUND:  // 前台窗口变化可能影响最大化状态
    // 延迟一帧执行全量扫描
    if (g_sysHelperForHook) {
        QMetaObject::invokeMethod(g_sysHelperForHook, []() {
            bool anyMax = g_sysHelperForHook->hasMaximizedOrFullscreenWindowOnMonitor();
            emit g_sysHelperForHook->fullscreenStateChanged(anyMax);
        }, Qt::QueuedConnection);
    }
    break;
```

3. 添加 200ms 防抖（避免高频事件触发重复扫描）:

```cpp
// 在 SysHelper 中添加成员
QTimer *m_fullscreenDebounceTimer;

// 初始化
m_fullscreenDebounceTimer = new QTimer(this);
m_fullscreenDebounceTimer->setSingleShot(true);
m_fullscreenDebounceTimer->setInterval(200);
connect(m_fullscreenDebounceTimer, &QTimer::timeout, this, [this]() {
    bool anyMax = hasMaximizedOrFullscreenWindowOnMonitor();
    emit fullscreenStateChanged(anyMax);
});
```

```cpp
// WinEventProc 中：
case EVENT_SYSTEM_MAXIMIZESTART:
case EVENT_SYSTEM_MINIMIZESTART:
case EVENT_SYSTEM_FOREGROUND:
    if (g_sysHelperForHook) {
        // 重启防抖定时器（只会触发一次）
        QMetaObject::invokeMethod(g_sysHelperForHook, [=]() {
            g_sysHelperForHook->findChild<QTimer*>("fullscreenDebounceTimer")->start();
            // 实际上应该用成员变量直接访问，上面是示意
        }, Qt::QueuedConnection);
    }
    break;
```

#### 3.4 更新 DockWindow 底部边缘唤起逻辑

**文件:** `src/ui/DockWindow.cpp:122-143`

当前底部边缘唤起逻辑依赖 `isTaskbarAutoHideEnabled()`，在新设计中需要**独立于任务栏设置**工作。

修改 `m_bottomEdgeTimer` 的 lambda:

```cpp
// 修改前：
if (!m_sysHelper->isTaskbarAutoHideEnabled()) return;

// 修改后：移除这个限制！Dock 隐藏时总是响应鼠标触底
```

#### 3.5 更新 Application 连接

**文件:** `src/core/Application.cpp`

```cpp
// 修改前：
connect(m_sysHelper, &SysHelper::foregroundWindowChanged,
        m_dockManager, &DockManager::onForegroundWindowChanged);

// 修改后：
connect(m_sysHelper, &SysHelper::fullscreenStateChanged,
        m_dockManager, &DockManager::onFullscreenStateChanged);
```

### 状态变化时序图

```
时间线:
│
├─ 用户打开记事本 → 非最大化 → Dock 置顶显示 [Docked]
│
├─ 用户最大化 Chrome 窗口 → hasMaximizedOrFullscreenWindowOnMonitor() = true
│   └─ DockManager → DockState::Hidden → Dock 滑出隐藏
│
├─ 用户恢复 Chrome（非最大化）→ 全屏扫描 → false
│   └─ DockManager → DockState::Docked → Dock 自动滑入显示
│
├─ 用户打开全屏视频（浏览器全屏）→ 扫描 → true → Hidden
│
├─ 用户 Alt+Tab 到全屏窗口 → 扫描 → true → 保持 Hidden
│
└─ 用户退出全屏/恢复 → 扫描 → false → Docked（自动弹出）
```

### 验证清单

- [ ] 无最大化窗口 → Dock 置顶显示，永不自动隐藏
- [ ] 最大化一个窗口 → Dock 自动隐藏（滑出动画）
- [ ] 恢复该窗口 → Dock 自动弹出（滑入动画）
- [ ] 全屏应用（媒体播放器/游戏/浏览器 F11）→ Dock 自动隐藏
- [ ] 退出全屏 → Dock 自动弹出
- [ ] 隐藏期间，鼠标触底 → Dock 手动唤回
- [ ] 隐藏期间，Win 键 → Dock 手动唤回
- [ ] 多个显示器场景：主显示器最大化 → 隐藏；副显示器最大化 → 考虑是否也隐藏

---

## 实施优先级与依赖

```
模块 1（预览过滤） ← 无依赖，先做
模块 2（指示器精简） ← 无依赖，和模块 1 并行
模块 3（显隐逻辑） ← 依赖模块 2 完成后再开始（涉及同一组件，减少冲突）
```

**建议实现顺序:** 模块 1 → 模块 2 → 模块 3

---

## 附录 A: 涉及文件清单

| 文件 | 模块 | 修改类型 |
|------|------|----------|
| `src/core/WindowCache.cpp` | M1 | 小改（lambda 条件） |
| `src/ui/DockItem.cpp` | M2 | 小改（删除代码块） |
| `include/core/SysHelper.h` | M3 | 新增方法声明 |
| `src/core/SysHelper.cpp` | M3 | 新增方法 + 修改 WinEvent 回调 |
| `include/core/DockManager.h` | M3 | 修改槽声明 |
| `src/core/DockManager.cpp` | M3 | 重写状态机方法 |
| `src/ui/DockWindow.cpp` | M3 | 修改底部边缘逻辑 |
| `src/core/Application.cpp` | M3 | 修改信号连接 |

## 附录 B: 切勿修改的内容

- **Logger, ConfigManager, ProcessMonitor** — 与本任务完全无关
- **PinnedItemsReader, IconProvider** — 与本任务完全无关
- **DockItem 的拖拽排序逻辑** — 不要动
- **鱼眼动画算法** — 不要动
- **毛玻璃 / 主题切换** — 不要动
- **CMakeLists.txt / 构建系统** — 不要动
- **Types.h** — 不要动 `DockState` 枚举定义
- **AppIdHelper** — 不要动
- **ClickStateMachine** — 不要动
- **installer/** — 不要动
