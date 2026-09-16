# libcurl (curl license). Use the OS certificate store on Windows.
if (WIN32)
    set(luastg_curl_tls "CURL_USE_SCHANNEL ON" "CURL_USE_OPENSSL OFF")
else ()
    find_package(OpenSSL REQUIRED)
    set(luastg_curl_tls "CURL_USE_SCHANNEL OFF" "CURL_USE_OPENSSL ON")
endif ()

CPMAddPackage(
    NAME libcurl
    VERSION 8.22.0
    GITHUB_REPOSITORY curl/curl
    GIT_TAG curl-8_22_0
    OPTIONS
        "BUILD_CURL_EXE OFF"
        "BUILD_SHARED_LIBS OFF"
        "BUILD_STATIC_LIBS ON"
        "BUILD_TESTING OFF"
        "BUILD_EXAMPLES OFF"
        "CURL_DISABLE_INSTALL ON"
        "CURL_ENABLE_EXPORT_TARGET OFF"
        "CURL_STATIC_CRT ON"
        "BUILD_LIBCURL_DOCS OFF"
        "BUILD_MISC_DOCS OFF"
        "ENABLE_CURL_MANUAL OFF"
        "HTTP_ONLY ON"
        "ENABLE_THREADED_RESOLVER ON"
        "ENABLE_ARES OFF"
        "CURL_USE_LIBPSL OFF"
        "CURL_USE_LIBSSH2 OFF"
        "CURL_USE_LIBSSH OFF"
        "CURL_USE_GSSAPI OFF"
        "USE_LIBIDN2 OFF"
        "USE_NGHTTP2 OFF"
        "CURL_ZLIB OFF"
        "CURL_BROTLI OFF"
        "CURL_ZSTD OFF"
        ${luastg_curl_tls}
)
unset(luastg_curl_tls)
set(LUASTG_CURL_NOTICE "${libcurl_SOURCE_DIR}/COPYING")
