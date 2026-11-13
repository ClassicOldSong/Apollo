# macos specific dependencies

FIND_LIBRARY(APP_KIT_LIBRARY AppKit)
FIND_LIBRARY(APP_SERVICES_LIBRARY ApplicationServices)
FIND_LIBRARY(AV_FOUNDATION_LIBRARY AVFoundation)
FIND_LIBRARY(CORE_MEDIA_LIBRARY CoreMedia)
FIND_LIBRARY(CORE_VIDEO_LIBRARY CoreVideo)
FIND_LIBRARY(FOUNDATION_LIBRARY Foundation)
FIND_LIBRARY(VIDEO_TOOLBOX_LIBRARY VideoToolbox)
FIND_LIBRARY(SCREEN_CAPTURE_KIT_LIBRARY ScreenCaptureKit)

if(SUNSHINE_ENABLE_TRAY)
    FIND_LIBRARY(COCOA Cocoa REQUIRED)
endif()

# Homebrew opus: sources include <opus/...>, and the library is linked by name
find_path(OPUS_INCLUDE_DIR opus/opus_multistream.h REQUIRED)
find_library(OPUS_LIBRARY opus REQUIRED)
include_directories(SYSTEM "${OPUS_INCLUDE_DIR}")
get_filename_component(OPUS_LIBRARY_DIR "${OPUS_LIBRARY}" DIRECTORY)
link_directories("${OPUS_LIBRARY_DIR}")
