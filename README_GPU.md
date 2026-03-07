# GPU-Accelerated Rendering for Termux Display Client

This document describes the GPU-accelerated rendering capabilities that have been migrated from KWin's Android EGL backend to the Termux Display Client.

## Overview

The GPU rendering system provides hardware-accelerated graphics using OpenGL ES 2.0 and EGL, with support for Android's AHardwareBuffer for efficient memory sharing between processes.

## Key Features

### 1. Dynamic Loading of Android EGL Extensions
- Avoids conflicts with Termux environment by loading EGL extensions at runtime
- Supports multiple library search paths for maximum compatibility
- Graceful fallback when extensions are not available

### 2. EGL Context Management
- Automatic EGL display and context creation
- Proper resource cleanup and error handling
- Support for OpenGL ES 2.0 with configurable attributes

### 3. AHardwareBuffer Integration
- Direct rendering to AHardwareBuffer via EGLImage
- Zero-copy texture binding for optimal performance
- Automatic framebuffer setup and management

### 4. Performance Monitoring
- Real-time frame timing statistics
- Average FPS calculation
- Detailed EGL/OpenGL information reporting

## Files Structure

```
termux-display-client/
├── include/
│   └── egl_renderer.h          # EGL renderer API
├── egl_renderer.c              # EGL renderer implementation
├── main_gpu.c                  # GPU rendering example
└── README_GPU.md               # This documentation
```

## API Reference

### Core Functions

```c
// Initialize EGL renderer
bool egl_renderer_init(EglRenderer *renderer);

// Setup render target from AHardwareBuffer
bool egl_renderer_setup_target(EglRenderer *renderer, AHardwareBuffer *ahb, int width, int height);

// Begin/end frame rendering
bool egl_renderer_begin_frame(EglRenderer *renderer);
bool egl_renderer_end_frame(EglRenderer *renderer);

// Clear screen with color
void egl_renderer_clear(EglRenderer *renderer, float r, float g, float b, float a);

// Cleanup resources
void egl_renderer_cleanup(EglRenderer *renderer);
```

### Performance Monitoring

```c
// Get performance statistics
void egl_renderer_get_performance_stats(const EglRenderer *renderer, 
                                       double *avg_frame_time_ms, 
                                       double *fps, 
                                       unsigned long *total_frames);

// Print detailed renderer information
void egl_renderer_print_info(const EglRenderer *renderer);

// Reset performance counters
void egl_renderer_reset_performance_stats(EglRenderer *renderer);
```

## Usage Example

```c
#include "include/egl_renderer.h"

EglRenderer renderer;

// Initialize
if (!egl_renderer_init(&renderer)) {
    // Handle error
    return -1;
}

// Setup render target
if (!egl_renderer_setup_target(&renderer, ahb, width, height)) {
    // Handle error
    egl_renderer_cleanup(&renderer);
    return -1;
}

// Render loop
while (running) {
    // Begin frame
    if (!egl_renderer_begin_frame(&renderer)) {
        break;
    }
    
    // Clear screen
    egl_renderer_clear(&renderer, 1.0f, 0.0f, 0.0f, 1.0f);  // Red
    
    // Add your rendering code here...
    
    // End frame
    if (!egl_renderer_end_frame(&renderer)) {
        break;
    }
}

// Cleanup
egl_renderer_cleanup(&renderer);
```

## Building

The GPU rendering functionality is automatically included when building the project:

```bash
mkdir build
cd build
cmake ..
make
```

This will create two executables:
- `termux-render-sample` - Original CPU-based rendering
- `termux-render-gpu` - New GPU-accelerated rendering

## Running

### GPU-Accelerated Example

```bash
./termux-render-gpu --width=1920 --height=1080 --fps=30
```

### Command Line Options

- `--width=N` - Set render width (default: 1080)
- `--height=N` - Set render height (default: 720)
- `--fps=N` - Set target FPS (default: 10, max: 60)

## Performance Benefits

GPU rendering provides several advantages over CPU-based rendering:

1. **Hardware Acceleration**: Utilizes GPU for parallel processing
2. **Reduced CPU Usage**: Frees up CPU for other tasks
3. **Better Performance**: Higher frame rates and smoother animation
4. **Power Efficiency**: GPUs are more efficient for graphics operations
5. **Advanced Effects**: Enables shaders, textures, and complex rendering

## Compatibility

### Requirements
- Android device with OpenGL ES 2.0 support
- EGL 1.4 or later
- AHardwareBuffer support (Android API 26+)

### Tested Environments
- Termux on Android 8.0+ devices
- Various GPU vendors (Adreno, Mali, PowerVR)

## Troubleshooting

### Common Issues

1. **EGL initialization fails**
   - Check if device supports OpenGL ES 2.0
   - Verify EGL libraries are available

2. **Android extensions not available**
   - This is expected in some environments
   - The system will still work with reduced functionality

3. **Performance issues**
   - Use performance monitoring to identify bottlenecks
   - Reduce resolution or frame rate if needed

### Debug Information

Enable detailed logging by setting the log level:

```c
// Print detailed renderer information
egl_renderer_print_info(&renderer);

// Monitor performance
double avg_time, fps;
unsigned long frames;
egl_renderer_get_performance_stats(&renderer, &avg_time, &fps, &frames);
printf("Performance: %.2f ms/frame, %.2f FPS\n", avg_time, fps);
```

## Migration from KWin

The GPU rendering system is based on KWin's Android EGL backend with the following key adaptations:

1. **Simplified Architecture**: Removed KWin-specific abstractions
2. **Dynamic Loading**: Added runtime loading of Android extensions
3. **C API**: Converted from C++ to C for better compatibility
4. **Performance Monitoring**: Added built-in performance tracking
5. **Error Handling**: Improved error reporting and recovery

## Future Enhancements

Potential improvements for future versions:

1. **Texture Loading**: Support for loading and rendering textures
2. **Shader Support**: Custom fragment and vertex shaders
3. **Geometry Rendering**: Support for 2D/3D geometry
4. **Multi-threading**: Parallel rendering capabilities
5. **Vulkan Support**: Next-generation graphics API support

## Contributing

When contributing to the GPU rendering system:

1. Follow the existing code style and patterns
2. Add appropriate error checking and logging
3. Update performance monitoring as needed
4. Test on multiple Android devices and GPU vendors
5. Update this documentation for any API changes