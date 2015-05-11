/*
    This file is part of the Structure SDK.
    Copyright © 2015 Occipital, Inc. All rights reserved.
    http://structure.io
*/

#pragma once

#include <stdint.h>
#include <stdlib.h>
#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>

# if !defined(__cplusplus) && !defined (HAS_LIBCXX)
#   error "Structure framework requires the C++ runtime. See Structure SDK Reference."
# endif

// Make sure the deployment target is higher or equal to iOS 7.
#if defined(__IPHONE_OS_VERSION_MIN_REQUIRED) && (__IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_7_0)
#   error The Structure SDK only supports iOS 7 or higher.
#endif

//------------------------------------------------------------------------------
#pragma mark - Sensor Controller Types

/** Sensor Initialization Status

See also:

- [STSensorController initializeSensorConnection]
*/
typedef NS_ENUM(NSInteger, STSensorControllerInitStatus)
{
    /// Cannot initialize sensor controller because sensor is not found.
    STSensorControllerInitStatusSensorNotFound      = 0,
    
    /// Sensor controller initialization succeeded.
    STSensorControllerInitStatusSuccess             = 1,
    
    /// Sensor controller has already initialized.
    STSensorControllerInitStatusAlreadyInitialized  = 2,
    
    /// Cannot initialize sensor controller because sensor is waking up.
    STSensorControllerInitStatusSensorIsWakingUp    = 3,
    
    /// Cannot initialize sensor controller because of failure in opening sensor connection.
    STSensorControllerInitStatusOpenFailed          = 4,
};

/** Streaming Interruption Reason
 
See also:
 
- [STSensorControllerDelegate sensorDidStopStreaming:]
*/
typedef NS_ENUM(NSInteger, STSensorControllerDidStopStreamingReason)
{
    /// Sensor stops streaming because of iOS app will resign active. This can occur when apps are sent to the background, during fast app switching, or when the notification/control center appears.
    STSensorControllerDidStopStreamingReasonAppWillResignActive = 0
};

/**
Constants indicating the Structure Sensor streaming configuration.

See also:

- [STSensorController startStreamingWithOptions:error:]
*/
typedef NS_ENUM(NSInteger, STStreamConfig)
{
    // 30 FPS modes
    
    /// QVGA depth at 30 FPS.
    STStreamConfigDepth320x240 = 0,

    /// QVGA depth at 30 FPS, aligned to the color camera.
    STStreamConfigRegisteredDepth320x240,

    /// QVGA depth and infrared at 30 FPS.
    STStreamConfigDepth320x240AndInfrared320x248,

    /// QVGA infrared at 30 FPS.
    STStreamConfigInfrared320x248,

    /// VGA depth at 30 FPS.
    STStreamConfigDepth640x480,

    /// VGA infrared at 30 FPS.
    STStreamConfigInfrared640x488,

    /// VGA depth and infrared at 30 FPS.
    STStreamConfigDepth640x480AndInfrared640x488,

    /// VGA depth at 30 FPS, aligned to the color camera.
    STStreamConfigRegisteredDepth640x480,
    
    /// QVGA depth at 60 FPS. Note: frame sync is not supported with this mode.
    STStreamConfigDepth320x240_60FPS
};

/** Frame Sync Configuration.

Constants indicating whether the driver should attempt to synchronize depth or infrared frames with color frames from AVFoundation.

When frame sync is active (i.e. **not** equal to `STFrameSyncOff`), one of the following methods is used in lieu of [STSensorControllerDelegate sensorDidOutputDepthFrame:], depending on the selected configuration:

- [STSensorControllerDelegate sensorDidOutputSynchronizedDepthFrame:andColorBuffer:]
- [STSensorControllerDelegate sensorDidOutputSynchronizedInfraredFrame:andColorBuffer:]

You must then repeatedly call frameSyncNewColorBuffer: from the AVFoundation video capture delegate methods. Otherwise, the sensor controller delegate methods will never deliver any frames. This is simply because synchronized frames cannot be delivered if there are no color frames to synchronize.

@note Frame sync of depth+infrared+RGB and 60 FPS depth are not currently supported.
@note For frame sync to be effective, the AVCaptureDevice must be configured to have a min and max frame rate of 30fps.
*/
typedef NS_ENUM(NSInteger, STFrameSyncConfig)
{
    /// Default mode, frame sync is off.
    STFrameSyncOff = 0,
    
    /// Frame sync between AVFoundation video frame and depth frame.
    STFrameSyncDepthAndRgb,
    
    /// Frame sync between AVFoundation video frame and infrared frame.
    STFrameSyncInfraredAndRgb
};

