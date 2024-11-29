include(FetchContent)

FetchContent_Declare(
    fmt DOWNLOAD_EXTRACT_TIMESTAMP FALSE URL https://github.com/fmtlib/fmt/archive/refs/tags/11.0.2.tar.gz
    URL_HASH SHA256=6cb1e6d37bdcb756dbbe59be438790db409cdb4868c66e888d5df9f13f7c027f)
FetchContent_GetProperties(fmt)
if(NOT fmt_POPULATED)
    FetchContent_Populate(fmt)
    # We do not require os features of fmt
    set(FMT_OS OFF CACHE BOOL "Disable FMT_OS" FORCE)
    add_subdirectory(${fmt_SOURCE_DIR} ${fmt_BINARY_DIR})
    set_target_properties(fmt PROPERTIES FOLDER "third-party")
endif()
