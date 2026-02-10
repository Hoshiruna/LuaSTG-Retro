# d3dcompiler

add_library(Microsoft.D3DCompiler.Redist SHARED IMPORTED GLOBAL)

# 注意：从 Windows SDK 10.0.26100 开始，d3dcompiler_47.dll 不再静态链接 CRT，而是链接到 UCRT

CPMAddPackage(
    NAME Microsoft.Windows.SDK.CPP
    VERSION 10.0.22621.3233
    URL https://www.nuget.org/api/v2/package/Microsoft.Windows.SDK.CPP/10.0.22621.3233
    URL_HASH SHA256=E4EFE1768EA61F4F999DBEF61B09895320629F975F9CEED8290A9633E0C31623
    DOWNLOAD_ONLY YES
)

if (NOT Microsoft.Windows.SDK.CPP_ADDED)
    message(WARNING "Microsoft.Windows.SDK.CPP was not added, fallback to local Windows SDK")
endif ()

if (CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(d3dcompiler_arch "x64")
elseif (CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(d3dcompiler_arch "x86")
else ()
    message(FATAL_ERROR "unknown platform")
endif ()

set(d3dcompiler47_dll "")
set(d3dcompiler47_lib "")

if (Microsoft.Windows.SDK.CPP_ADDED)
    set(windows_sdk_pkg_root "${Microsoft.Windows.SDK.CPP_SOURCE_DIR}")

    set(windows_sdk_pkg_dll_candidates
        "${windows_sdk_pkg_root}/c/Redist/D3D/${d3dcompiler_arch}/d3dcompiler_47.dll"
    )
    file(GLOB windows_sdk_pkg_dll_candidates_glob
        "${windows_sdk_pkg_root}/c/Redist/*/D3D/${d3dcompiler_arch}/d3dcompiler_47.dll"
        "${windows_sdk_pkg_root}/c/bin/*/${d3dcompiler_arch}/d3dcompiler_47.dll"
    )
    list(APPEND windows_sdk_pkg_dll_candidates ${windows_sdk_pkg_dll_candidates_glob})

    foreach(candidate IN LISTS windows_sdk_pkg_dll_candidates)
        if (EXISTS "${candidate}")
            set(d3dcompiler47_dll "${candidate}")
            break()
        endif ()
    endforeach ()

    file(GLOB windows_sdk_pkg_lib_candidates
        "${windows_sdk_pkg_root}/c/Lib/*/um/${d3dcompiler_arch}/d3dcompiler.lib"
        "${windows_sdk_pkg_root}/c/lib/*/um/${d3dcompiler_arch}/d3dcompiler.lib"
    )
    list(SORT windows_sdk_pkg_lib_candidates)
    list(REVERSE windows_sdk_pkg_lib_candidates)
    list(LENGTH windows_sdk_pkg_lib_candidates windows_sdk_pkg_lib_candidates_count)
    if (windows_sdk_pkg_lib_candidates_count GREATER 0)
        list(GET windows_sdk_pkg_lib_candidates 0 d3dcompiler47_lib)
    endif ()
endif ()

if (d3dcompiler47_dll STREQUAL "")
    file(GLOB windows_kits_dll_candidates
        "C:/Program Files (x86)/Windows Kits/10/bin/*/${d3dcompiler_arch}/d3dcompiler_47.dll"
    )
    list(SORT windows_kits_dll_candidates)
    list(REVERSE windows_kits_dll_candidates)
    list(LENGTH windows_kits_dll_candidates windows_kits_dll_candidates_count)
    if (windows_kits_dll_candidates_count GREATER 0)
        list(GET windows_kits_dll_candidates 0 d3dcompiler47_dll)
    endif ()
endif ()

if (d3dcompiler47_lib STREQUAL "")
    file(GLOB windows_kits_lib_candidates
        "C:/Program Files (x86)/Windows Kits/10/Lib/*/um/${d3dcompiler_arch}/d3dcompiler.lib"
    )
    list(SORT windows_kits_lib_candidates)
    list(REVERSE windows_kits_lib_candidates)
    list(LENGTH windows_kits_lib_candidates windows_kits_lib_candidates_count)
    if (windows_kits_lib_candidates_count GREATER 0)
        list(GET windows_kits_lib_candidates 0 d3dcompiler47_lib)
    endif ()
endif ()

if (d3dcompiler47_lib STREQUAL "")
    set(d3dcompiler47_lib "d3dcompiler.lib")
endif ()

if (d3dcompiler47_dll STREQUAL "" OR NOT EXISTS "${d3dcompiler47_dll}")
    message(FATAL_ERROR "Cannot find d3dcompiler_47.dll in Microsoft.Windows.SDK.CPP package or local Windows Kits")
endif()

# import

set_target_properties(Microsoft.D3DCompiler.Redist PROPERTIES
    IMPORTED_IMPLIB "${d3dcompiler47_lib}"
    IMPORTED_LOCATION "${d3dcompiler47_dll}"
)
