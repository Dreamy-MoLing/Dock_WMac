/**
 * @file SysHelper.cpp
 * @brief 系统适配层 - 平台无关存根
 *
 * 仅包含构造函数，平台相关实现在各自平台文件中。
 * SysHelper_linux.cpp (Linux) 或 SysHelper_win.cpp (Win32) 提供完整实现。
 */

#include "core/SysHelper.h"
#include "core/IPCHelper.h"

SysHelper::SysHelper(QObject *parent)
    : QObject(parent)
    , m_ipcHelper(nullptr)
{
}

void SysHelper::setIPCHelper(IPCHelper *helper)
{
    m_ipcHelper = helper;
}


