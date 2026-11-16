# DirectXMath is header-only and uses the same API on Unix platforms.
CPMAddPackage(
    NAME luastg_directxmath
    GITHUB_REPOSITORY microsoft/DirectXMath
    GIT_TAG oct2024
    DOWNLOAD_ONLY YES
)

# DirectXMath's documented Unix build uses CoreRT's standalone SAL header.
# CoreRT is archived. Keep its original license beside the downloaded header.
set(luastg_sal_directory "${CMAKE_BINARY_DIR}/directxmath-sal")
file(MAKE_DIRECTORY "${luastg_sal_directory}")
foreach(luastg_sal_file IN ITEMS sal.h LICENSE.TXT)
    if (luastg_sal_file STREQUAL "sal.h")
        set(luastg_sal_source src/Native/inc/unix/sal.h)
    else ()
        set(luastg_sal_source LICENSE.TXT)
    endif ()
    if (NOT EXISTS "${luastg_sal_directory}/${luastg_sal_file}")
        file(DOWNLOAD
            "https://raw.githubusercontent.com/dotnet/corert/master/${luastg_sal_source}"
            "${luastg_sal_directory}/${luastg_sal_file}"
            STATUS luastg_sal_status TLS_VERIFY ON)
        list(GET luastg_sal_status 0 luastg_sal_code)
        if (NOT luastg_sal_code EQUAL 0)
            file(REMOVE "${luastg_sal_directory}/${luastg_sal_file}")
            message(FATAL_ERROR "Cannot download DirectXMath's SAL dependency: ${luastg_sal_status}")
        endif ()
    endif ()
endforeach()

add_library(luastg_directxmath_headers INTERFACE)
target_compile_definitions(luastg_directxmath_headers INTERFACE PAL_STDCPP_COMPAT)
target_include_directories(luastg_directxmath_headers INTERFACE
    "${luastg_directxmath_SOURCE_DIR}/Inc"
    "${luastg_sal_directory}"
)
