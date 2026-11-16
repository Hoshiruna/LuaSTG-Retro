#pragma once

#include <sdkddkver.h>
#include <sstream>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "Core/framework.hpp"
#include <Windows.h>
#include <Unknwn.h>
#include <VersionHelpers.h>
#include <wrl/client.h>
#include <wrl/wrappers/corewrappers.h>
#include <wil/resource.h>
#include <wil/com.h>
#include <win32/base.hpp>
#include <timeapi.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <d3d11_4.h>
#include <d3dcompiler.h>
#include <wincodec.h>
#include <dwrite_3.h>
#include <d2d1_3.h>
#include <dcomp.h>
#include <dwmapi.h>
#include <DirectXMath.h>

#include "tracy/TracyD3D11.hpp"

#include "Platform/HResultChecker.hpp"

namespace Microsoft::WRL::Wrappers
{
    namespace HandleTraits
    {
        struct ThreadHandleTraits : HANDLENullTraits
        {
        };
    }
    using ThreadHandle = HandleT<HandleTraits::ThreadHandleTraits>;
}

#ifdef _DEBUG
inline void
F_D3D_SET_DEBUG_NAME(IDXGIObject* ptr, std::string_view file, int line, std::string_view name)
{
    if(ptr) {
        std::stringstream strstr;
        strstr << "File: " << file << " ";
        strstr << "Line: " << line << " ";
        strstr << "DXGIObject: " << name;
        std::string str = strstr.str();
        ptr->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)str.length(), str.c_str());
    }
}
inline void
F_D3D_SET_DEBUG_NAME(ID3D11Device* ptr, std::string_view file, int line, std::string_view name)
{
    if(ptr) {
        std::stringstream strstr;
        strstr << "File: " << file << " ";
        strstr << "Line: " << line << " ";
        strstr << "DXGIObject: " << name;
        std::string str = strstr.str();
        ptr->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)str.length(), str.c_str());
    }
}
inline void
F_D3D_SET_DEBUG_NAME(ID3D11DeviceChild* ptr, std::string_view file, int line, std::string_view name)
{
    if(ptr) {
        std::stringstream strstr;
        strstr << "File: " << file << " ";
        strstr << "Line: " << line << " ";
        strstr << "DXGIObject: " << name;
        std::string str = strstr.str();
        ptr->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)str.length(), str.c_str());
    }
}
#define M_D3D_SET_DEBUG_NAME(OBJ, STR) F_D3D_SET_DEBUG_NAME(OBJ, __FILE__, __LINE__, STR)
#define M_D3D_SET_DEBUG_NAME_SIMPLE(OBJ) M_D3D_SET_DEBUG_NAME(OBJ, #OBJ)
#else
#define M_D3D_SET_DEBUG_NAME(OBJ, STR)
#define M_D3D_SET_DEBUG_NAME_SIMPLE(OBJ)
#endif
