/**
 * @file uninstall.cpp
 * @brief Dock_WMac 安全卸载器
 *
 * 纯 Win32 API，无 Qt 依赖。三层安全验证：
 *   1. 目录名必须匹配白名单（Dock_WMac / dock_wmac_icons）
 *   2. 父目录必须匹配系统路径
 *   3. 不得为符号链接/reparse point
 *
 * 运行模式：
 *   - 无参数：GUI MessageBox 确认
 *   - /SILENT：静默执行（Inno Setup 调用）
 *   - /DRYRUN：仅输出，不实际删除（审计用）
 *
 * 日志：%TEMP%\Dock_WMac_uninstall.log
 */

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <cstdio>
#include <ctime>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "advapi32.lib")

// ─── 日志 ────────────────────────────────────────────────────

static FILE *g_log = nullptr;
static bool   g_silent = false;
static bool   g_dryRun = false;
static int    g_deleteCount = 0;
static int    g_regCount = 0;

static void Log(const wchar_t *format, ...)
{
    if (!g_log) return;

    // 时间戳
    time_t now = time(nullptr);
    struct tm tmNow;
    localtime_s(&tmNow, &now);
    fwprintf(g_log, L"%04d-%02d-%02d %02d:%02d:%02d ",
             tmNow.tm_year + 1900, tmNow.tm_mon + 1, tmNow.tm_mday,
             tmNow.tm_hour, tmNow.tm_min, tmNow.tm_sec);

    va_list args;
    va_start(args, format);
    vfwprintf(g_log, format, args);
    va_end(args);
    fwprintf(g_log, L"\n");
    fflush(g_log);
}

// ─── 路径工具 ────────────────────────────────────────────────

// 获取父目录（原地修改，去掉最后一个反斜杠后的内容）
static void GetParentDir(wchar_t *path)
{
    // 去掉末尾反斜杠
    size_t len = wcslen(path);
    while (len > 0 && (path[len - 1] == L'\\' || path[len - 1] == L'/')) {
        path[--len] = L'\0';
    }
    // 找到最后一个反斜杠
    wchar_t *lastSlash = wcsrchr(path, L'\\');
    if (!lastSlash) lastSlash = wcsrchr(path, L'/');
    if (lastSlash) *lastSlash = L'\0';
}

// 获取路径末尾的目录名
static const wchar_t *GetDirName(const wchar_t *path)
{
    size_t len = wcslen(path);
    // 去掉末尾反斜杠
    while (len > 0 && (path[len - 1] == L'\\' || path[len - 1] == L'/')) {
        len--;
    }
    // 找到最后一个反斜杠
    const wchar_t *last = path + len;
    while (last > path && *(last - 1) != L'\\' && *(last - 1) != L'/') {
        last--;
    }
    return last;
}

// 展开环境变量（如 %APPDATA%）
static bool ExpandEnvString(const wchar_t *input, wchar_t *output, size_t outSize)
{
    DWORD result = ExpandEnvironmentStringsW(input, output, (DWORD)outSize);
    return result > 0 && result <= outSize;
}

// ─── 安全校验 ────────────────────────────────────────────────

struct CleanTarget
{
    const wchar_t *envPath;          // 含环境变量的路径，如 L"%APPDATA%\\Dock_WMac"
    const wchar_t *expectedParentEnv; // 父目录环境变量，如 L"%APPDATA%"
    const wchar_t *expectedDirName;   // 目录名，如 L"Dock_WMac"
    const wchar_t *description;       // 描述
};

