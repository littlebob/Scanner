/*
  This file is part of the Structure SDK.
  Copyright © 2015 Occipital, Inc. All rights reserved.
  http://structure.io
*/

#import "ViewController.h"
#import "ViewController+Camera.h"
#import "ViewController+Sensor.h"
#import "ViewController+SLAM.h"
#import "ViewController+OpenGL.h"

#import <Structure/Structure.h>
#import <Structure/StructureSLAM.h>

@implementation ViewController (Sensor)

#pragma mark -  Structure Sensor delegates

- (void)setupStructureSensor
{
    // Get the sensor controller singleton.
    _sensorController = [STSensorController sharedController];
    
    // Set ourself as the delegate to receive sensor data.
    _sensorController.delegate = self;
    
    // Place holder for future processed depth frames.
    _lastFloatDepth = [[STFloatDepthFrame alloc] init];
}

- (BOOL)isStructureConnectedAndCharged
{
    return [_sensorController isConnected] && ![_sensorController isLowPower];
}

- (void)sensorDidConnect
{
    NSLog(@"[Structure] Sensor connected!");
    
    if ([self currentStateNeedsSensor])
        [self connectToStructureSensorAndStartStreaming];
}

- (void)sensorDidLeaveLowPowerMode
{
    _appStatus.sensorStatus = AppStatus::SensorStatusNeedsUserToConnect;
    [self updateAppStatusMessage];
}

- (void)sensorBatteryNeedsCharging
{
    // Notify the user that the sensor needs to be charged.
    _appStatus.sensorStatus = AppStatus::SensorStatusNeedsUserToCharge;
    [self updateAppStatusMessage];
}

- (void)sensorDidStopStreaming:(STSensorControllerDidStopStreamingReason)reason
{
    if (reason == STSensorControllerDidStopStreamingReasonAppWillResignActive)
    {
        [self stopColorCamera];
        NSLog(@"[Structure] Stopped streaming because the app will resign its active state.");
    }
    else
    {
        NSLog(@"[Structure] Stopped streaming for an unknown reason.");
    }
}

- (void)sensorDidDisconnect
{
    // If we receive the message while in background, do nothing. We'll check the status when we
    // become active again.
    if ([[UIApplication sharedApplication] applicationState] != UIApplicationStateActive)
        return;
    
    NSLog(@"[Structure] Sensor disconnected!");
    
    // Reset the scan on disconnect, since we won't be able to recover afterwards.
    if (_slamState.scannerState == ScannerStateScanning)
    {
        [self resetButtonPressed:self];
    }
    
    if (_useColorCamera)
        [self stopColorCamera];
    
    // We only show the app status when we need sensor
    if ([self currentStateNeedsSensor])
    {
        _appStatus.sensorStatus = AppStatus::SensorStatusNeedsUserToConnect;
        [self updateAppStatusMessage];
    }
    
    if (_calibrationOverlay)
        _calibrationOverlay.hidden = true;
    
    [self updateIdleTimer];
}


- (STSensorControllerInitStatus)connectToStructureSensorAndStartStreaming
{
    
    // Try connecting to a Structure Sensor.
    STSensorControllerInitStatus result = [_sensorController initializeSensorConnection];
    
    if (result == STSensorControllerInitStatusSuccess || result == STSensorControllerInitStatusAlreadyInitialized)
    {
        // Even though _useColorCamera was set in viewDidLoad by asking if an approximate calibration is guaranteed,
        // it's still possible that the Structure Sensor that has just been plugged in has a custom or approximate calibration
        // that we couldn't have known about in advance.
        
        STCalibrationType calibrationType = [_sensorController calibrationType];
        if (calibrationType == STCalibrationTypeApproximate || calibrationType == STCalibrationTypeDeviceSpecific)
        {
            _useColorCamera = true;
        }
        else
        {
            _useColorCamera = false;
        }
        
        if (_useColorCamera)
        {
            // the new Tracker use both depth and color frames. We will enable the new tracker option here.
            self.enableNewTrackerSwitch.on = true;
            self.enableNewTrackerSwitch.enabled = true;
            self.enableNewTrackerView.hidden = false;
        }
        else
        {
            // the new Tracker use both depth and color frames. We will disable the new tracker option when there is no color camera input.
            self.enableNewTrackerSwitch.on = false;
            self.enableNewTrackerSwitch.enabled = false;
            self.enableNewTrackerView.hidden = true;
        }
        
        _appStatus.sensorStatus = AppStatus::SensorStatusOk;
        [self updateAppStatusMessage];
        
        // Start streaming depth data.
        [self startStructureSensorStreaming];
    }
    else
    {
        switch (result)
        {
            case STSensorControllerInitStatusSensorNotFound:
                NSLog(@"[Structure] No sensor found"); break;
            case STSensorControllerInitStatusOpenFailed:
                NSLog(@"[Structure] Error: Open failed."); break;
            case STSensorControllerInitStatusSensorIsWakingUp:
                NSLog(@"[Structure] Error: Sensor still waking up."); break;
            default: {}
        }
        
        _appStatus.sensorStatus = AppStatus::SensorStatusNeedsUserToConnect;
        [self updateAppStatusMessage];
    }
    
    [self updateIdleTimer];
    
    return result;
}

