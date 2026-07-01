#pragma once

namespace DockWMac::app
{
    class SingleInstanceGuard
    {
    public:
        SingleInstanceGuard();
        ~SingleInstanceGuard();

        SingleInstanceGuard(SingleInstanceGuard const&) = delete;
        SingleInstanceGuard& operator=(SingleInstanceGuard const&) = delete;

        bool IsDuplicate() const noexcept;

    private:
        HANDLE m_mutex{};
        bool m_duplicate{};
    };
}
