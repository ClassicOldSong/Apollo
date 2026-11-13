/**
 * @file src/platform/macos/sc_video.mm
 * @brief Display capture through ScreenCaptureKit. Built with ARC.
 */
#import "sc_video.h"

#if !__has_feature(objc_arc)
  #error sc_video.mm must be built with -fobjc-arc
#endif

static const int64_t repeat_after = 100 * NSEC_PER_MSEC;

@implementation SCVideo {
  SCContentFilter *_filter;
  SCStream *_stream;
  dispatch_queue_t _queue;
  FrameCallbackBlock _callback;
  dispatch_semaphore_t _signal;
  dispatch_source_t _repeat;
  CMSampleBufferRef _last;  // last complete frame, retained
  uint64_t _lastDelivered;  // dispatch_time of the last callback
}

+ (BOOL)isAvailable {
  if (@available(macOS 14.0, *)) {
    return YES;
  }
  return NO;
}

// SCShareableContent is asynchronous; a display made a moment ago can take a little while to appear.
static SCDisplay *find_display(CGDirectDisplayID displayID) {
  for (int attempt = 0; attempt < 30; attempt++) {
    __block SCDisplay *found = nil;
    dispatch_semaphore_t done = dispatch_semaphore_create(0);
    [SCShareableContent getShareableContentExcludingDesktopWindows:NO
                                               onScreenWindowsOnly:YES
                                                 completionHandler:^(SCShareableContent *content, NSError *error) {
                                                   for (SCDisplay *d in content.displays) {
                                                     if (d.displayID == displayID) {
                                                       found = d;
                                                     }
                                                   }
                                                   dispatch_semaphore_signal(done);
                                                 }];
    dispatch_semaphore_wait(done, dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC));
    if (found) {
      return found;
    }
    [NSThread sleepForTimeInterval:0.1];
  }
  return nil;
}

- (id)initWithDisplay:(CGDirectDisplayID)displayID frameRate:(int)frameRate {
  self = [super init];
  if (!self) {
    return nil;
  }
  SCDisplay *display = find_display(displayID);
  if (!display) {
    return nil;
  }
  _filter = [[SCContentFilter alloc] initWithDisplay:display excludingWindows:@[]];
  CGRect rect = _filter.contentRect;
  float scale = _filter.pointPixelScale;

  self.displayID = displayID;
  self.frameRate = frameRate > 0 ? frameRate : 60;
  self.pixelFormat = kCVPixelFormatType_32BGRA;
  self.frameWidth = (int) lround(rect.size.width * scale);
  self.frameHeight = (int) lround(rect.size.height * scale);
  _queue = dispatch_queue_create("apollo.sc-video", dispatch_queue_attr_make_with_qos_class(DISPATCH_QUEUE_SERIAL, QOS_CLASS_USER_INTERACTIVE, 0));
  return self;
}

- (void)setFrameWidth:(int)frameWidth frameHeight:(int)frameHeight {
  _frameWidth = frameWidth;
  _frameHeight = frameHeight;
}

- (SCStreamConfiguration *)configuration {
  SCStreamConfiguration *config = [[SCStreamConfiguration alloc] init];
  config.width = self.frameWidth;
  config.height = self.frameHeight;
  config.minimumFrameInterval = CMTimeMake(1, self.frameRate);
  config.pixelFormat = self.pixelFormat;
  config.showsCursor = YES;
  config.queueDepth = 5;
  config.preservesAspectRatio = YES;
  if (self.pixelFormat == kCVPixelFormatType_32BGRA) {
    config.colorSpaceName = kCGColorSpaceSRGB;
  } else {
    config.colorMatrix = kCGDisplayStreamYCbCrMatrix_ITU_R_709_2;
  }
  return config;
}

