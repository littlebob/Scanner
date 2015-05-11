/*
  This file is part of the Structure SDK.
  Copyright © 2015 Occipital, Inc. All rights reserved.
  http://structure.io
*/

#import "ViewController.h"
#import "ViewController+Camera.h"
#import "ViewController+Sensor.h"
#import "ViewController+SLAM.h"

#import <Structure/Structure.h>
#import <Structure/StructureSLAM.h>

#import <objc/runtime.h>

@implementation ViewController (Camera)

#pragma mark -  Color Camera

-(void)avCaptureSessionDidStartRunning:(NSNotification*)notification
{
    // We lock the focus as soon as possible we can to make it as close as possible to infinity.
    // This is necessary on iOS7 since we cannot manually lock the focus at infinity, but on iOS8
    // we can use setFocusModeLockedWithLensPosition.
    if (notification.name == AVCaptureSessionDidStartRunningNotification)
    {
        NSError * error;
        if([self.videoDevice lockForConfiguration:&error]) {
            
            if ([self.videoDevice isFocusModeSupported:AVCaptureFocusModeLocked])
                self.videoDevice.focusMode = (AVCaptureFocusMode)AVCaptureFocusModeLocked;
            
            [self.videoDevice unlockForConfiguration];
        }
    }
}

- (BOOL)queryCameraAuthorizationStatusAndNotifyUserIfNotGranted
{
    // This API was introduced in iOS 7, but in iOS 8 it's actually enforced.
    if ([AVCaptureDevice respondsToSelector:@selector(authorizationStatusForMediaType:)])
    {
        AVAuthorizationStatus authStatus = [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeVideo];
        
        if (authStatus != AVAuthorizationStatusAuthorized)
        {
            NSLog(@"Not authorized to use the camera!");
            
            [AVCaptureDevice requestAccessForMediaType:AVMediaTypeVideo
                                     completionHandler:^(BOOL granted)
             {
                 // This block fires on a separate thread, so we need to ensure any actions here
                 // are sent to the right place.
                 
                 // If the request is granted, let's try again to start an AVFoundation session.
                 // Otherwise, alert the user that things won't go well.
                 if (granted)
                 {
                     dispatch_async(dispatch_get_main_queue(), ^(void) {
                         
                         [self startColorCamera];
                         
                         _appStatus.colorCameraIsAuthorized = true;
                         [self updateAppStatusMessage];
                         
                     });
                 }
             }];
            
            return false;
        }
        
    }
    
    return true;
    
}

