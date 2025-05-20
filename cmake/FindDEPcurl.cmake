find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(CURL REQUIRED libcurl)
else()
    # Use CMAKE package resolution instead of the system
    find_package(CURL REQUIRED)
endif()
