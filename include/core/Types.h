#ifndef TYPES_H
#define TYPES_H

#include <QString>

/**
 * @file Types.h
 * @brief 项目全局公共类型与枚举定义
 */

/**
 * @brief Dock 交互状态
 */
enum class DockState {
    Docked,     // 常驻显示
    Hidden,     // 隐藏（窗口全屏/最大化时）
    Animating   // 过渡动画中
};

/**
 * @brief Dock 图标数据单元
 */
struct DockItemData {
    QString appId;          // 应用标识 (Windows AppUserModelID)
    QString displayName;    // 显示名称
    QString execPath;       // 可执行文件路径
    QString iconPath;       // 图标路径
    bool    isRunning;      // 是否正在运行
    int     badgeCount;     // 未读通知数 (0=无)
    int     windowCount = 1; // 窗口数量（默认1，用于多窗口堆叠指示器）
};

#endif // TYPES_H
