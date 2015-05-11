/*
  This file is part of the Structure SDK.
  Copyright © 2015 Occipital, Inc. All rights reserved.
  http://structure.io
*/

#import "ViewController.h"
#import "ViewController+OpenGL.h"

#import <Structure/Structure.h>
#import <Structure/StructureSLAM.h>

@implementation ViewController (SLAM)

#pragma mark - SLAM

// Set up SLAM related objects.
- (void)setupSLAM:(STStreamInfo *)streamInfo
{
    if (_slamState.initialized)
        return;
    
    // Retrieve some sensor info for our current streaming configuration.
    _slamState.streamInfo = streamInfo;
    
    if (!streamInfo)
    {
        NSLog(@"Error: Could not retrieve sensor information.");
        return;
    }
    
    // Initialize the scene.
    _slamState.scene = [[STScene alloc] initWithContext:_display.context
                                             streamInfo:_slamState.streamInfo
                                      freeGLTextureUnit:GL_TEXTURE2];
    
    // Initialize the camera pose tracker.
    NSDictionary* trackerOptions = @{
                                     kSTTrackerTypeKey: self.enableNewTrackerSwitch.on ? @(STTrackerDepthAndColorBased) : @(STTrackerDepthBased),
                                     kSTTrackerTrackAgainstModelKey: @TRUE, // tracking against the model is much better for close range scanning.
                                     kSTTrackerQualityKey: @(STTrackerQualityAccurate),
                                     kSTTrackerBackgroundProcessingEnabledKey: @TRUE
                                     };
    
    NSError* trackerInitError = nil;
    
    // Initialize the camera pose tracker.
    _slamState.tracker = [[STTracker alloc] initWithScene:_slamState.scene options:trackerOptions error:&trackerInitError];
    
    if (trackerInitError != nil)
    {
        NSLog(@"Error during STTracker initialization: `%@'.", [trackerInitError localizedDescription]);
    }
    
    NSAssert (_slamState.tracker != nil, @"Could not create a tracker.");
    
    // Initialize the mapper.
    GLKVector3 volumeResolution = GLKVector3Make(_options.volumeResolution,
                                                 _options.volumeResolution,
                                                 _options.volumeResolution);
    _slamState.mapper = [[STMapper alloc] initWithScene:_slamState.scene
                                       volumeResolution:volumeResolution];
    
    // We need it for the TrackAgainstModel tracker, and for live rendering.
    _slamState.mapper.liveTriangleMeshEnabled = true;
    
    // Default volume size set in options struct
    _slamState.mapper.volumeSizeInMeters = _options.initialVolumeSize;
    
    // Setup the cube placement initializer.
    NSError* cameraPoseInitializerError = nil;
    _slamState.cameraPoseInitializer = [[STCameraPoseInitializer alloc]
                                        initWithStreamInfo:_slamState.streamInfo
                                        volumeSizeInMeters:_slamState.mapper.volumeSizeInMeters
                                        options:@{kSTCameraPoseInitializerStrategyKey: @(STCameraPoseInitializerStrategyTableTopCube)}
                                        error:&cameraPoseInitializerError];
    NSAssert (cameraPoseInitializerError == nil, @"Could not initialize STCameraPoseInitializer: %@", [cameraPoseInitializerError localizedDescription]);
    
    // Set up the cube renderer with the current volume size.
    _display.cubeRenderer = [[STCubeRenderer alloc] initWithScene:_slamState.scene];
    
    // Set up the initial volume size.
    [self adjustVolumeSize:_slamState.mapper.volumeSizeInMeters];
    
    // Start with cube placement mode
    [self enterCubePlacementState];
    
    NSDictionary* keyframeManagerOptions = @{
                                             kSTKeyFrameManagerMaxSizeKey: @(_options.maxNumKeyFrames),
                                             kSTKeyFrameManagerMaxDeltaTranslationKey: @(_options.maxKeyFrameTranslation),
                                             kSTKeyFrameManagerMaxDeltaRotationKey: @(_options.maxKeyFrameRotation), // 20 degrees.
                                             };
    
    NSError* keyFrameManagerInitError = nil;
    _slamState.keyFrameManager = [[STKeyFrameManager alloc] initWithOptions:keyframeManagerOptions error:&keyFrameManagerInitError];
    if (keyFrameManagerInitError != nil)
    {
        NSLog(@"Error happens in init STKeyFrameManger: %@", [keyFrameManagerInitError localizedDescription]);
    }
    
    _depthAsRgbaVisualizer = [[STDepthToRgba alloc] initWithStreamInfo:_slamState.streamInfo
                                                               options:@{kSTDepthToRgbaStrategyKey: @(STDepthToRgbaStrategyGray)}
                                                                 error:nil];
    
    _slamState.initialized = true;
}