/** Sensor Calibration Type

Calibration types indicate whether a Structure Sensor + iOS device combination has no calibration, approximate calibration, or a device specific calibration from Calibrator.app.
 
See also:

- [STSensorController calibrationType]
 */
typedef NS_ENUM(NSInteger, STCalibrationType)
{
    /// There is no calibration for Structure Sensor + iOS device combination.
    STCalibrationTypeNone = 0,
    
    /// There exists an approximate calibration Structure Sensor + iOS device combination.
    STCalibrationTypeApproximate,
    
    /// There exists a device specific calibration from Calibrator.app of this Structure Sensor + iOS device combination.
    STCalibrationTypeDeviceSpecific,
};

//------------------------------------------------------------------------------

// Dictionary keys for `[STSensorController startStreamingWithOptions:error:]`.
extern NSString* const kSTStreamConfigKey;
extern NSString* const kSTFrameSyncConfigKey;
extern NSString* const kSTHoleFilterConfigKey;
extern NSString* const kSTHighGainConfigKey;

//------------------------------------------------------------------------------
# pragma mark - STFrame types

/** Generic Frame Type
 
 STFrame is the generic type for raw frames streaming from Structure Sensor. Both <STDepthFrame> and <STInfraredFrame> inherit from this.
 
 See also <STFloatDepthFrame>.
 
 */
@interface STFrame : NSObject

/// Contiguous chunk of `width * height` pixels.
@property (readwrite, nonatomic) uint16_t *data;

/// Frame width.
@property (readwrite, nonatomic) int width;

/// Frame height.
@property (readwrite, nonatomic) int height;

/** Capture timestamp in seconds since the iOS device boot (same clock as CoreMotion and AVCapture). */
@property (readwrite, nonatomic) NSTimeInterval timestamp;

@end

/** Depth Frame.
 
STDepthFrame is inherited from the generic type `STFrame`. It is the raw depth frame object for frames streaming from Structure Sensor.

See also <STFrame>.

*/
@interface STDepthFrame : STFrame
@end

/** Infrared Frame.
 
STInfraredFrame is inherited from the generic type `STFrame`. It is the raw infrared frame object for frames streaming from Structure Sensor.

See also <STFrame>.

*/
@interface STInfraredFrame : STFrame
@end


//------------------------------------------------------------------------------
# pragma mark - STStreamInfo

/** Structure Sensor Internal Stream Info.

An STStreamInfo object contains the internal properties of the images being streamed by a sensor.

*/
@interface STStreamInfo : NSObject

/** Returns a Boolean value that indicates whether a given streamInfo is equal to the receiver by comparing the stream and sensor calibration properties.
@param aStreamInfo The STStreamInfo object with which to compare the receiver.
*/
- (BOOL)isEqualToStreamInfo:(STStreamInfo *)aStreamInfo;

/**
Get the rigid body transformation (RBT) between the iOS color camera and the Structure Sensor depth stream viewpoint.

When using an un-registered mode, this transform is the same as [STSensorController colorCameraPoseInSensorCoordinateFrame]. However, when using a registered depth mode, the depth frame is already aligned to the color camera viewpoint and thus this transform is identity.

The RBT represents the world motion of the RGB camera w.r.t. the Structure Sensor stream viewpoint. The coordinate frame is right handed: X right, Y down, Z out. Equivalently, this matrix can transform a 3D point expressed in the iOS color camera coordinate system to the Structure Sensor depth stream coordinate system.

@param matrix4x4 This output parameter should be a pointer to 16 floating point values in _column_ major order. This is the default ordering of GLKMatrix4.
*/
- (void)colorCameraPoseInDepthCoordinateFrame:(float *)matrix4x4;
@end

