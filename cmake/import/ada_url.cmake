# ada_url

CPMAddPackage(
    NAME ada_url
    VERSION 4.0.0
    URL https://github.com/ada-url/ada/releases/download/v4.0.0/singleheader.zip
    URL_HASH SHA256=D7FA5AEA7B3805C91111231D1AAAEEFC39AE20562A9F599E13C22471EE576B9F
    DOWNLOAD_ONLY YES
)

if(ada_url_ADDED)
    add_library(ada_url STATIC)
    luastg_target_common_options(ada_url)
    target_include_directories(ada_url PUBLIC
        ${ada_url_SOURCE_DIR}
    )
    target_sources(ada_url PRIVATE
        ${ada_url_SOURCE_DIR}/ada.h
        ${ada_url_SOURCE_DIR}/ada.cpp
        ${ada_url_SOURCE_DIR}/ada_c.h
    )
    set_target_properties(ada_url PROPERTIES FOLDER external)
endif()
