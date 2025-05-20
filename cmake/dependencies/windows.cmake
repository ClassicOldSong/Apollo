# windows specific dependencies

# Make sure MinHook is installed
find_library(MINHOOK_LIBRARY libMinHook.a QUIET)
find_path(MINHOOK_INCLUDE_DIR MinHook.h PATH_SUFFIXES include QUIET)
if (MINHOOK_LIBRARY AND MINHOOK_INCLUDE_DIR)
    add_library(minhook::minhook STATIC IMPORTED)
    set_property(TARGET minhook::minhook PROPERTY IMPORTED_LOCATION ${MINHOOK_LIBRARY})
    target_include_directories(minhook::minhook INTERFACE ${MINHOOK_INCLUDE_DIR})
else()
    # Use CMAKE package resolution instead of the system
    find_package(minhook REQUIRED)
endif()
