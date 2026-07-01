#include "pch.h"
#include "SingleInstanceGuard.h"

namespace DockWMac::app
{
    SingleInstanceGuard::SingleInstanceGuard()
    {
        m_mutex = CreateMutexW(nullptr, TRUE, L"Local\\Dock_WMac_v2_SingleInstance");
        m_duplicate = m_mutex && GetLastError() == ERROR_ALREADY_EXISTS;
    }

    SingleInstanceGuard::~SingleInstanceGuard()
    {
        if (m_mutex)
        {
            CloseHandle(m_mutex);
        }
    }

    bool SingleInstanceGuard::IsDuplicate() const noexcept
    {
        return m_duplicate;
    }
}
