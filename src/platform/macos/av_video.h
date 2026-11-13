/**
 * @file src/platform/macos/av_video.h
 * @brief Declarations for video capture on macOS.
 */
#pragma once

// platform includes
#import <AppKit/AppKit.h>
#import <AVFoundation/AVFoundation.h>

struct CaptureSession {
  AVCaptureVideoDataOutput *output;
  NSCondition *captureStopped;
};

typedef bool (^FrameCallbackBlock)(CMSampleBufferRef);

/**
 * What display.mm and nv12_zero_device need from a capture backend (AVVideo or SCVideo).
 */
@protocol FrameSource <NSObject>
@property (nonatomic, assign) OSType pixelFormat;
@property (nonatomic, assign) int frameWidth;
@property (nonatomic, assign) int frameHeight;
- (void)setFrameWidth:(int)frameWidth frameHeight:(int)frameHeight;
- (dispatch_semaphore_t)capture:(FrameCallbackBlock)frameCallback;
@end

@interface AVVideo: NSObject <AVCaptureVideoDataOutputSampleBufferDelegate, FrameSource>

#define kMaxDisplays 32

@property (nonatomic, assign) CGDirectDisplayID displayID;
@property (nonatomic, assign) CMTime minFrameDuration;
@property (nonatomic, assign) OSType pixelFormat;
@property (nonatomic, assign) int frameWidth;
@property (nonatomic, assign) int frameHeight;


@property (nonatomic, assign) AVCaptureSession *session;
@property (nonatomic, assign) NSMapTable<AVCaptureConnection *, AVCaptureVideoDataOutput *> *videoOutputs;
@property (nonatomic, assign) NSMapTable<AVCaptureConnection *, FrameCallbackBlock> *captureCallbacks;
@property (nonatomic, assign) NSMapTable<AVCaptureConnection *, dispatch_semaphore_t> *captureSignals;

+ (NSArray<NSDictionary *> *)displayNames;
+ (NSString *)getDisplayName:(CGDirectDisplayID)displayID;

- (id)initWithDisplay:(CGDirectDisplayID)displayID frameRate:(int)frameRate;

- (void)setFrameWidth:(int)frameWidth frameHeight:(int)frameHeight;
- (dispatch_semaphore_t)capture:(FrameCallbackBlock)frameCallback;

@end
