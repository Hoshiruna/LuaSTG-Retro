# efsw 1.6.3 (MIT).
CPMAddPackage(
    NAME efsw
    VERSION 1.6.3
    GITHUB_REPOSITORY SpartanJ/efsw
    GIT_TAG 1.6.3
    OPTIONS
        "BUILD_SHARED_LIBS OFF"
        "BUILD_STATIC_LIBS OFF"
        "BUILD_TEST_APP OFF"
        "EFSW_INSTALL OFF"
)
set_target_properties(efsw PROPERTIES FOLDER external)

set(LUASTG_EFSW_NOTICE "${efsw_SOURCE_DIR}/LICENSE")
