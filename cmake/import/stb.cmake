# nothings stb

CPMAddPackage(
    NAME nothings_stb
    VERSION 2026.8.2
    URL https://github.com/nothings/stb/archive/2c980bb59875b0d32144a71867fbdebb2f77cd20.zip
    URL_HASH SHA256=8E59F72B0780690CDA64726804269F638A3BE77B9D1506EA95F443F7964BCCF0
    DOWNLOAD_ONLY YES
)

if(nothings_stb_ADDED)
    add_library(nothings_stb STATIC)
    if (NOT EXISTS ${CMAKE_BINARY_DIR}/nothings_stb)
        file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/nothings_stb)
    endif ()
    if (NOT EXISTS ${CMAKE_BINARY_DIR}/nothings_stb/include)
        file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/nothings_stb/include)
    endif ()
    target_include_directories(nothings_stb PUBLIC
        ${nothings_stb_SOURCE_DIR}
        ${CMAKE_BINARY_DIR}/nothings_stb/include
    )
    if (NOT EXISTS ${CMAKE_BINARY_DIR}/nothings_stb/include/stb_vorbis.h)
        file(WRITE ${CMAKE_BINARY_DIR}/nothings_stb/include/stb_vorbis.h
            "#define STB_VORBIS_HEADER_ONLY\n"
            "#include \"stb_vorbis.c\"\n"
            "#undef STB_VORBIS_HEADER_ONLY\n"
        )
    endif ()
    if (NOT EXISTS ${CMAKE_BINARY_DIR}/nothings_stb/nothings_stb.c)
        file(WRITE ${CMAKE_BINARY_DIR}/nothings_stb/nothings_stb.c
            "#define STB_IMAGE_IMPLEMENTATION\n"
            "#include \"stb_image.h\"\n"
            "#include \"stb_vorbis.c\"\n"
        )
    endif ()
    target_sources(nothings_stb PRIVATE
        ${CMAKE_BINARY_DIR}/nothings_stb/include/stb_vorbis.h
        ${nothings_stb_SOURCE_DIR}/stb_image.h
        ${CMAKE_BINARY_DIR}/nothings_stb/nothings_stb.c
    )
    set_target_properties(nothings_stb PROPERTIES FOLDER external)
endif()