//------------------------------------------------------------------------------
# pragma mark - Sensor Controller Delegate

/** Sensor Controller Delegate
 
 The interface that your application-specific class must implement in order to receive sensor controller callbacks.
 
 @warning When creating a new application implementing a sensor controller delegate, the main `Info.plist` needs to contain an additional key "`Supported external accessory protocols`", with the following array of values:
 
 - `io.structure.control`
 - `io.structure.depth`
 - `io.structure.infrared`
 
 Without this modification to the plist, the app will not be able to connect to the sensor. All sample apps have this key/value array.
 
 See also <[STSensorController sharedController]> & <[STSensorController delegate]>.
 
 @note Delegate Registration Example
 
 [ STSensorController sharedController ].delegate = self;
 */
@protocol STSensorControllerDelegate <NSObject>

/// @name Connection Status

/// Notifies the delegate that the controller established a successful connection to the sensor.
- (void)sensorDidConnect;

/// Notifies the delegate that the sensor was disconnected from the controller.
- (void)sensorDidDisconnect;

/** Notifies the delegate that the sensor stopped streaming frames to the controller.

@param reason The reason why the stream was stopped. See: STSensorControllerDidStopStreamingReason.
*/
- (void)sensorDidStopStreaming:(STSensorControllerDidStopStreamingReason)reason;

/// @name Power Management

/// Notifies the delegate that the sensor has left low power mode.
- (void)sensorDidLeaveLowPowerMode;

/// Notifies the delegate that the sensor needs charging.
- (void)sensorBatteryNeedsCharging;

@optional

/// @name Colorless Frames

/** Notifies the delegate that the sensor made a new depth frame available to the controller. If the data is needed beyond the scope of this method, the depthFrame and its data must be copied by the receiver.

@param depthFrame The new depth frame.
*/
- (void)sensorDidOutputDepthFrame:(STDepthFrame *)depthFrame;

/** Notifies the delegate that the sensor made a new infrared frame available to the controller. If the data is needed beyond the scope of this method, the irFrame and its data must be copied by the receiver.

@param irFrame The new infrared frame.
*/
- (void)sensorDidOutputInfraredFrame:(STInfraredFrame *)irFrame;

/// @name Color-synchronized Frames

/** Notifies the delegate that the sensor made a new pair of depth and color frames available to the controller.

Frame sync methods will only be used if `kSTFrameSyncConfigKey` has been configured in [STSensorController startStreamingWithOptions:error:] to sync frames .
Also, data will only be delivered if frameSyncNewColorBuffer: is called every time a new sample buffer is available. The driver needs the CMSampleBuffers in order to return them through these methods.
If the CMSampleBuffers are needed beyond the scope of these functions, retain them with `CFRetain(sampleBuffer)`. Make sure to pair the retain with `CFRelease(sampleBuffer)` when the data is no longer needed. If the STFrame types are needed beyond the scope of these functions, the data must be copied by the receiver.
 
See also:

- <[STSensorController startStreamingWithOptions:error:]>
- <[STSensorController frameSyncNewColorBuffer:]>

@param depthFrame The new depth frame.
@param sampleBuffer The new color buffer.
*/
- (void)sensorDidOutputSynchronizedDepthFrame:(STDepthFrame *)depthFrame
                               andColorBuffer:(CMSampleBufferRef)sampleBuffer;

/** Notifies the delegate that the sensor made a new pair of synchronized infrared and color frames available to the controller.

Frame sync methods will only be used if `kSTFrameSyncConfigKey` has been configured in [STSensorController startStreamingWithOptions:error:] to sync frames .
Also, data will only be delivered if frameSyncNewColorBuffer: is called every time a new sample buffer is available. The driver needs the CMSampleBuffers in order to return them through these methods.
If the CMSampleBuffers are needed beyond the scope of these functions, retain them with `CFRetain(sampleBuffer)`. Make sure to pair the retain with `CFRelease(sampleBuffer)` when the data is no longer needed. If the STFrame types are needed beyond the scope of these functions, the data must be copied by the receiver.
 
See also:

- <[STSensorController startStreamingWithOptions:error:]>
- <[STSensorController frameSyncNewColorBuffer:]>

@param irFrame The new infrared frame.
@param sampleBuffer The new color buffer.
*/
- (void)sensorDidOutputSynchronizedInfraredFrame:(STInfraredFrame *)irFrame
                                  andColorBuffer:(CMSampleBufferRef)sampleBuffer;

