# Video

set(lib_src core/VideoDecoder.hpp)
if (WIN32)
    list(APPEND lib_src
        backend/VideoDecoder.cpp
        backend/VideoDecoderMediaFoundation.hpp
        backend/VideoDecoderMediaFoundation.cpp
    )
else ()
    list(APPEND lib_src backend/VideoDecoderUnavailable.cpp)
endif ()
source_group(TREE ${CMAKE_CURRENT_LIST_DIR} FILES ${lib_src})

set(lib_name "Core.Video")

add_library(${lib_name})
luastg_target_common_options(${lib_name})
luastg_target_more_warning(${lib_name})
target_include_directories(${lib_name} PUBLIC .)
target_sources(${lib_name} PRIVATE ${lib_src})
target_link_libraries(${lib_name} PUBLIC
    Core.ReferenceCounted
    Core.FileSystem
    Core.Logging
    utf8
)

if (WIN32)
    target_link_libraries(${lib_name} PRIVATE
        win32
        mfplat.lib
        mfreadwrite.lib
        mfuuid.lib
        ole32.lib
    )
endif ()
target_link_libraries(${lib_name} PRIVATE options_compile_utf8)

set_target_properties(${lib_name} PROPERTIES FOLDER engine)
