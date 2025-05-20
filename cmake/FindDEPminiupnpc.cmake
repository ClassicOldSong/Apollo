find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(MINIUPNP miniupnpc REQUIRED)
    include_directories(SYSTEM ${MINIUPNP_INCLUDE_DIRS})
else()
    # Use CMAKE package resolution instead of the system
    find_package(miniupnpc REQUIRED)
endif()