- (void)setupColorCamera
{
    // If already setup, skip it
    if (self.avCaptureSession)
        return;
    
    bool cameraAccessAuthorized = [self queryCameraAuthorizationStatusAndNotifyUserIfNotGranted];
    
    if (!cameraAccessAuthorized)
    {
        _appStatus.colorCameraIsAuthorized = false;
        [self updateAppStatusMessage];
        return;
    }
    
    // Use VGA color.
    NSString *sessionPreset = AVCaptureSessionPreset640x480;
    
    // Set up Capture Session.
    self.avCaptureSession = [[AVCaptureSession alloc] init];
    [self.avCaptureSession beginConfiguration];
    
    // Set preset session size.
    [self.avCaptureSession setSessionPreset:sessionPreset];
    
    // Create a video device and input from that Device.  Add the input to the capture session.
    self.videoDevice = [AVCaptureDevice defaultDeviceWithMediaType:AVMediaTypeVideo];
    if (self.videoDevice == nil)
        assert(0);
    
    // Configure Focus, Exposure, and White Balance
    NSError *error;
    
    // iOS8 supports manual focus at near-infinity, but iOS7 doesn't.
#if __IPHONE_OS_VERSION_MAX_ALLOWED >= 80000
    bool avCaptureSupportsManualFocus = [self.videoDevice respondsToSelector:@selector(setFocusModeLockedWithLensPosition:completionHandler:)];
#else
    bool avCaptureSupportsFocusNearInfinity = false;
#endif
    
    if([self.videoDevice lockForConfiguration:&error])
    {
        // Allow exposure to initially change
        if ([self.videoDevice isExposureModeSupported:AVCaptureExposureModeContinuousAutoExposure])
            [self.videoDevice setExposureMode:AVCaptureExposureModeContinuousAutoExposure];
        
        // Allow white balance to initially change
        if ([self.videoDevice isWhiteBalanceModeSupported:AVCaptureWhiteBalanceModeContinuousAutoWhiteBalance])
            [self.videoDevice setWhiteBalanceMode:AVCaptureWhiteBalanceModeContinuousAutoWhiteBalance];
        
        if (avCaptureSupportsManualFocus)
        {
            // Set focus at the maximum position allowable (e.g. "near-infinity")
            
#if __IPHONE_OS_VERSION_MAX_ALLOWED >= 80000
            [self.videoDevice setFocusModeLockedWithLensPosition:1.0f completionHandler:nil];
#endif
        }
        else
        {
            
            if ([self.videoDevice isAutoFocusRangeRestrictionSupported])
                [self.videoDevice setAutoFocusRangeRestriction:AVCaptureAutoFocusRangeRestrictionFar];
            
            if ([self.videoDevice isFocusModeSupported:AVCaptureFocusModeContinuousAutoFocus])
                [self.videoDevice setFocusMode:AVCaptureFocusModeContinuousAutoFocus];
            
            // If setFocusModeLockedWithLensPosition is not supported, we will try to obtain a similar
            // behavior by monitoring the start running notification and locking the focus really quickly.
            // It seems to consistently give a focus near/at infinity on iOS7.
            [[NSNotificationCenter defaultCenter] addObserver:self
                                                     selector:@selector(avCaptureSessionDidStartRunning:)
                                                         name:AVCaptureSessionDidStartRunningNotification object:self.avCaptureSession];
        }
        
        [self.videoDevice unlockForConfiguration];
    }
    
    //  Add the device to the session.
    AVCaptureDeviceInput *input = [AVCaptureDeviceInput deviceInputWithDevice:self.videoDevice error:&error];
    if (error)
    {
        NSLog(@"Cannot initialize AVCaptureDeviceInput");
        assert(0);
    }
    
    [self.avCaptureSession addInput:input]; // After this point, captureSession captureOptions are filled.
    
    //  Create the output for the capture session.
    AVCaptureVideoDataOutput* dataOutput = [[AVCaptureVideoDataOutput alloc] init];
    
    // We don't want to process late frames.
    [dataOutput setAlwaysDiscardsLateVideoFrames:YES];
    
    // Use YCbCr pixel format.
    [dataOutput setVideoSettings:[NSDictionary
                                  dictionaryWithObject:[NSNumber numberWithInt:kCVPixelFormatType_420YpCbCr8BiPlanarFullRange]
                                  forKey:(id)kCVPixelBufferPixelFormatTypeKey]];
    
    // Set dispatch to be on the main thread so OpenGL can do things with the data
    [dataOutput setSampleBufferDelegate:self queue:dispatch_get_main_queue()];
    
    [self.avCaptureSession addOutput:dataOutput];
    
    // Force the framerate to 30 FPS, to be in sync with Structure Sensor.
    if ([self.videoDevice respondsToSelector:@selector(setActiveVideoMaxFrameDuration:)]
        && [self.videoDevice respondsToSelector:@selector(setActiveVideoMinFrameDuration:)])
    {
        // Available since iOS 7.
        if([self.videoDevice lockForConfiguration:&error])
        {
            [self.videoDevice setActiveVideoMaxFrameDuration:CMTimeMake(1, 30)];
            [self.videoDevice setActiveVideoMinFrameDuration:CMTimeMake(1, 30)];
            [self.videoDevice unlockForConfiguration];
        }
    }
    else
    {
        NSLog(@"iOS 7 or higher is required. Camera not properly configured.");
    }
    
    [self.avCaptureSession commitConfiguration];
}

- (void)startColorCamera
{
    if (self.avCaptureSession && [self.avCaptureSession isRunning])
        return;
    
    // Re-setup so focus is lock even when back from background
    if (self.avCaptureSession == nil)
        [self setupColorCamera];
    
    // Start streaming color images.
    [self.avCaptureSession startRunning];
}

- (void)stopColorCamera
{
    if ([self.avCaptureSession isRunning])
    {
        // Stop the session
        [self.avCaptureSession stopRunning];
    }
    
    [[NSNotificationCenter defaultCenter] removeObserver:self
                                                    name:AVCaptureSessionDidStartRunningNotification
                                                  object:self.avCaptureSession];
    
    self.avCaptureSession = nil;
    self.videoDevice = nil;
}

- (void)setColorCameraParametersForInit
{
    NSError *error;
    
    [self.videoDevice lockForConfiguration:&error];
    
    // Auto-exposure
    if ([self.videoDevice isExposureModeSupported:AVCaptureExposureModeContinuousAutoExposure])
        [self.videoDevice setExposureMode:AVCaptureExposureModeContinuousAutoExposure];
    
    // Auto-white balance.
    if ([self.videoDevice isWhiteBalanceModeSupported:AVCaptureWhiteBalanceModeContinuousAutoWhiteBalance])
        [self.videoDevice setWhiteBalanceMode:AVCaptureWhiteBalanceModeContinuousAutoWhiteBalance];
    
    [self.videoDevice unlockForConfiguration];
    
}

- (void)setColorCameraParametersForScanning
{
    NSError *error;
    
    [self.videoDevice lockForConfiguration:&error];
    
    // Exposure locked to its current value.
    if([self.videoDevice isExposureModeSupported:AVCaptureExposureModeLocked])
        [self.videoDevice setExposureMode:AVCaptureExposureModeLocked];
    
    // White balance locked to its current value.
    if([self.videoDevice isWhiteBalanceModeSupported:AVCaptureWhiteBalanceModeLocked])
        [self.videoDevice setWhiteBalanceMode:AVCaptureWhiteBalanceModeLocked];
    
    [self.videoDevice unlockForConfiguration];
}

- (void)captureOutput:(AVCaptureOutput *)captureOutput didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer fromConnection:(AVCaptureConnection *)connection
{
    // Pass color buffers directly to the driver, which will then produce synchronized depth/color pairs.
    [_sensorController frameSyncNewColorBuffer:sampleBuffer];
}

@end