// 三层安全验证
static bool ValidatePath(const wchar_t *fullPath, const CleanTarget &target)
{
    // 第 1 层：目录名白名单
    const wchar_t *dirName = GetDirName(fullPath);
    if (_wcsicmp(dirName, target.expectedDirName) != 0) {
        Log(L"[FAIL] Directory name mismatch: '%s' != '%s'  — path: %s",
            dirName, target.expectedDirName, fullPath);
        return false;
    }

    // 第 2 层：父目录匹配
    wchar_t expandedParent[MAX_PATH];
    if (!ExpandEnvString(target.expectedParentEnv, expandedParent, MAX_PATH)) {
        Log(L"[FAIL] Cannot expand parent env: %s", target.expectedParentEnv);
        return false;
    }

    wchar_t actualParent[MAX_PATH];
    wcscpy_s(actualParent, fullPath);
    GetParentDir(actualParent);

    if (_wcsicmp(actualParent, expandedParent) != 0) {
        Log(L"[FAIL] Parent directory mismatch: '%s' != '%s'  — path: %s",
            actualParent, expandedParent, fullPath);
        return false;
    }

    // 第 3 层：非符号链接
    DWORD attrs = GetFileAttributesW(fullPath);
    if (attrs != INVALID_FILE_ATTRIBUTES
        && (attrs & FILE_ATTRIBUTE_REPARSE_POINT)) {
        Log(L"[FAIL] Path is a reparse point (symlink/junction), refusing: %s", fullPath);
        return false;
    }

    Log(L"[PASS] %s → dirname=%s ✓, parent=%s ✓, not-symlink ✓",
        fullPath, dirName, actualParent);
    return true;
}

// 递归删除目录
static bool RemoveDirectoryRecursive(const wchar_t *path)
{
    if (g_dryRun) {
        Log(L"[DRYRUN] Would delete: %s", path);
        g_deleteCount++;
        return true;
    }

    size_t len = wcslen(path);
    wchar_t *searchPath = new wchar_t[len + 3];
    wcscpy_s(searchPath, len + 3, path);
    searchPath[len] = L'\\';
    searchPath[len + 1] = L'*';
    searchPath[len + 2] = L'\0';

    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(searchPath, &fd);
    if (hFind == INVALID_HANDLE_VALUE) {
        delete[] searchPath;
        BOOL result = RemoveDirectoryW(path);
        if (!result) {
            Log(L"[WARN] RemoveDirectory failed: %s (error=%lu)", path, GetLastError());
        }
        return result != FALSE;
    }

    // 先递归删除所有子项
    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
            continue;

        wchar_t *subPath = new wchar_t[len + wcslen(fd.cFileName) + 3];
        wsprintfW(subPath, L"%s\\%s", path, fd.cFileName);

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            RemoveDirectoryRecursive(subPath);
        } else {
            SetFileAttributesW(subPath, FILE_ATTRIBUTE_NORMAL);
            DeleteFileW(subPath);
        }
        delete[] subPath;
    } while (FindNextFileW(hFind, &fd));

    FindClose(hFind);
    delete[] searchPath;

    BOOL result = RemoveDirectoryW(path);
    if (!result) {
        Log(L"[WARN] RemoveDirectory failed: %s (error=%lu)", path, GetLastError());
    }
    return result != FALSE;
}

// 删除注册表值
static bool DeleteRegistryValue(HKEY root, const wchar_t *subKey, const wchar_t *valueName)
{
    if (g_dryRun) {
        Log(L"[DRYRUN] Would delete registry: HKCU\\%s [%s]", subKey, valueName);
        g_regCount++;
        return true;
    }

    HKEY hKey;
    LONG result = RegOpenKeyExW(root, subKey, 0, KEY_SET_VALUE, &hKey);
    if (result != ERROR_SUCCESS) {
        Log(L"[INFO] Registry key not found, skip: HKCU\\%s", subKey);
        return true; // 不存在也算成功
    }

    result = RegDeleteValueW(hKey, valueName);
    RegCloseKey(hKey);

    if (result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND) {
        Log(L"[DELETE] Registry: HKCU\\%s [%s]", subKey, valueName);
        g_regCount++;
        return true;
    }

    Log(L"[FAIL] Cannot delete registry value: HKCU\\%s [%s] (error=%lu)",
        subKey, valueName, result);
    return false;
}

// ─── 清理流程 ────────────────────────────────────────────────

