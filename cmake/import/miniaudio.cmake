# miniaudio (public domain or MIT-0, at the user's choice)

CPMAddPackage(
    NAME miniaudio
    VERSION 0.11.25
    GITHUB_REPOSITORY mackron/miniaudio
    GIT_TAG 0.11.25
    DOWNLOAD_ONLY YES
)

if(miniaudio_ADDED)
    add_library(miniaudio STATIC)
    luastg_target_common_options(miniaudio)
    target_include_directories(miniaudio PUBLIC
        ${miniaudio_SOURCE_DIR}
        ${miniaudio_SOURCE_DIR}/extras/decoders/libvorbis
    )
    target_compile_definitions(miniaudio PUBLIC
        MA_NO_MP3
        MA_NO_VORBIS
    )
    target_sources(miniaudio PRIVATE
        ${miniaudio_SOURCE_DIR}/miniaudio.c
        ${miniaudio_SOURCE_DIR}/miniaudio.h
        ${miniaudio_SOURCE_DIR}/extras/decoders/libvorbis/miniaudio_libvorbis.c
        ${miniaudio_SOURCE_DIR}/extras/decoders/libvorbis/miniaudio_libvorbis.h
    )
    target_link_libraries(miniaudio PUBLIC
        Vorbis::vorbis
        Vorbis::vorbisfile
    )
    set_target_properties(miniaudio PROPERTIES FOLDER external)
endif()
