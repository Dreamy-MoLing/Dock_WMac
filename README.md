# Dock_WMac v1.0.0

Dock_WMac 是面向 Windows 10 1809+ 和 Windows 11 的原生桌面 Dock。它以
Mac 风格的悬浮 Dock 呈现应用启动、窗口切换、分组预览和固定项管理，同时在
Windows 已定义行为的地方遵循系统任务栏语义。

Dock_WMac 是 Windows 任务栏扩展，不是 Explorer 或 Windows Shell 的替代品。
开始菜单、通知区域、时钟、快速设置、应用 Jump List、任务栏进度与覆盖图标等
Shell 所有功能继续由 Windows 原生任务栏提供。

## v1.0.0 功能

- 从 Windows 任务栏固定项生成初始 Dock，并持久化 Dock 自己的固定项和顺序。
- 将固定应用与其运行窗口合并为一个图标，避免重复的运行中图标。
- 遵循 Windows 任务栏的启动、激活、恢复、前台再次点击最小化和多窗口选择行为。
- 支持中键与 `Shift` + 左键请求新实例，并尊重应用自身的单实例策略。
- 使用 DWM 显示单窗口和多窗口预览；最小化窗口可预览且悬停不会恢复窗口。
- 提供固定、取消固定、选择窗口、关闭窗口和关闭应用组全部窗口的原生菜单。
- 支持图标悬停放大、拖动排序、运行指示点、自动隐藏以及底部/左侧/右侧布局。
- 图标沿中心线放大，最大姿态的图标外接圆与 Dock 栏边缘相切且不会被裁切。
- 响应浅色、深色、高对比度、减少动画、DPI 和 Explorer 重启。
- 仅在 Windows 主显示器显示；主显示器变更时，唯一的 Dock 实例跟随新的主显示器。

## 原生实现

主 Dock 界面使用 C++20、C++/WinRT、Win32、DirectComposition、Direct2D、
DirectWrite 和 DWM。一个非激活的原生顶层窗口包含独立
的背景栏与图标合成层，对用户呈现为一个 Dock，并避免透明 XAML 窗口的默认背景
和缩放裁切问题。

项目不使用 Electron、Qt、WPF、Avalonia、WebView 或 WinUI XAML 作为 Dock 主
界面。WinUI 3 仅允许用于后续设置或对话框等次级界面。
Windows App SDK Stable 依赖继续固定用于这些次级界面的开发边界，但 v1.0.0 主
Dock 不初始化 Windows App Runtime，也不要求用户预装该运行时。

## 安装与运行

1. 从 GitHub Releases 获取 `Dock_WMac-v1.0.0-windows-x64.zip`。
2. 将压缩包完整解压到同一目录。
3. 运行 `Dock_WMac_v2.exe`。

发布包是使用静态 Visual C++ 运行库的原生 x64 unpackaged Win32 应用，不要求
预装 Windows App Runtime、WinUI、WebView 或 Visual C++ Redistributable。用户设置、Dock 固定项、排序
和诊断文件存放在 `%LOCALAPPDATA%\Dock_WMac`。删除发布目录即可移除程序文件；
需要清除用户状态时再删除该用户数据目录。

## 基本操作

| 操作 | 行为 |
|---|---|
| 左键点击未运行应用 | 启动应用 |
| 左键点击单窗口应用 | 激活、恢复，或在已前台时最小化 |
| 左键点击多窗口应用 | 打开窗口预览组 |
| 中键或 `Shift` + 左键 | 请求新应用实例 |
| 悬停运行中应用 | 显示 DWM 窗口预览 |
| 拖动图标 | 调整并保存 Dock 顺序 |
| 右键点击图标 | 窗口选择、固定、取消固定、关闭 |
| 右键点击 Dock 空白处 | 位置、自动隐藏、退出 |

## 构建

要求：

- Visual Studio 2022、MSVC v143 和 Windows SDK 10.0.26100.0；或使用已安装
  v145 工具集的 Visual Studio Insiders/VS2026。
- Windows App SDK NuGet 包恢复能力。
- x64 Windows 10 1809+ 或 Windows 11。

Visual Studio 2022 Developer PowerShell：

```powershell
msbuild Dock_WMac_v2.sln /restore /p:Configuration=Release /p:Platform=x64
```

仅安装 v145 工具集时：

```powershell
& "C:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" Dock_WMac_v2.sln /restore /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v145
```

运行测试和原生集成自检：

```powershell
build\v2\x64\Release\Dock_WMac_v2_tests.exe
build\v2\x64\Release\Dock_WMac_v2.exe --self-check
```

生成并验证正式发布包：

```powershell
.\scripts\package-v1-release.ps1 -Configuration Release -Platform x64 -PlatformToolset v145 -Version v1.0.0
```

输出位于 `artifacts\release\Dock_WMac-v1.0.0-windows-x64.zip`。打包过程会校验
模型测试、原生自检、Dock 状态诊断、资源诊断、原生依赖和禁止的浏览器运行时。

## 诊断

```powershell
build\v2\x64\Release\Dock_WMac_v2.exe --dump-dock-state
build\v2\x64\Release\Dock_WMac_v2.exe --dump-resource-metrics
.\scripts\validate-v1-resource-stability.ps1 -DurationSeconds 300
```

状态诊断写入 `%LOCALAPPDATA%\Dock_WMac\diagnostics`。五分钟资源稳定性报告写入
`artifacts\validation`。

## 许可证

Dock_WMac 使用 [MIT License](LICENSE)。参考项目仅用于行为、架构和风险研究；
未复制 RoundedTB、TaskbarX、Windhawk 或其他项目的代码与资产。
