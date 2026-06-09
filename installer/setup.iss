; Dock_WMac — Inno Setup 安装脚本
; 用法: 先运行 cmake --build build --config Release + windeployqt，然后:
;   iscc installer/setup.iss
; 产物: dist/Dock_WMac_Setup_v0.2.3.exe

#define MyAppName "Mac任务栏"
#define MyAppVersion "0.2.3"
#define MyAppPublisher "Dock_WMac"
#define MyAppExeName "Mac任务栏.exe"
#define MyUninstallExeName "uninstall.exe"

[Setup]
AppId={{B8F4A3D2-7E6C-4F1A-9D8B-2C5E0F7A3D1B}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
; 安装目录全英文，避免编码冲突
DefaultDirName={localappdata}\Programs\Dock_WMac
DefaultGroupName=Dock_WMac
DisableProgramGroupPage=yes
DisableDirPage=no
UninstallDisplayName={#MyAppName}
UninstallDisplayIcon={app}\{#MyAppExeName}
; 不需要管理员权限 — 安装在用户目录
PrivilegesRequired=lowest
; 输出
OutputDir=..\dist
OutputBaseFilename=Dock_WMac_Setup_v{#MyAppVersion}
; 压缩
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
; 界面与版本信息
VersionInfoVersion={#MyAppVersion}
VersionInfoDescription=Mac任务栏 — macOS 风格 Windows Dock

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "..\deploy\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
; 安全卸载器
Source: "..\deploy\{#MyUninstallExeName}"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
; 仅开始菜单，不创建桌面快捷方式
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\卸载 {#MyAppName}"; Filename: "{uninstallexe}"

[Registry]
; 开机自启（卸载时 uninsdeletevalue 自动清理注册表）
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; \
    ValueName: "Dock_WMac"; ValueType: string; ValueData: "{app}\{#MyAppExeName}"; \
    Flags: uninsdeletevalue

[Run]
; 安装完成后自动启动（静默安装时跳过）
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#MyAppName}}"; \
    Flags: nowait postinstall skipifsilent

[UninstallRun]
; 1. 终止运行中的进程
Filename: "taskkill"; Parameters: "/f /im {#MyAppExeName}"; \
    Flags: runhidden skipifdoesntexist
; 2. 运行安全卸载器清理用户数据
Filename: "{app}\{#MyUninstallExeName}"; Parameters: "/SILENT"; \
    Flags: runhidden

[UninstallDelete]
; 兜底清理 — 如果 uninstall.exe 未能清理这些目录
Type: filesandordirs; Name: "{userappdata}\Dock_WMac"
Type: filesandordirs; Name: "{localappdata}\Dock_WMac"
Type: filesandordirs; Name: "{%TEMP}\dock_wmac_icons"
Type: filesandordirs; Name: "{%TEMP}\Dock_WMac_uninstall.log"

[Code]
function InitializeUninstall(): Boolean;
begin
  Result := True;
end;
