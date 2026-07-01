#ifndef LOGGER_H
#define LOGGER_H

#include <QString>

/**
 * @file Logger.h
 * @brief 日志系统初始化
 *
 * 安装 Qt 消息处理器，将日志输出到文件和 stderr。
 * 日志优先写入程序旁 data/；目录不可写时回退到用户本地应用数据目录。
 */

namespace Logger {

/** @brief 初始化日志系统，安装消息处理器 */
void init();

/** @brief 获取日志文件路径 */
QString logFilePath();

/** @brief 获取当前实际日志目录路径（用于诊断和清理） */
QString logDir();

} // namespace Logger

#endif // LOGGER_H
