# simdutf

CPMAddPackage(
    NAME simdutf
    VERSION 9.0.0
    URL https://github.com/simdutf/simdutf/releases/download/v9.0.0/singleheader.zip
    URL_HASH SHA256=C47C68CD51025EC66509BC36215B4C4F1F0F0A98129139EE55C541531B652526
    DOWNLOAD_ONLY YES
)

if (NOT simdutf_ADDED)
    message(FATAL_ERROR "simdutf is requied")
endif ()

set(_root ${simdutf_SOURCE_DIR})

add_library(simdutf STATIC)
add_library(simdutf::simdutf ALIAS simdutf)
target_compile_features(simdutf PRIVATE c_std_17 cxx_std_20)
target_include_directories(simdutf PUBLIC ${_root})
target_sources(simdutf PRIVATE ${_root}/simdutf.h ${_root}/simdutf.cpp)

set_target_properties(simdutf PROPERTIES FOLDER external)