/// @name Power Management

/// Notifies the delegate that the sensor has entered low power mode, this currently does nothing and is reserved for future usage.
- (void)sensorDidEnterLowPowerMode;

@end

//------------------------------------------------------------------------------
# pragma mark - Sensor Controller

/** The sensor controller is the central point that manages all the interactions between the sensor and your application-specific delegate class.

Its only instance is available through the sharedController method.

Your custom delegate object can be registered using its delegate property.

See also:

- <STSensorControllerDelegate>
*/
@interface STSensorController : NSObject

/// @name Controller Setup

/**
The STSensorController singleton.

Use it to register your application-specific STSensorControllerDelegate delegate.
*/
+ (STSensorController *)sharedController;

/**
The STSensorControllerDelegate delegate.

It will receive all notifications from the sensor, as well as raw stream data.
*/
@property(nonatomic, unsafe_unretained) id<STSensorControllerDelegate> delegate;

/**
Attempt to connect to the Structure Sensor.

@return Connection has succeeded only if the STSensorControllerInitStatus return value is either one of:

- STSensorControllerInitStatusSuccess
- STSensorControllerInitStatusAlreadyInitialized

@note Many methods (including startStreamingWithOptions:error:) will have no effect until this method succeeds at initializing the sensor.
*/
- (STSensorControllerInitStatus)initializeSensorConnection;

/**
This will begin streaming data from the sensor and delivering data using the delegate methods.

Here is some sample code to get registered QVGA depth with depth/color frame sync:

    NSError* error;
    BOOL success = [sensorController startStreamingWithOptions:@{kSTStreamConfigKey: @(STStreamConfigRegisteredDepth320x240),
                                                                 kSTFrameSyncConfigKey: @(STFrameSyncDepthAndRgb)} 
                                                         error:&error];

Here is some sample code to get VGA infrared images with high gain:

    NSError* error;
    BOOL success = [sensorController startStreamingWithOptions:@{kSTStreamConfigKey: @(STStreamConfigInfrared640x488),
                                                                 kSTHighGainConfigKey: @YES}
                                                         error:&error];
 
@param options Specifies the sensor configuration to use. The valid dictionary keys are:

- `kSTStreamConfigKey`: STStreamConfig value to specify the desired streaming configuration. Required.
- `kSTFrameSyncConfigKey`: STFrameSyncConfig value to configure frame sync with color frames from AVFoundation. Default is `STFrameSyncOff`.
- `kSTHoleFilterConfigKey`: boolean value to enable hole filtering. Default is `@YES` if the stream configuration includes depth. The depth filter enables a dilation of depth values that has the effect of filling holes. Note: setting it to `@YES` if the stream configuration does not include depth is invalid.
- `kSTHighGainConfigKey`: boolean value to enable high gain mode. Default is `@NO`. When set to `@YES` the sensor gain will be increased, causing better performance in dark or far away objects at the expense of some bright nearby objects. See also: setHighGainEnabled:.
 
@param error will contain detailed information if the provided options are incorrect.

@return `TRUE` if the streaming options combination is valid, `FALSE` otherwise, filling in `error` with a detailed message.
*/
- (BOOL)startStreamingWithOptions:(NSDictionary *)options error:(NSError * __autoreleasing *)error;

/**
Stop streaming data from the sensor.

After this method is called, pending frames may still be delivered.
*/
- (void)stopStreaming;

/** Give the driver a color frame that will be used to synchronize shutters between the iOS camera and the Structure Sensor camera.

When receiving the CMSampleBufferRef from AVFoundation, you should only call this one method and do no other processing.
When a synchronized frame is found, one of the optional synchronized STSensorController delegate methods will be called, at which point you can then process/render the sampleBuffer.
@param sampleBuffer The new color buffer
*/
- (void)frameSyncNewColorBuffer:(CMSampleBufferRef)sampleBuffer;