- (void)startStructureSensorStreaming
{
    if (![self isStructureConnectedAndCharged])
        return;
    
    // Tell the driver to start streaming.
    NSError *error = nil;
    BOOL optionsAreValid = FALSE;
    if (_useColorCamera)
    {
        // We can use either registered or unregistered depth.
        _structureStreamConfig = _options.useRegisteredDepth ? STStreamConfigRegisteredDepth320x240 : STStreamConfigDepth320x240;
        
        // We are using the color camera, so let's make sure the depth gets synchronized with it.
        optionsAreValid = [_sensorController startStreamingWithOptions:@{kSTStreamConfigKey : @(_structureStreamConfig),
                                                                         kSTFrameSyncConfigKey : @(STFrameSyncDepthAndRgb)} error:&error];
        
        [self startColorCamera];
    }
    else
    {
        _structureStreamConfig = STStreamConfigDepth320x240;
        
        optionsAreValid = [_sensorController startStreamingWithOptions:@{kSTStreamConfigKey : @(_structureStreamConfig),
                                                                         kSTFrameSyncConfigKey : @(STFrameSyncOff)} error:&error];
    }
    
    if (!optionsAreValid)
    {
        NSLog(@"Error during streaming start: %s", [[error localizedDescription] UTF8String]);
        return;
    }
    
    NSLog(@"[Structure] Streaming started.");
    
    // Notify and initialize streaming dependent objects.
    [self onStructureSensorStartedStreaming];
}

- (void)onStructureSensorStartedStreaming
{
    STCalibrationType calibrationType = [_sensorController calibrationType];
    
    // The Calibrator app will be updated to support future iPads, and additional attachment brackets will be released as well.
    const bool deviceIsLikelySupportedByCalibratorApp = (UI_USER_INTERFACE_IDIOM() == UIUserInterfaceIdiomPad);
    
    // Only present the option to switch over to the Calibrator app if the sensor doesn't already have a device specific
    // calibration and the app knows how to calibrate this iOS device.
    if (calibrationType != STCalibrationTypeDeviceSpecific && deviceIsLikelySupportedByCalibratorApp)
    {
        if (!_calibrationOverlay)
            _calibrationOverlay = [CalibrationOverlay calibrationOverlaySubviewOf:self.view atOrigin:CGPointMake(8, 8)];
        else
            _calibrationOverlay.hidden = false;
    }
    else
    {
        if (_calibrationOverlay)
            _calibrationOverlay.hidden = true;
    }
    
    // We cannot initialize SLAM objects before a sensor is ready to be used, so is a good time
    // to do it.
    
    // Retrieve some sensor info for our current streaming configuration.
    STStreamInfo *streamInfo = [_sensorController getStreamInfo:_structureStreamConfig];
    
    // if the sensor info changes, we should reinitialize SLAM
    if(![streamInfo isEqualToStreamInfo:_slamState.streamInfo])
        [self clearSLAM];
    
    if (!_slamState.initialized)
        [self setupSLAM:streamInfo];
}


- (void)sensorDidOutputSynchronizedDepthFrame:(STDepthFrame *)depthFrame
                               andColorBuffer:(CMSampleBufferRef)sampleBuffer
{
    if (_slamState.initialized)
    {
        [self processDepthFrame:depthFrame colorFrame:sampleBuffer];
        // Scene rendering is triggered by new frames to avoid rendering the same view several times.
        [self renderScene];
    }
}

- (void)sensorDidOutputDepthFrame:(STDepthFrame *)depthFrame
{
    if (_slamState.initialized)
    {
        [self processDepthFrame:depthFrame colorFrame:nil];
        // Scene rendering is triggered by new frames to avoid rendering the same view several times.
        [self renderScene];
    }
}

@end