static bool ExecuteCleanup()
{
    // 定义清理目标（顺序：数据目录 → 日志目录 → 图标缓存 → 注册表）
    const CleanTarget targets[] = {
        { L"%APPDATA%\\Dock_WMac",       L"%APPDATA%", L"Dock_WMac",       L"config" },
        { L"%LOCALAPPDATA%\\Dock_WMac",  L"%LOCALAPPDATA%", L"Dock_WMac",  L"logs" },
        { L"%TEMP%\\dock_wmac_icons",    L"%TEMP%",    L"dock_wmac_icons", L"icon cache" },
    };

    Log(L"[INFO] Starting cleanup...");

    // 阶段 1：删除文件和目录
    for (int i = 0; i < 3; i++) {
        wchar_t fullPath[MAX_PATH];
        if (!ExpandEnvString(targets[i].envPath, fullPath, MAX_PATH)) {
            Log(L"[FAIL] Cannot expand path: %s", targets[i].envPath);
            continue;
        }

        Log(L"[CHECK] Validating: %s (%s)", fullPath, targets[i].description);

        if (!ValidatePath(fullPath, targets[i])) {
            Log(L"[SKIP] Safety check failed for: %s", fullPath);
            continue;
        }

        // 检查目录是否存在
        DWORD attrs = GetFileAttributesW(fullPath);
        if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
            Log(L"[INFO] Directory does not exist, skip: %s", fullPath);
            continue;
        }

        if (RemoveDirectoryRecursive(fullPath)) {
            Log(L"[DELETE] Removed: %s", fullPath);
            g_deleteCount++;
        }
    }

    // 阶段 2：删除注册表项
    DeleteRegistryValue(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        L"Dock_WMac");

    Log(L"[INFO] Cleanup complete. %d directories, %d registry entries removed.",
        g_deleteCount, g_regCount);
    return true;
}

// ─── GUI 确认对话框 ──────────────────────────────────────────

static int ShowConfirmDialog()
{
    const wchar_t *msg =
        L"The following will be removed:\n\n"
        L"[Directories]\n"
        L"  %APPDATA%\\Dock_WMac\\          (config)\n"
        L"  %LOCALAPPDATA%\\Dock_WMac\\     (logs)\n"
        L"  %TEMP%\\dock_wmac_icons\\       (icon cache)\n\n"
        L"[Registry]\n"
        L"  HKCU\\...\\Run\\Dock_WMac        (auto-start)\n\n"
        L"Continue?";

    int result = MessageBoxW(nullptr, msg,
        L"Dock_WMac Uninstaller",
        MB_ICONQUESTION | MB_YESNO | MB_DEFBUTTON2);

    return result;
}

// ─── 入口 ────────────────────────────────────────────────────

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow)
{
    // 解析命令行参数
    int argc;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    for (int i = 1; i < argc; i++) {
        if (_wcsicmp(argv[i], L"/SILENT") == 0)
            g_silent = true;
        else if (_wcsicmp(argv[i], L"/DRYRUN") == 0)
            g_dryRun = true;
    }

    // 初始化日志
    wchar_t logPath[MAX_PATH];
    ExpandEnvString(L"%TEMP%\\Dock_WMac_uninstall.log", logPath, MAX_PATH);
    _wfopen_s(&g_log, logPath, L"a, ccs=UTF-8");

    const wchar_t *mode = g_dryRun ? L"DRYRUN" : (g_silent ? L"SILENT" : L"INTERACTIVE");
    Log(L"[INFO] === Uninstaller started (mode: %s) ===", mode);

    // 交互模式：先弹确认框
    bool proceed = true;
    if (!g_silent && !g_dryRun) {
        int choice = ShowConfirmDialog();
        proceed = (choice == IDYES);
        if (!proceed) {
            Log(L"[INFO] User cancelled.");
        }
    }

    if (proceed) {
        ExecuteCleanup();
    }

    Log(L"[INFO] === Uninstaller finished ===\n");

    if (g_log) fclose(g_log);
    LocalFree(argv);

    // 交互模式：完成后弹结果框
    if (!g_silent && !g_dryRun && proceed) {
        wchar_t resultMsg[512];
        wsprintfW(resultMsg,
            L"Cleanup complete.\n\n"
            L"%d director%s removed.\n"
            L"%d registry entr%s removed.\n\n"
            L"Details: %s",
            g_deleteCount, g_deleteCount == 1 ? L"y" : L"ies",
            g_regCount, g_regCount == 1 ? L"y" : L"ies",
            logPath);
        MessageBoxW(nullptr, resultMsg, L"Dock_WMac Uninstaller", MB_ICONINFORMATION);
    }

    return 0;
}
