cmake_minimum_required(VERSION 3.20)
# `CMAKE_CUDA_ARCHITECTURES` requires 3.18
# `set_source_files_properties` requires 3.18
# `cmake_path(CONVERT ... TO_NATIVE_PATH_LIST ...)` requires 3.20
# todo - set this conditionally

project(Apollo VERSION 0.0.0
        DESCRIPTION "Self-hosted game stream host for Artemis"
        HOMEPAGE_URL "https://github.com/ClassicOldSong/Apollo")

set(PROJECT_LICENSE "GPL-3.0-only")

set(PROJECT_FQDN "com.SudoMaker.dev.Apollo")

set(PROJECT_BRIEF_DESCRIPTION "GameStream host for Artemis")  # must be <= 35 characters

set(PROJECT_LONG_DESCRIPTION "Offering low latency, cloud gaming server capabilities with support for AMD, Intel, \
and Nvidia GPUs for hardware encoding. Software encoding is also available. You can connect to Apollo from any \
Artemis client on a variety of devices. A web UI is provided to allow configuration, and client pairing, from \
your favorite web browser. Pair from the local server or any mobile device.")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to 'Release' as none was specified.")
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
endif()

# set the module path, used for includes
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# export compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# set version info for this build
include(${CMAKE_MODULE_PATH}/prep/build_version.cmake)

# cmake build flags
include(${CMAKE_MODULE_PATH}/prep/options.cmake)

# initial prep
include(${CMAKE_MODULE_PATH}/prep/init.cmake)

# configure special package files, such as sunshine.desktop, Flatpak manifest, Portfile , etc.
include(${CMAKE_MODULE_PATH}/prep/special_package_configuration.cmake)

# Exit early if END_BUILD is ON, i.e. when only generating package manifests
if(${END_BUILD})
    return()
endif()

# project constants
include(${CMAKE_MODULE_PATH}/prep/constants.cmake)

# load macros
include(${CMAKE_MODULE_PATH}/macros/common.cmake)

# load dependencies
include(${CMAKE_MODULE_PATH}/dependencies/common.cmake)

# setup compile definitions
include(${CMAKE_MODULE_PATH}/compile_definitions/common.cmake)

# target definitions
include(${CMAKE_MODULE_PATH}/targets/common.cmake)

# packaging
include(${CMAKE_MODULE_PATH}/packaging/common.cmake)
