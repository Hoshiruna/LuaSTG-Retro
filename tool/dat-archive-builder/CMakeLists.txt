set(tool_name dat-archive-builder)

add_executable(${tool_name})
target_compile_options(${tool_name} PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
        "$<$<CXX_COMPILER_ID:MSVC>:/sdl>"
        "$<$<CXX_COMPILER_ID:MSVC>:/W4>"
)
set_target_properties(${tool_name} PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)
target_sources(${tool_name} PRIVATE main.cpp)
target_link_libraries(${tool_name} PRIVATE Core.FileSystem)

set_target_properties(${tool_name} PROPERTIES FOLDER tool)
