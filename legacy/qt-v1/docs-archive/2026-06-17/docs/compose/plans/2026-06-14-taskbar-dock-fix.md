# Taskbar & Dock 功能修正计划

> **For agentic workers:** Use compose:execute to implement this plan task-by-task.

**Goal:** 修正点击行为使其与Windows原生任务栏一致，改进Mac Dock风格的预览窗口交互，删除不必要的双击功能

**Architecture:** 修复ClickStateMachine的5状态逻辑，改进WindowPreviewPanel的延迟隐藏机制，删除DockWindow中的双击处理

**Tech Stack:** C++17, Qt6, Win32 API (DWM, SetForegroundWindow)

---

## 行为规范（基于官方文档）

### Windows任务栏点击行为
| 状态 | 点击行为 |
|------|----------|
| 应用未运行 | 启动应用 |
| 应用运行，窗口在前台 | 最小化窗口 |
| 应用运行，窗口在后台 | 激活窗口（带到前台） |
| 应用运行，所有窗口最小化 | 恢复最近活动窗口 |

**注意：** Windows任务栏**没有双击功能**，双击与单击行为相同。

### Mac Dock点击行为
| 操作 | 行为 |
|------|------|
| 单击 | 打开/切换到应用 |
| Option-click | 切换到上一个应用并隐藏当前应用 |
| Control-click | 显示快捷菜单 |

**项目目标：** 外观/动画模仿Mac Dock，点击逻辑与Windows任务栏一致

---

## 问题诊断

### 问题1：点击图标没有正确最小化
**可能原因：**
- ClickStateMachine的ForegroundActive状态判断不准确
- WindowCache的isForegroundApp()检测有误
- SetForegroundWindow()调用失败（需要检查AllowSetForegroundWindow）

### 问题2：部分图标点击后启动新实例而非激活窗口
**可能原因：**
- ClickStateMachine的NoWindows状态误判（应用实际在运行但窗口未被检测到）
- wmClass匹配失败导致状态机无法找到窗口
- ProcessMonitor的窗口检测延迟

### 问题3：双击功能不符合Windows行为
**当前实现：** 双击启动新实例
**正确行为：** 双击与单击相同（Windows任务栏无双击功能）

### 问题4：预览窗口交互不理想
**已修复：** 添加了startDelayedHide()延迟隐藏
**待验证：** 鼠标从Dock移向预览窗口时的交互

---

## Task 1: 修复ClickStateMachine的窗口激活逻辑

**Files:**
- Modify: `src/core/ClickStateMachine.cpp:108-130` (showHiddenWindow)
- Modify: `src/core/ClickStateMachine.cpp:132-141` (restoreLastActive)
- Modify: `src/core/ClickStateMachine.cpp:153-161` (bringToForeground)

- [ ] **Step 1: 检查showHiddenWindow实现**

当前实现：
```cpp
void ClickStateMachine::showHiddenWindow(const QString &wmClass)
{
    WindowList allWindows = m_cache->getWindowsForPreview(wmClass);
    HWND target = nullptr;
    DWORD bestTime = 0;
    for (const auto &w : allWindows) {
        if (!w.isVisible && !w.isMinimized && !w.isToolWindow && IsWindow(w.hwnd)) {
            if (!target || w.lastActiveTime > bestTime) {
                target = w.hwnd;
                bestTime = w.lastActiveTime;
            }
        }
    }
    if (target) {
        ShowWindow(target, SW_SHOW);
        AllowSetForegroundWindow(ASFW_ANY);
        SetForegroundWindow(target);
    }
}
```

**问题：** 条件 `!w.isVisible && !w.isMinimized` 可能过于严格，应该只检查 `!w.isVisible`

- [ ] **Step 2: 修复showHiddenWindow条件**

```cpp
void ClickStateMachine::showHiddenWindow(const QString &wmClass)
{
    WindowList allWindows = m_cache->getWindowsForPreview(wmClass);
    HWND target = nullptr;
    DWORD bestTime = 0;
    for (const auto &w : allWindows) {
        // 只要是不可见的窗口（包括最小化）都应该显示
        if (!w.isVisible && !w.isToolWindow && IsWindow(w.hwnd)) {
            if (!target || w.lastActiveTime > bestTime) {
                target = w.hwnd;
                bestTime = w.lastActiveTime;
            }
        }
    }
    if (target) {
        ShowWindow(target, SW_SHOW);
        AllowSetForegroundWindow(ASFW_ANY);
        SetForegroundWindow(target);
        SetFocus(target);
    }
}
```

- [ ] **Step 3: 运行测试验证**

Run: `cmake --build build --config Release && build\Release\test_click_state_machine.exe`

---

## Task 2: 修复bringToForeground的窗口激活

**Files:**
- Modify: `src/core/ClickStateMachine.cpp:153-161`

- [ ] **Step 1: 检查bringToForeground实现**

当前实现：
```cpp
void ClickStateMachine::bringToForeground(const QString &wmClass)
{
    HWND hwnd = m_cache->getLastActiveHwnd(wmClass);
    if (!hwnd || !IsWindow(hwnd)) return;
    AllowSetForegroundWindow(ASFW_ANY);
    SetForegroundWindow(hwnd);
    SetFocus(hwnd);
}
```

