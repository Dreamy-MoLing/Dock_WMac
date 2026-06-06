#!/usr/bin/env bash
# setup_msvc.sh — 将 MSVC 工具链环境变量注入当前 bash 会话
#
# 用法: source setup_msvc.sh
# 自动发现 VS 2022（含 Insiders），运行 vcvars64.bat，导出编译环境到 bash。
#
# 注意：在当前 shell 中设置 PATH/INCLUDE/LIB/LIBPATH 等变量。
# MSVC link.exe 优先级高于 /usr/bin/link（已自动处理）。

if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    echo "[setup_msvc] 请使用 source 执行:  source setup_msvc.sh" >&2
    exit 1
fi

_vs_where="C:/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe"

if [[ ! -f "$_vs_where" ]]; then
    echo "[setup_msvc] vswhere.exe 未找到" >&2
    return 1
fi

# 发现最新 VS（含预览版）
_vs_path=$("$_vs_where" -latest -prerelease -property installationPath 2>/dev/null)
if [[ -z "$_vs_path" ]]; then
    echo "[setup_msvc] 未找到 Visual Studio" >&2
    return 1
fi

_vcvars="$_vs_path/VC/Auxiliary/Build/vcvars64.bat"
if [[ ! -f "$_vcvars" ]]; then
    echo "[setup_msvc] vcvars64.bat 未找到: $_vcvars" >&2
    return 1
fi

echo "[setup_msvc] $_vs_path"

# 1. 运行 vcvars64.bat → 用 set 打印所有环境变量 → 临时文件
#    必须在 cmd /c 中 call vcvars，因为脚本会修改当前 cmd 环境
_tmp=$(mktemp)
cmd //c "call \"$_vcvars\" > nul && set" 2>/dev/null > "$_tmp"

# 2. 解析并导出 MSVC 专用环境变量（INCLUDE/LIB/LIBPATH 等）
_loaded_keys=()
while IFS='=' read -r key value; do
    # 跳过空行/无效行
    [[ -z "$key" ]] && continue
    case "$key" in
        VSINSTALLDIR|VCINSTALLDIR|DevEnvDir|VCToolsVersion|VCToolsRedistDir|\
        WindowsSdkDir|WindowsSDKVersion|WindowsLibPath|\
        ExtensionSdkDir|UniversalCRTSdkDir|UCRTVersion|\
        INCLUDE|Include|LIB|LIBPATH|LibPath|\
        FrameworkDir|FrameworkVersion|Framework40Version|\
        NETFXSDKDir|FSharpTargetsPath|\
        CommandPromptType|Platform|PreferredToolArchitecture|\
        VSCMD_ARG_*|__*)
            # 转换 \ → / ，去尾部 ;
            _val="${value//\\//}"
            _val="${_val%;}"
            export "$key=$_val"
            _loaded_keys+=("$key")
            ;;
    esac
done < "$_tmp"

# 3. 从 Windows PATH 提取 MSVC/WinSDK 目录，转为 Unix PATH 追加到 bash
#    先取 cmd 侧的完整 PATH
_msvc_win_path=$(grep -E '^PATH=' "$_tmp" | head -1 | cut -d= -f2-)
rm -f "$_tmp"

# 4. 转 Unix 分隔符
_msvc_unix_path="${_msvc_win_path//;/:}"
_msvc_unix_path="${_msvc_unix_path//\\//}"

# 5. 过滤：只保留 MSVC / Windows Kits / Qt 相关路径
IFS=':' read -ra _segs <<< "$_msvc_unix_path"
for _p in "${_segs[@]}"; do
    case "$_p" in
        *"[MSVC]"*|*"[msvc]"*|*"Visual Studio"*|*"Windows Kits"*|*"Qt"*|*".dotnet"*)
            ;;
        *)
            # 避免引入 /usr/bin/link 被 MSVC link.exe 覆盖的警告
            # 只添加还没在 PATH 中的目录
            case ":$PATH:" in
                *":$_p:"*) continue ;;
            esac
            export PATH="$_p:$PATH"
            ;;
    esac
done

# 6. 确保 MSVC Tools 目录在最前面（link.exe 优先于 /usr/bin/link）
if [[ -d "$_vs_path/VC/Tools/MSVC" ]]; then
    # 找到版本号子目录（如 14.43.34808）
    for _msvc_ver_dir in "$_vs_path/VC/Tools/MSVC/"*/; do
        _msvc_bin="${_msvc_ver_dir}bin/Hostx64/x64"
        _msvc_bin="${_msvc_bin//\\//}"
        if [[ -d "$_msvc_bin" ]]; then
            # 去重后加到最前
            case ":$PATH:" in
                *":$_msvc_bin:"*) ;;
                *) export PATH="$_msvc_bin:$PATH" ;;
            esac
        fi
    done
fi

# 7. VS 自带的 cmake（如果系统没装）
if ! command -v cmake &>/dev/null; then
    _cmake="$_vs_path/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
    _cmake_dir="${_cmake%/*}"
    _cmake_dir="${_cmake_dir//\\//}"
    if [[ -f "$_cmake" ]]; then
        export PATH="$_cmake_dir:$PATH"
        echo "[setup_msvc] cmake (VS bundled)"
    fi
fi

# 8. 报告
echo "[setup_msvc] 已加载 ${#_loaded_keys[@]} 个环境变量"
echo "[setup_msvc] cl.exe : $(command -v cl.exe 2>/dev/null || echo 'NOT FOUND — 请检查 VS 安装')"
echo "[setup_msvc] ninja  : $(command -v ninja 2>/dev/null || echo 'NOT FOUND — cmake 会使用 MSVC 自带 ninja')"
echo "[setup_msvc] cmake  : $(command -v cmake 2>/dev/null || echo 'NOT FOUND')"
echo "[setup_msvc] 环境就绪"

unset _vs_where _vs_path _vcvars _tmp _loaded_keys _msvc_win_path _msvc_unix_path _segs _p _msvc_ver_dir _msvc_bin _cmake _cmake_dir _val
