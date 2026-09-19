# D3D11's shader compiler and redistributable runtime.
add_library(Microsoft.D3DCompiler.Redist SHARED IMPORTED GLOBAL)

set(d3dcompiler_arch "x64")
set(d3dcompiler47_dll "")
set(d3dcompiler47_lib "")

# Prefer the SDK selected by Visual Studio. FetchContent download failures are
# fatal, so local discovery must happen before the NuGet fallback.
file(TO_CMAKE_PATH "$ENV{WindowsSdkDir}" visual_studio_windows_sdk_root)
file(TO_CMAKE_PATH "$ENV{WindowsSDKLibVersion}" visual_studio_windows_sdk_version)
string(REGEX REPLACE "/+$" "" visual_studio_windows_sdk_root "${visual_studio_windows_sdk_root}")
string(REGEX REPLACE "/+$" "" visual_studio_windows_sdk_version "${visual_studio_windows_sdk_version}")

set(windows_kits_roots
    "${visual_studio_windows_sdk_root}"
    "C:/Program Files (x86)/Windows Kits/10"
)
foreach(sdk_root IN LISTS windows_kits_roots)
    if (sdk_root STREQUAL "")
        continue()
    endif ()

    set(sdk_dll_candidate "${sdk_root}/Redist/D3D/${d3dcompiler_arch}/d3dcompiler_47.dll")
    if (visual_studio_windows_sdk_version STREQUAL "")
        file(GLOB sdk_version_directories LIST_DIRECTORIES TRUE "${sdk_root}/Lib/*")
        list(SORT sdk_version_directories COMPARE NATURAL ORDER DESCENDING)
    else ()
        set(sdk_version_directories "${sdk_root}/Lib/${visual_studio_windows_sdk_version}")
    endif ()

    foreach(sdk_version_directory IN LISTS sdk_version_directories)
        set(sdk_lib_candidate "${sdk_version_directory}/um/${d3dcompiler_arch}/d3dcompiler.lib")
        if (EXISTS "${sdk_dll_candidate}" AND EXISTS "${sdk_lib_candidate}")
            set(d3dcompiler47_dll "${sdk_dll_candidate}")
            set(d3dcompiler47_lib "${sdk_lib_candidate}")
            break()
        endif ()
    endforeach ()
    if (NOT d3dcompiler47_dll STREQUAL "")
        break()
    endif ()
endforeach ()

if (d3dcompiler47_dll STREQUAL "" OR d3dcompiler47_lib STREQUAL "")
    # Keep the pinned NuGet SDK for machines without a local SDK redist.
    CPMAddPackage(
        NAME Microsoft.Windows.SDK.CPP
        VERSION 10.0.22621.3233
        URL https://www.nuget.org/api/v2/package/Microsoft.Windows.SDK.CPP/10.0.22621.3233
        URL_HASH SHA256=E4EFE1768EA61F4F999DBEF61B09895320629F975F9CEED8290A9633E0C31623
        DOWNLOAD_ONLY YES
    )
    set(windows_sdk_pkg_root "${Microsoft.Windows.SDK.CPP_SOURCE_DIR}")
    file(GLOB sdk_dll_candidates
        "${windows_sdk_pkg_root}/c/Redist/D3D/${d3dcompiler_arch}/d3dcompiler_47.dll"
        "${windows_sdk_pkg_root}/c/Redist/*/D3D/${d3dcompiler_arch}/d3dcompiler_47.dll"
    )
    file(GLOB sdk_lib_candidates
        "${windows_sdk_pkg_root}/c/Lib/*/um/${d3dcompiler_arch}/d3dcompiler.lib"
        "${windows_sdk_pkg_root}/c/lib/*/um/${d3dcompiler_arch}/d3dcompiler.lib"
    )
    list(SORT sdk_dll_candidates COMPARE NATURAL ORDER DESCENDING)
    list(SORT sdk_lib_candidates COMPARE NATURAL ORDER DESCENDING)
    list(LENGTH sdk_dll_candidates sdk_dll_candidate_count)
    list(LENGTH sdk_lib_candidates sdk_lib_candidate_count)
    if (sdk_dll_candidate_count GREATER 0 AND sdk_lib_candidate_count GREATER 0)
        list(GET sdk_dll_candidates 0 d3dcompiler47_dll)
        list(GET sdk_lib_candidates 0 d3dcompiler47_lib)
    endif ()
endif ()

if (NOT EXISTS "${d3dcompiler47_dll}" OR NOT EXISTS "${d3dcompiler47_lib}")
    message(FATAL_ERROR "Cannot find the x64 D3DCompiler redistributable and import library. Install the Windows SDK component in Visual Studio Installer.")
endif ()

# Windows SDK 10.0.26100 and newer redists require UCRT, available on Windows 10+.
message(STATUS "D3DCompiler runtime: ${d3dcompiler47_dll}")
message(STATUS "D3DCompiler import library: ${d3dcompiler47_lib}")
set_target_properties(Microsoft.D3DCompiler.Redist PROPERTIES
    IMPORTED_IMPLIB "${d3dcompiler47_lib}"
    IMPORTED_LOCATION "${d3dcompiler47_dll}"
)
