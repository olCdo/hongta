# Honta Android Handoff Package

This directory is the minimal package for Android integration.

## Contents

```text
libs/
  arm64-v8a/libhonta_native.so          Native library for Android devices
  x86_64/libhonta_native.so             Native library for Android Emulator
configs/
  runtime.json                          Runtime configuration
  camera/top_rgb_camera.json            Camera intrinsic and mounting configuration
  top_cover/A001.json                   Top-cover model data
api/
  honta_api.h                           C ABI header
java/
  com/honta/vision/HontaNative.java     Java wrapper example
  com/honta/vision/HontaNativeController.java
docs/
  android_api_reference.md              Android API reference
```

## Android Placement

Copy native libraries into the Android app:

```text
app/src/main/jniLibs/arm64-v8a/libhonta_native.so
app/src/main/jniLibs/x86_64/libhonta_native.so
```

Copy config files into an app-readable directory while preserving this structure:

```text
runtime.json
camera/top_rgb_camera.json
top_cover/A001.json
```

Pass the absolute device path of `runtime.json` to:

```java
HontaNative.init(configPath);
```

## Build Notes

The included `libhonta_native.so` files were built with real RTSP detection enabled:

```text
HONTA_ENABLE_MOCK_DETECTION=OFF
HONTA_HAS_REAL_DETECTION=ON
```

The native code includes the crop detection path: frames are cropped before circle detection, detected centers are offset back to original full-frame coordinates, and those full-frame coordinates are passed into calibration and map coordinate solving.