/// @name Sensor Status

/// Checks whether the controlled sensor is connected.
- (BOOL)isConnected;

/// Checks whether the controlled sensor is in low-power mode.
- (BOOL)isLowPower;

/// Returns an integer in 0..100 representing the battery charge.
- (int)getBatteryChargePercentage;

/// @name Sensor Information

/// Returns the name of the controlled sensor, or nil if no sensor is connected.
- (NSString *)getName;

/// Returns the serial number of the controlled sensor, or nil if no sensor is connected.
- (NSString *)getSerialNumber;

/// Returns the firmware revision of the controlled sensor, or nil if no sensor is connected.
- (NSString *)getFirmwareRevision;

/// Returns the hardware revision of the controlled sensor, or nil if no sensor is connected.
- (NSString *)getHardwareRevision;

/** Launches Calibrator.app or prompts the user to install it.
 
An option should be presented to the user when the sensor doesn't have a calibrationType with value 
 `CalibrationTypeDeviceSpecific`, the iOS device is supported by the Calibrator app, and registered depth is needed.
*/
+ (BOOL)launchCalibratorAppOrGoToAppStore;

/** Returns a boolean indicating whether an (at least) approximate depth-color calibration will be available when the sensor is connected to the current device.

This can be used to decide whether it is relevant to show color-specific UI elements before a sensor is connected.
 */
+ (BOOL)approximateCalibrationGuaranteedForDevice;

/** Returns the type of the current depth-color calibration as a STCalibrationType enumeration value.

This value can change depending on the actual device-sensor combination in use.
*/
- (STCalibrationType)calibrationType;

/** Returns the sensor stream information associated to a particular configuration.

See also:

- [STScene initWithContext:streamInfo:freeGLTextureUnit:]
- [STDepthToRgba initWithStreamInfo:options:error:]
- [STCameraPoseInitializer initWithStreamInfo:volumeSizeInMeters:options:error:]
@param config The streaming config.
*/
- (STStreamInfo *)getStreamInfo:(STStreamConfig)config;

/// @name Advanced Setup

/** Enable or disable high sensor gain after the stream was started.

This method can dynamically overwrite the `kSTHighGainConfigKey` option specified when startStreamingWithOptions:error: was called.
 
@param enabled When set to `YES`, the sensor gain will be increased, causing better performance in dark or far away objects at the expense of some bright nearby objects.
*/
- (void)setHighGainEnabled:(BOOL)enabled;

/**
Specify a new rigid body transformation (RBT) between the iOS camera and the Structure Sensor camera. This transformation will be used by the SDK to associate depth pixels to color pixels. When using a registered streaming mode, depth images will be pre-warped to match the iOS camera viewpoint using this transform too.

You should only use this method if you are using a custom bracket on an unsupported device. Otherwise we strongly recommend using the Calibrator program.

A stream stop and restart will be required for this to take effect on registered depth images. The new transform will be lost after the sensor did disconnect, so it needs to be called again if the sensor got unplugged or after switching applications.

The RBT represents the world motion of the RGB camera w.r.t. the Structure Sensor. The coordinate frame is right handed: X right, Y down, Z out. Equivalently, it can transform a 3D point expressed in the iOS color camera coordinate system to the Structure Sensor camera coordinate system, which is aligned with the infrared camera.

@param newMatrix4x4 This parameter is expected as a pointer to 16 floating point values in _column_ major order. This is the default ordering of GLKMatrix4.

@note The following is an example call of this method using the Eigen C++ library (not required).
Eigen is column major by default, so the `data()` function of an `Isometry3f` can be used directly.

    - (void)updateRegistration
    {
        [[STSensorController sharedController] stopStreaming];
        
        Eigen::Isometry3f sampleIsometry = Eigen::Isometry3f::Identity();
        Eigen::Vector3f translation = Eigen::Vector3f(0.034, 0, 0.017);

        sampleIsometry.translate(translation);
        sampleIsometry.rotate((Eigen::Matrix3f() << 0.99977, -0.0210634, -0.00412405,
                                                   0.0210795, 0.99977, 0.00391278,
                                                   0.00404069, -0.00399881, 0.999984).finished());

        [[STSensorController sharedController] setColorCameraPoseInSensorCoordinateFrame:sampleIsometry.data()];

        [[STSensorController sharedController] startStreamingWithOptions:@{kSTStreamConfigKey: @(STStreamConfigRegisteredDepth320x240)} 
                                                                   error:nil];
    }
*/
- (void)setColorCameraPoseInSensorCoordinateFrame:(float *)newMatrix4x4;

