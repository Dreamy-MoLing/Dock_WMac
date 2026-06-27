#ifndef TYPES_H
#define TYPES_H

#include <QString>

/**
 * @file Types.h
 * @brief 项目全局公共类型与枚举定义
 */

enum class DockState {
    Docked,
    Hidden
};

struct DockItemData {
    QString appId;          // 应用标识 (Windows AppUserModelID)
    QString displayName;    // 显示名称
    QString execPath;       // 启动目标（.exe/.lnk/shell 目标）
    QString iconPath;       // 图标路径
    bool    isRunning;      // 是否正在运行
    int     badgeCount = 0;  // 未读通知数 (0=无)
    int     windowCount = 1; // 窗口数量（默认1，用于多窗口堆叠指示器）

    QString targetPath;     // 快捷方式解析出的实际目标（可为空）
    QString arguments;      // 快捷方式参数（可为空）
    QString appUserModelId; // Windows AppUserModelID（可为空）
};

#endif // TYPES_H