- (void)resetSLAM
{
    [_slamState.mapper reset];
    [_slamState.tracker reset];
    [_slamState.scene clear];
    [_slamState.keyFrameManager clear];
    
    [self enterCubePlacementState];
}

- (void)clearSLAM
{
    _slamState.initialized = false;
    _slamState.streamInfo = nil;
    _slamState.scene = nil;
    _slamState.tracker = nil;
    _slamState.mapper = nil;
    _slamState.keyFrameManager = nil;
}

- (void)processDepthFrame:(STDepthFrame *)depthFrame
               colorFrame:(CMSampleBufferRef)sampleBuffer
{
    // Compute a processed depth frame from the raw data.
    // Both shift and float values in meters will then be available.
    [_lastFloatDepth updateFromDepthFrame:depthFrame];
    
    // Upload the new color image for next rendering.
    if (_useColorCamera && sampleBuffer != nil)
        [self uploadGLColorTexture:sampleBuffer];
    else if(!_useColorCamera)
        [self uploadGLColorTextureFromDepth];
    
    switch (_slamState.scannerState)
    {
        case ScannerStateCubePlacement:
        {
            // Provide the new depth frame to the cube renderer for ROI highlighting.
            [_display.cubeRenderer setDepthFrame:_useColorCamera?_lastFloatDepth.registeredToColor:_lastFloatDepth];
            
            // Estimate the new scanning volume position.
            if (GLKVector3Length(_lastGravity) > 1e-5f)
            {
                bool success = [_slamState.cameraPoseInitializer updateCameraPoseWithGravity:_lastGravity depthFrame:_lastFloatDepth error:nil];
                NSAssert (success, @"Camera pose initializer error.");
            }
            
            // Tell the cube renderer whether there is a support plane or not.
            [_display.cubeRenderer setCubeHasSupportPlane:_slamState.cameraPoseInitializer.hasSupportPlane];
            
            // Enable the scan button if the pose initializer could estimate a pose.
            self.scanButton.enabled = _slamState.cameraPoseInitializer.hasValidPose;
            break;
        }
            
        case ScannerStateScanning:
        {
            NSError* trackingError = nil;
            
            // First try to estimate the 3D pose of the new frame.
            BOOL trackingOk = [_slamState.tracker updateCameraPoseWithDepthFrame:_lastFloatDepth colorBuffer:sampleBuffer error:&trackingError];
            
            // Integrate it into the current mesh estimate if tracking was successful.
            if (trackingOk)
            {
                [_slamState.mapper integrateDepthFrame:_lastFloatDepth cameraPose:[_slamState.tracker lastFrameCameraPose]];
                
                NSError* keyFrameError = nil;
                [_slamState.keyFrameManager processKeyFrameCandidateWithCameraPose:[_slamState.tracker lastFrameCameraPose]
                                                                       colorBuffer:sampleBuffer
                                                                        depthFrame:nil];
                
                [self hideTrackingErrorMessage];
            }
            else if(trackingError.code == STErrorTrackerLostTrack)
            {
                [self showTrackingMessage:@"Tracking Lost! Please Realign or Press Reset."];
            }
            else if(trackingError.code == STErrorTrackerPoorQuality)
            {
                switch ([_slamState.tracker status])
                {
                    case STTrackerStatusDodgyForUnknownReason:
                    {
                        NSLog(@"STTracker Tracker quality is bad, but we don't know why.");
                        // Don't show anything on screen since this can happen often.
                        break;
                    }
                        
                    case STTrackerStatusFastMotion:
                    {
                        NSLog(@"STTracker Camera moving too fast.");
                        // Don't show anything on screen since this can happen often.
                        break;
                    }
                        
                    case STTrackerStatusTooClose:
                    {
                        NSLog(@"STTracker Too close to the model.");
                        [self showTrackingMessage:@"Too close to the scene! Please step back."];
                        break;
                    }
                        
                    case STTrackerStatusTooFar:
                    {
                        NSLog(@"STTracker Too far from the model.");
                        [self showTrackingMessage:@"Please get closer to the model."];
                        break;
                    }
                        
                    case STTrackerStatusRecovering:
                    {
                        NSLog(@"STTracker Recovering.");
                        [self showTrackingMessage:@"Recovering, please move gently."];
                        break;
                    }
                        
                    case STTrackerStatusModelLost:
                    {
                        NSLog(@"STTracker model not in view.");
                        [self showTrackingMessage:@"Please put the model back in view."];
                        break;
                    }
                    default:
                        NSLog(@"STTracker unknown quality.");
                }
            }
            else
            {
                NSLog(@"[Structure] STTracker Error: %@.", [trackingError localizedDescription]);
            }
            
            break;
        }
            
        case ScannerStateViewing:
        default:
        {} // Do nothing, the MeshViewController will take care of this.
    }
}

@end