#pragma once

#include "SingleInstanceGuard.h"
#include "../dock/DockModel.h"
#include "../infra/AppSettings.h"
#include "../render/NativeDockHost.h"
#include "../shell/DwmPreviewHost.h"
#include "../shell/ShellTypes.h"
#include "../shell/WindowEventMonitor.h"

namespace DockWMac::app
{
    class DockRuntime
    {
    public:
        DockRuntime();
        ~DockRuntime();

        DockRuntime(DockRuntime const&) = delete;
        DockRuntime& operator=(DockRuntime const&) = delete;

        int Run();

    private:
        bool Start();
        void Stop();
        void StartRefreshTimers();
        void StopRefreshTimers();
        void ScheduleDockRefresh(std::chrono::milliseconds delay);
        bool HandleThreadTimer(MSG const& message);
        void LoadSystemPinnedSnapshot();
        void RefreshDockItems(bool initializeOrder);
        void ConfigureDockWindow();
        void ApplyIconCache(std::vector<DockWMac::dock::DockItem>& items);
        void HandleDockAction(DockWMac::dock::DockAction const& action);
        void HandleDockOrderChanged(std::vector<std::wstring> const& order);
        void HandlePinToDock(std::wstring const& itemId);
        void HandleUnpinFromDock(std::wstring const& itemId);
        void HandleCloseWindow(HWND hwnd);
        void HandleCloseAllWindows(std::wstring const& itemId);
        void ShowDockPreview(DockWMac::dock::DockWindowRef const& window);
        bool ShowDockWindowGroupPreview(std::vector<DockWMac::dock::DockWindowRef> const& windows);
        void HideDockPreview();
        void HandleShellEnvironmentChanged();
        void HandleSystemSettingsChanged();

        SingleInstanceGuard m_instance;
        DockWMac::infra::RuntimePaths m_paths;
        DockWMac::infra::AppSettings m_settings;
        DockWMac::dock::DockState m_dockState;
        std::unique_ptr<DockWMac::shell::DwmPreviewHost> m_previewHost;
        std::unique_ptr<DockWMac::render::NativeDockHost> m_dockHost;
        DockWMac::shell::WindowEventMonitor m_windowEventMonitor;
        std::vector<DockWMac::shell::PinnedApp> m_systemPinnedApps;
        std::vector<DockWMac::dock::DockItem> m_items;
        UINT_PTR m_refreshTimer{};
        UINT_PTR m_actionRefreshTimer{};
        int m_exitCode{};
        bool m_started{};
    };
}