/**
Retrieve the current 4x4 transformation in _column_ major order between the iOS color camera and the Structure Sensor camera. See [STSensorController setColorCameraPoseInSensorCoordinateFrame].

@param matrix4x4 This output parameter should be a pointer to 16 floating point values in _column_ major order. This is the default ordering of GLKMatrix4.
*/
- (void)colorCameraPoseInSensorCoordinateFrame:(float *)matrix4x4;

@end

//------------------------------------------------------------------------------
# pragma mark - STFloatDepthFrame

/**
Processed depth image where the pixels contain float values in millimeters.

Raw STDepthFrame objects output by Structure Sensor have 16 bit integers pixels holding internal shift values. STFloatDepthFrame transforms this data into metric float values.
*/
@interface STFloatDepthFrame : NSObject

/// Image width.
@property (readonly, nonatomic) int width;

/// Image height.
@property (readonly, nonatomic) int height;

/** Capture timestamp in seconds since the iOS device boot (same clock as CoreMotion and AVCapture). */
@property (readonly, nonatomic) NSTimeInterval timestamp;

/**
Pointer to the beginning of a contiguous chunk of `width * height` depth pixel values, in millimeters.
@note pixels without depth will have NaN values.
*/
@property (readonly, nonatomic) const float *depthAsMillimeters;

/**
Pointer to a STFloatDepthFrame object that is registered to the color camera viewpoint.
@note Readonly. If STStreamConfig is not setup as registered mode, this methods will compute a host side registered depth.
*/
@property (readonly, nonatomic) STFloatDepthFrame* registeredToColor;

/// Recompute metric values from raw Structure depth frame.
/// @param depthFrame The depth frame.
- (void)updateFromDepthFrame:(STDepthFrame *)depthFrame;

/// Create a copy of the current STFloatDepthFrame
/// @param floatDepthFrame The STFloatDepthFrame from which to copy.
- (instancetype)initWithFloatDepthFrame:(STFloatDepthFrame *)floatDepthFrame;

@end
//------------------------------------------------------------------------------
# pragma mark - STGLTextureShaderRGBA

/// Helper class to render a simple OpenGL ES quad with 2D texture in RGBA format.
@interface STGLTextureShaderRGBA : NSObject

/// Enable the underlying shader program.
- (void)useShaderProgram;

/// Render the texture on a fullscreen quad using the given GL_TEXTUREX unit.
/// @param textureUnit A given native OpenGL texture unit to render.
- (void)renderTexture:(GLint)textureUnit;

@end

//------------------------------------------------------------------------------
# pragma mark - STGLTextureShaderYCbCr

/// Helper class to render a simple OpenGL ES quad with a 2D texture in YCbCr format.
@interface STGLTextureShaderYCbCr : NSObject

/// Enable the underlying shader program.
- (void)useShaderProgram;

/// Render the texture on a fullscreen quad using the given GL_TEXTUREX unit.
/// @param lumaTextureUnit A given native OpenGL luminance texture unit to render.
/// @param chromaTextureUnit A given native OpenGL chorma texture unit to render.
- (void)renderWithLumaTexture:(GLint)lumaTextureUnit chromaTexture:(GLint)chromaTextureUnit;

@end

//------------------------------------------------------------------------------
# pragma mark - STGLTextureShaderGray

/// Helper class to render a simple OpenGL ES quad with a 2D single channel texture.
@interface STGLTextureShaderGray : NSObject

/// Enable the underlying shader program.
- (void)useShaderProgram;

