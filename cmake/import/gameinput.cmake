CPMAddPackage(
    NAME Microsoft.GameInput
    VERSION 4.1.313
    URL https://www.nuget.org/api/v2/package/Microsoft.GameInput/4.1.313
    URL_HASH SHA256=8604DC4FB268E8CB7D785863C55C79AC2FA5370D3E5C605EB38896406D315127
    DOWNLOAD_ONLY YES
)

set(gameinput_package "${Microsoft.GameInput_SOURCE_DIR}")
add_library(Microsoft.GameInput STATIC IMPORTED GLOBAL)
set_target_properties(Microsoft.GameInput PROPERTIES
    IMPORTED_LOCATION "${gameinput_package}/native/lib/x64/GameInput.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${gameinput_package}/native/include"
)

set(LUASTG_GAMEINPUT_RUNTIME_FILES
    "${gameinput_package}/native/bin/x64/GameInputRedist.dll"
    "${gameinput_package}/native/bin/x64/GameInputRawInputProxy.exe"
)
set(LUASTG_GAMEINPUT_LICENSE_FILES
    "${gameinput_package}/LICENSE.txt"
    "${gameinput_package}/NOTICE.txt"
)
