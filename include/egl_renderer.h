/*
 * EGL Renderer for Termux Display Client
 * 
 * This module provides OpenGL ES rendering capabilities using EGL,
 * with dynamic loading of Android-specific extensions to avoid
 * conflicts with Termux environment.
 * 
 * Based on KWin Android EGL backend implementation.
 */

#pragma once

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <android/hardware_buffer.h>
#include <stdbool.h>
#include <dlfcn.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
struct LorieBuffer;

/**
 * EGL renderer context
 */
typedef struct {
    // EGL objects
    EGLDisplay display;
    EGLContext context;
    EGLConfig config;
    EGLSurface surface;
    
    // Dynamic loading of Android EGL extensions
    void *egl_android_lib;
    EGLClientBuffer (*eglGetNativeClientBufferANDROID)(const struct AHardwareBuffer *buffer);
    
    // OpenGL objects
    GLuint framebuffer;
    GLuint texture;
    EGLImageKHR egl_image;
    
    // State
    bool initialized;
    int width, height;
    
    // Performance monitoring
    struct {
        unsigned long frame_count;
        double total_render_time;
        double last_frame_time;
        struct timespec frame_start_time;
    } perf;
    
} EglRenderer;

/**
 * Initialize EGL renderer
 * @param renderer Renderer context to initialize
 * @return true on success, false on failure
 */
bool egl_renderer_init(EglRenderer *renderer);

/**
 * Cleanup EGL renderer
 * @param renderer Renderer context to cleanup
 */
void egl_renderer_cleanup(EglRenderer *renderer);

/**
 * Setup render target from AHardwareBuffer
 * @param renderer Renderer context
 * @param ahb AHardwareBuffer to use as render target
 * @param width Buffer width
 * @param height Buffer height
 * @return true on success, false on failure
 */
bool egl_renderer_setup_target(EglRenderer *renderer, AHardwareBuffer *ahb, int width, int height);

/**
 * Begin frame rendering
 * @param renderer Renderer context
 * @return true on success, false on failure
 */
bool egl_renderer_begin_frame(EglRenderer *renderer);

/**
 * End frame rendering and flush to target
 * @param renderer Renderer context
 * @return true on success, false on failure
 */
bool egl_renderer_end_frame(EglRenderer *renderer);

/**
 * Clear the render target with specified color
 * @param renderer Renderer context
 * @param r Red component (0.0-1.0)
 * @param g Green component (0.0-1.0)
 * @param b Blue component (0.0-1.0)
 * @param a Alpha component (0.0-1.0)
 */
void egl_renderer_clear(EglRenderer *renderer, float r, float g, float b, float a);

/**
 * Check if Android EGL extensions are available
 * @param renderer Renderer context
 * @return true if extensions are loaded, false otherwise
 */
bool egl_renderer_has_android_extensions(const EglRenderer *renderer);

/**
 * Get rendering performance statistics
 * @param renderer Renderer context
 * @param avg_frame_time_ms Average frame time in milliseconds (output)
 * @param fps Frames per second (output)
 * @param total_frames Total frames rendered (output)
 */
void egl_renderer_get_performance_stats(const EglRenderer *renderer, 
                                       double *avg_frame_time_ms, 
                                       double *fps, 
                                       unsigned long *total_frames);

/**
 * Reset performance statistics
 * @param renderer Renderer context
 */
void egl_renderer_reset_performance_stats(EglRenderer *renderer);

/**
 * Get detailed EGL/OpenGL information
 * @param renderer Renderer context
 */
void egl_renderer_print_info(const EglRenderer *renderer);

#ifdef __cplusplus
}
#endif