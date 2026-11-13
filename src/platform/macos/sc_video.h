/**
 * @file src/platform/macos/sc_video.h
 * @brief Display capture through ScreenCaptureKit, with the same interface as AVVideo.
 */
#pragma once

#import <AppKit/AppKit.h>
#import <CoreMedia/CoreMedia.h>
#import <ScreenCaptureKit/ScreenCaptureKit.h>

#import "av_video.h"  // FrameCallbackBlock

/**
 * Captures one display. Unlike AVCaptureScreenInput it works on CGVirtualDisplay displays, and it takes
 * the display's pixel size from ScreenCaptureKit, because CGDisplayCopyDisplayMode() returns NULL for a
 * virtual display in the process that created it.
 */
API_AVAILABLE(macos(14.0))
@interface SCVideo: NSObject <SCStreamOutput, SCStreamDelegate, FrameSource>

@property (nonatomic, assign) CGDirectDisplayID displayID;
@property (nonatomic, assign) int frameRate;
@property (nonatomic, assign) OSType pixelFormat;
@property (nonatomic, assign) int frameWidth;
@property (nonatomic, assign) int frameHeight;

+ (BOOL)isAvailable;
- (id)initWithDisplay:(CGDirectDisplayID)displayID frameRate:(int)frameRate;
- (void)setFrameWidth:(int)frameWidth frameHeight:(int)frameHeight;
/**
 * Starts delivering frames to frameCallback until it returns false or the stream fails.
 * @return A semaphore signalled when delivery stops.
 */
- (dispatch_semaphore_t)capture:(FrameCallbackBlock)frameCallback;
- (void)stop;

@end
