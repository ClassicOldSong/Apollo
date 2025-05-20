# windows specific dependencies

#
# Loads the MinHook library giving the priority to the system package first, with a fallback
# to the submodule.
#
include_guard(GLOBAL)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(lib_suffix "x64")
else()
    set(lib_suffix "x86")
endif()

find_library(MINHOOK_LIBRARY NAME libMinHook libMinHook.${lib_suffix} PATH_SUFFIXES lib)
find_path(MINHOOK_INCLUDE_DIR MinHook.h PATH_SUFFIXES include)
if (NOT MINHOOK_LIBRARY OR NOT MINHOOK_INCLUDE_DIR)
    message(STATUS "minhook v1.3.4 package not found in the system. Falling back to FetchContent.")
    include(FetchContent)

    if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
        cmake_policy(SET CMP0135 NEW)  # Avoid warning about DOWNLOAD_EXTRACT_TIMESTAMP in CMake 3.24
    endif()
    
    FetchContent_Declare(
        minhook
        URL      https://github.com/TsudaKageyu/minhook/releases/download/v1.3.4/MinHook_134_lib.zip
        URL_HASH SHA256=8dba3053104942b72208330395f9107910d306eb89263b3f2b3d0747a40c92ac
        DOWNLOAD_EXTRACT_TIMESTAMP
    )
    FetchContent_MakeAvailable(minhook)

    find_library(MINHOOK_LIBRARY
        NAMES libMinHook.${lib_suffix}
        PATHS "${minhook_SOURCE_DIR}/lib"
        NO_DEFAULT_PATH
    )
    find_path(MINHOOK_INCLUDE_DIR
        NAMES MinHook.h
        PATHS "${minhook_SOURCE_DIR}/include"
        NO_DEFAULT_PATH
    )

    if(NOT MINHOOK_LIBRARY)
        message(FATAL_ERROR "MinHook library not found in fetched content: ${minhook_SOURCE_DIR}/lib")
    endif()
    if(NOT MINHOOK_INCLUDE_DIR)
        message(FATAL_ERROR "MinHook headers not found in fetched content: ${minhook_SOURCE_DIR}/include")
    endif()
endif()

add_library(minhook::minhook STATIC IMPORTED)
set_target_properties(minhook::minhook PROPERTIES
    IMPORTED_LOCATION "${MINHOOK_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${MINHOOK_INCLUDE_DIR}"
)