/// Render the texture on a fullscreen quad using the given GL_TEXTUREX unit.
/// @param lumaTextureUnit A given native OpenGL texture unit to render.
- (void)renderWithLumaTexture:(GLint)lumaTextureUnit;

@end

//------------------------------------------------------------------------------
# pragma mark - STDepthToRgba

/// Required dictionary key to specify the converter strategy.
extern NSString* const kSTDepthToRgbaStrategyKey;

/**
Constants indicating the depth to color conversion strategy of STDepthToRgba.

See also:
 
- [STDepthToRgba initWithStreamInfo:options:error:]
*/
typedef NS_ENUM(NSInteger, STDepthToRgbaStrategy)
{
    /// Linear mapping using a color gradient – pure red encodes the minimal depth, and pure blue encodes the furthest possible depth.
    STDepthToRgbaStrategyRedToBlueGradient = 0,
    
    /// Linear mapping from closest to furthest depth as a grayscale intensity.
    STDepthToRgbaStrategyGray,
};

/// Helper class to convert float depth data to RGB values for better visualization.
@interface STDepthToRgba : NSObject

/// Pointer to the RGBA values.
@property (nonatomic, readonly) uint8_t *rgbaBuffer;

/// Width of the output RGBA image.
@property (nonatomic, readonly) int width;

/// Height of the output RGBA image.
@property (nonatomic, readonly) int height;

/** Initialize with required STStreamInfo data.
 @param streamInfo The Structure Sensor stream information.
 @param options The option dictionary. The valid keys are:
 
 - `kSTDepthToRgbaStrategyKey`: STDepthToRgbaStrategy value to specify the conversion strategy to use. Required.
 
 @param error will contain detailed information if the provided options are incorrect.
 */
- (instancetype)initWithStreamInfo:(STStreamInfo *)streamInfo options:(NSDictionary *)options error:(NSError* __autoreleasing *)error;

/// Convert the given depth frame to RGBA. The returned pointer is the same as the one accessed through `rgbaBuffer`.
/// @param frame The depth frame.
- (uint8_t *)convertDepthFrameToRgba:(STFloatDepthFrame *)frame;

@end

//------------------------------------------------------------------------------
# pragma mark - STWirelessLog

/**
Wireless logging utility.

This class gives the ability to wirelessly send log messages to a remote console.

It is very useful when the sensor is occupying the lightning port.
*/
@interface STWirelessLog : NSObject

/** 
Redirects the standard and error outputs to a TCP connection.

Messages sent to the `stdout` and `stderr` (such as `NSLog`, `std::cout`, `std::cerr`, `printf`) will be sent to the given IPv4 address on the specified port.

In order to receive these messages on a remote machine, you can, for instance, use the *netcat* command-line utility (available by default on Mac OS X). Simply run in a terminal: `nc -lk <port>`

@note If the connection fails, the returned error will be non-`nil` and no output will be transmitted.
 
@note Only one connection can be active.
@param ipv4Address The IPv4 address of the remote console.
@param port The port of the remote console.
@param error will contain detailed information if the provided options are incorrect.
*/
+ (void)broadcastLogsToWirelessConsoleAtAddress:(NSString *)ipv4Address usingPort:(int)port error:(NSError **)error ;

@end

//------------------------------------------------------------------------------
# pragma mark - STBackgroundTaskDelegate

/**
The interface that your class can implement to receive STBackgroundTask callbacks.

See also:

- <[STBackgroundTask delegate]>
*/

@class STBackgroundTask;

@protocol STBackgroundTaskDelegate <NSObject>
@optional

/** Reports progress in the background task.
@param sender The STBackgroundTask that reports the progress.
@param progress is a floating-point value from 0.0 (Not Started) to 1.0 (Complete).
*/
- (void)backgroundTask:(STBackgroundTask*)sender didUpdateProgress:(double)progress;

@end

/**
Handle to control a task running asynchronously in a background queue.

See also:

- <[STMesh decimateTaskWithMesh:numFaces:completionHandler:]>
- <[STMesh fillHolesTaskWithMesh:completionHandler:]>
- <[STColorizer colorizeTaskWithMesh:scene:keyframes:completionHandler:options:error:]>
- STBackgroundTaskDelegate
*/