- (dispatch_semaphore_t)capture:(FrameCallbackBlock)frameCallback {
  @synchronized(self) {
    [self stopLocked];
    _callback = frameCallback;
    _signal = dispatch_semaphore_create(0);
    dispatch_semaphore_t signal = _signal;

    _stream = [[SCStream alloc] initWithFilter:_filter configuration:[self configuration] delegate:self];
    NSError *error = nil;
    if (![_stream addStreamOutput:self type:SCStreamOutputTypeScreen sampleHandlerQueue:_queue error:&error]) {
      NSLog(@"SCVideo: addStreamOutput failed: %@", error);
      _stream = nil;
      return nil;
    }
    dispatch_semaphore_t started = dispatch_semaphore_create(0);
    __block NSError *startError = nil;
    [_stream startCaptureWithCompletionHandler:^(NSError *e) {
      startError = e;
      dispatch_semaphore_signal(started);
    }];
    dispatch_semaphore_wait(started, dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC));
    if (startError) {
      NSLog(@"SCVideo: startCapture failed: %@", startError);
      _stream = nil;
      return nil;
    }

    // ScreenCaptureKit sends nothing while the screen is still, but the callback is also how capture
    // learns it should stop, so repeat the last frame when none has come for a while.
    SCStream *stream = _stream;
    _repeat = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, _queue);
    dispatch_source_set_timer(_repeat, dispatch_time(DISPATCH_TIME_NOW, repeat_after), repeat_after, repeat_after / 10);
    __weak SCVideo *weakSelf = self;
    dispatch_source_set_event_handler(_repeat, ^{
      [weakSelf repeatLastFrame:stream];
    });
    dispatch_resume(_repeat);
    return signal;
  }
}

- (void)stream:(SCStream *)stream didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer ofType:(SCStreamOutputType)type {
  if (type != SCStreamOutputTypeScreen || !CMSampleBufferIsValid(sampleBuffer)) {
    return;
  }
  // Only complete frames carry pixels; idle frames just say nothing changed.
  CFArrayRef attachments = CMSampleBufferGetSampleAttachmentsArray(sampleBuffer, false);
  if (attachments && CFArrayGetCount(attachments) > 0) {
    NSDictionary *info = (__bridge NSDictionary *) CFArrayGetValueAtIndex(attachments, 0);
    NSNumber *status = info[SCStreamFrameInfoStatus];
    if (status && status.integerValue != SCFrameStatusComplete) {
      return;
    }
  }
  if (!CMSampleBufferGetImageBuffer(sampleBuffer)) {
    return;
  }
  @synchronized(self) {
    if (stream != _stream) {
      return;
    }
    CFRetain(sampleBuffer);
    if (_last) {
      CFRelease(_last);
    }
    _last = sampleBuffer;
  }
  [self deliver:sampleBuffer stream:stream];
}

// Runs on _queue, like the sample handler
- (void)repeatLastFrame:(SCStream *)stream {
  CMSampleBufferRef last = nil;
  @synchronized(self) {
    if (stream != _stream || !_last || dispatch_time(DISPATCH_TIME_NOW, 0) - _lastDelivered < (uint64_t) repeat_after) {
      return;
    }
    last = (CMSampleBufferRef) CFRetain(_last);
  }
  [self deliver:last stream:stream];
  CFRelease(last);
}

- (void)deliver:(CMSampleBufferRef)sampleBuffer stream:(SCStream *)stream {
  FrameCallbackBlock callback;
  @synchronized(self) {
    if (stream != _stream) {
      return;
    }
    callback = _callback;
    _lastDelivered = dispatch_time(DISPATCH_TIME_NOW, 0);
  }
  if (callback && !callback(sampleBuffer)) {
    @synchronized(self) {
      if (stream == _stream) {
        [self stopLocked];
      }
    }
  }
}

- (void)stream:(SCStream *)stream didStopWithError:(NSError *)error {
  NSLog(@"SCVideo: stream stopped: %@", error);
  @synchronized(self) {
    if (stream == _stream) {
      [self stopLocked];
    }
  }
}

- (void)stopLocked {
  if (_repeat) {
    dispatch_source_cancel(_repeat);
    _repeat = nil;
  }
  if (_last) {
    CFRelease(_last);
    _last = nil;
  }
  if (_stream) {
    SCStream *stream = _stream;
    _stream = nil;
    [stream stopCaptureWithCompletionHandler:^(NSError *e) {
    }];
  }
  _callback = nil;
  if (_signal) {
    dispatch_semaphore_signal(_signal);
    _signal = nil;
  }
}

- (void)stop {
  @synchronized(self) {
    [self stopLocked];
  }
}

- (void)dealloc {
  [self stop];
}

@end