**问题：** 缺少ShowWindow调用，如果窗口被最小化需要先恢复

- [ ] **Step 2: 修复bringToForeground**

```cpp
void ClickStateMachine::bringToForeground(const QString &wmClass)
{
    HWND hwnd = m_cache->getLastActiveHwnd(wmClass);
    if (!hwnd || !IsWindow(hwnd)) return;
    
    // 如果窗口被最小化，先恢复
    if (IsIconic(hwnd)) {
        ShowWindow(hwnd, SW_RESTORE);
    }
    
    AllowSetForegroundWindow(ASFW_ANY);
    SetForegroundWindow(hwnd);
    SetFocus(hwnd);
}
```

- [ ] **Step 3: 运行测试验证**

Run: `cmake --build build --config Release && build\Release\test_click_state_machine.exe`

---

## Task 3: 删除双击功能

**Files:**
- Modify: `src/ui/DockWindow.cpp:753-760` (handleDoubleClick)
- Modify: `src/ui/DockWindow.cpp:495-500` (clicked信号连接)
- Modify: `src/ui/DockItem.cpp` (如有双击事件处理)

- [ ] **Step 1: 检查DockWindow中的双击处理**

```cpp
void DockWindow::handleDoubleClick(DockItem *item)
{
    launchApp(item);
    QTimer::singleShot(2000, item, [item]() {
        item->triggerInteractionIndicator();
    });
}
```

- [ ] **Step 2: 删除handleDoubleClick函数**

删除整个函数，包括头文件声明

- [ ] **Step 3: 检查clicked信号连接**

```cpp
connect(item, &DockItem::clicked, this, [this, item](const QString &) {
    handleSingleClick(item);
});
```

确认没有doubleClicked信号连接

- [ ] **Step 4: 运行测试验证**

Run: `cmake --build build --config Release`

---

## Task 4: 改进预览窗口的延迟隐藏

**Files:**
- Modify: `src/ui/WindowPreviewPanel.cpp:44-46` (m_leaveTimer间隔)
- Modify: `src/ui/DockWindow.cpp:406-410` (leaveEvent)

- [ ] **Step 1: 增加leaveTimer间隔到300ms**

```cpp
m_leaveTimer->setInterval(300);  // 从200ms增加到300ms
```

- [ ] **Step 2: 验证leaveEvent调用startDelayedHide**

当前实现：
```cpp
void DockWindow::leaveEvent(QEvent *event)
{
    Q_UNUSED(event);
    resetFishEyeEffect();
    m_windowPreview->startDelayedHide();
}
```

确认逻辑正确

- [ ] **Step 3: 运行测试验证**

手动测试：鼠标从Dock移向预览窗口，确认预览窗口不会立即消失

---

## Task 5: 修复isForegroundApp检测

**Files:**
- Modify: `src/core/WindowCache.cpp` (isForegroundApp实现)

- [ ] **Step 1: 检查isForegroundApp实现**

```cpp
bool WindowCache::isForegroundApp(const QString &wmClass)
{
    HWND fg = GetForegroundWindow();
    if (!fg) return false;
    // 检查前台窗口是否属于该wmClass
    // ...
}
```

- [ ] **Step 2: 添加调试日志**

```cpp
bool WindowCache::isForegroundApp(const QString &wmClass)
{
    HWND fg = GetForegroundWindow();
    if (!fg) {
        qDebug() << "isForegroundApp: no foreground window";
        return false;
    }
    
    QString fgClass = getWmClassForHwnd(fg);
    bool result = (fgClass.toLower() == wmClass.toLower());
    qDebug() << "isForegroundApp:" << wmClass << "fgClass:" << fgClass << "result:" << result;
    return result;
}
```

- [ ] **Step 3: 运行测试并分析日志**

Run: `cmake --build build --config Release && build\Release\WMacDock.exe`

检查dock.log中的isForegroundApp日志

---

## Task 6: 优化动画性能（可选）

**Files:**
- Modify: `src/ui/DockWindow.cpp:823-854` (animateItemToScale)

- [ ] **Step 1: 检查当前实现**

已优化：复用动画对象，缩短时长到150ms，使用OutCubic缓动

- [ ] **Step 2: 如仍有卡顿，进一步优化**

考虑使用QTimer进行批量布局更新，而非每帧更新

---

## 验证清单

完成所有任务后，验证以下行为：

1. **点击未运行的应用** → 启动应用 ✓
2. **点击前台窗口的应用** → 最小化窗口 ✓
3. **点击后台窗口的应用** → 激活窗口（带到前台）✓
4. **点击最小化窗口的应用** → 恢复窗口 ✓
5. **双击任何应用** → 与单击行为相同 ✓
6. **鼠标从Dock移向预览窗口** → 预览窗口保持显示300ms ✓
7. **预览窗口中点击缩略图** → 激活对应窗口 ✓
8. **动画流畅无卡顿** ✓