//------------------------------------------------------------------------------
# pragma mark - STBackgroundTask

@interface STBackgroundTask : NSObject

/// Start the execution of the task asynchronously, in a background queue.
- (void)start;

/**
Cancels the background task if possible.
 
@note If the operation is already near completion, the completion handler may still be called successfully.
*/
- (void)cancel;

/// Synchronously wait until the task finishes its execution.
- (void)waitUntilCompletion;

/// Whether the task was canceled. You can check this in the completion handler to make sure the task was not canceled right after it finished.
@property(atomic, readonly) BOOL isCancelled;

/// By setting a STBackgroundTaskDelegate delegate to an STBackgroundTask, you can receive progress updates.
@property(atomic, unsafe_unretained) id<STBackgroundTaskDelegate> delegate;

@end

//------------------------------------------------------------------------------
# pragma mark - STErrorCode

/// Constant to identify the Structure Framework error domain.
extern NSString* const StructureSDKErrorDomain;

/** Error codes that specify an error. These may appear in NSError objects returned by various Structure Framework methods.

`NSString* const StructureSDKErrorDomain;`

is the constant to identify the Structure Framework error domain.
*/
typedef NS_ENUM(NSInteger, STErrorCode)
{
    /// The operation could not be completed because one or more keys in the options dictionary were not recognized.
    STErrorOptionNotRecognized                             = 0,
    
    /// The operation could not be completed because one or more values in the options dictionary were invalid.
    STErrorOptionInvalidValue                              = 1,
    
    /// The operation could not be completed because one or more required values in the options dictionary were missing.
    STErrorOptionMissingValue                              = 2,
    
    /// The operation could not be completed because the options dictionary contained one or more non-dynamic properties that could not be updated.
    STErrorOptionCannotBeUpdated                           = 3,
    
    /// The operation could not be completed because parameters passed to the SDK method contains non-valid values.
    STErrorInvalidValue                                    = 10,
    
    /// STCameraPoseInitializer tried to initialize a camera pose using without a depth frame.
    STErrorCameraPoseInitializerDepthFrameMissing          = 20,
    
    /// No such file was found.
    STErrorFileNoSuchFile                                  = 30,
    
    /// The operation could not be completed because file output path is invalid.
    STErrorFileWriteInvalidFileName                        = 31,
    
    /// `STTracker` lost tracking.
    STErrorTrackerLostTrack                                = 40,
    
    /// `STTracker` is not initialized yet.
    STErrorTrackerNotInitialized                           = 41,
    
    /// `STTracker` doesn't support the input color sample buffer format.
    STErrorTrackerColorSampleBufferFormatNotSupported      = 42,
    
    /// `STTracker` doesn't have color sample buffer and cannot continue tracking.
    STErrorTrackerColorSampleBufferMissing                 = 43,
    
    /// `STTracker` detected the color sample buffer exposure has changed.
    STErrorTrackerColorExposureTimeChanged                 = 44,
    
    /// `STTracker` doesn't have device motion and cannot continue tracking.
    STErrorTrackerDeviceMotionMissing                      = 45,
    
    /// `STTracker` doesn't have live triangle mesh to proceed tracking against model. Make sure `setLiveTriangleMeshEnabled` is enabled.
    STErrorTrackerTrackAgainstModelWithoutLiveTriangleMesh = 46,

    /// `STTracker` could not return a high quality camera pose estimate.
    STErrorTrackerPoorQuality                              = 47,

    /// The STMesh operation could not be completed because it contains an empty mesh.
    STErrorMeshEmpty                                       = 60,
    
    /// The STMesh operation could not be completed because it was cancelled.
    STErrorMeshTaskCancelled                               = 61,
    
    /// The STMesh operation could not be completed because it contains an invalid texture format.
    STErrorMeshInvalidTextureFormat                        = 62,
    
    /// The colorize operation could not be completed because `STColorizer` doesn't have keyframes.
    STErrorColorizerNoKeyframes                            = 80,
    
    /// The colorize operation could not be completed because `STColorizer` doesn't have a mesh.
    STErrorColorizerEmptyMesh                              = 81,
};


//------------------------------------------------------------------------------
