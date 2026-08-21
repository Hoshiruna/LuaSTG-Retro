function(luastg_target_common_options __TARGET__)
    if (MSVC)
        target_compile_options(${__TARGET__} PRIVATE
            /MP
            /utf-8
            "$<$<CONFIG:Debug>:/ZI>"
        )
    endif ()
    set_target_properties(${__TARGET__} PROPERTIES
        C_STANDARD 17
        C_STANDARD_REQUIRED ON
        C_EXTENSIONS OFF
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )
    if (WIN32)
        target_compile_definitions(${__TARGET__} PRIVATE
            _UNICODE
            UNICODE
            WINVER=0x0A00
            _WIN32_WINNT=0x0A00
            NTDDI_VERSION=0x0A000006
        )
    endif ()
endfunction()

function(luastg_target_common_options2 __TARGET__)
    if (MSVC)
        target_compile_options(${__TARGET__} PRIVATE
            /MP
            /utf-8
            "$<$<CONFIG:Debug>:/ZI>"
        )
    endif ()
    set_target_properties(${__TARGET__} PROPERTIES
        C_STANDARD 17
        C_STANDARD_REQUIRED ON
        C_EXTENSIONS OFF
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )
endfunction()

function(luastg_target_more_warning __TARGET__)
    if (MSVC)
        target_compile_options(${__TARGET__} PRIVATE /W4)
    elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(${__TARGET__} PRIVATE -Wall -Wextra -Wpedantic)
    endif ()
endfunction()

function(luastg_target_copy_to_output_directory __AFTER_TARGET__ __TARGET__)
    add_custom_command(TARGET ${__AFTER_TARGET__} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/engine
        COMMAND ${CMAKE_COMMAND} -E rm -f ${CMAKE_SOURCE_DIR}/engine/"$<TARGET_FILE_NAME:${__TARGET__}>"
        COMMAND ${CMAKE_COMMAND} -E copy  "$<TARGET_FILE:${__TARGET__}>" ${CMAKE_SOURCE_DIR}/engine
    )
endfunction()

function(luastg_target_copy_to_bin_directory after_target target)
    add_custom_command(TARGET ${after_target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/bin
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:${target}>" ${CMAKE_BINARY_DIR}/bin/
    )
endfunction()
