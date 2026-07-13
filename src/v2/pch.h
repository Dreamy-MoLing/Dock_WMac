#pragma once

#include <windows.h>
#include <unknwn.h>
#include <commctrl.h>
#include <commoncontrols.h>
#include <dwmapi.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <dcomp.h>
#include <d2d1_1.h>
#include <dwrite.h>
#include <wincodec.h>
#include <propkey.h>
#include <propsys.h>
#include <shlobj_core.h>
#include <shobjidl.h>
#ifdef GetCurrentTime
#undef GetCurrentTime
#endif

#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <memory>
#include <optional>
#include <algorithm>
#include <map>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.UI.ViewManagement.h>
