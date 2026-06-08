#ifndef LOGGER_H
#define LOGGER_H

#include <QString>

/**
 * @file Logger.h
 * @brief 日志系统初始化
 *
 * 安装 Qt 消息处理器，将日志输出到文件和 stderr。
 * 日志路径: ~/.local/share/Dock_WMac/dock.log (Linux)
 *          %LOCALAPPDATA%/Dock_WMac/dock.log (Windows)
 */

namespace Logger {

/** @brief 初始化日志系统，安装消息处理器 */
void init();

/** @brief 获取日志文件路径 */
QString logFilePath();

/** @brief 获取日志目录路径（用于卸载清理）
 *  Windows: %LOCALAPPDATA%/Dock_WMac */
QString logDir();

} // namespace Logger

#endif // LOGGER_H
