set(luastg_cpu_test_targets Core.ReferenceCounted.Test Core.InputSystem.Test)
if (LUASTG_BUILD_RUNTIME)
    list(APPEND luastg_cpu_test_targets Core.FileSystem.Test Core.Image.Test Core.Http.Test)
endif ()

foreach(test_target IN LISTS luastg_cpu_test_targets)
    # HTTP already has a CTest runner that starts its own local server.
    if (NOT test_target STREQUAL "Core.Http.Test")
        set(test_directory "${CMAKE_BINARY_DIR}/cpu-tests/${test_target}")
        file(MAKE_DIRECTORY "${test_directory}")
        add_test(NAME ${test_target} COMMAND $<TARGET_FILE:${test_target}>)
        set_tests_properties(${test_target} PROPERTIES
            LABELS cpu
            WORKING_DIRECTORY "${test_directory}"
            TIMEOUT 120
        )
        if (test_target STREQUAL "Core.Image.Test")
            file(COPY "${PROJECT_SOURCE_DIR}/engine/image/test/assets" DESTINATION "${test_directory}")
        endif ()
    endif ()
endforeach()

if (LUASTG_USE_SDLGPU)
    list(APPEND luastg_cpu_test_targets Core.Graphics.SDLGPU.ShaderTest)
endif ()

add_custom_target(luastg-cpu-tests DEPENDS ${luastg_cpu_test_targets})
set_target_properties(luastg-cpu-tests PROPERTIES FOLDER engine/test)
