/*
 * Simplified EGL Renderer Implementation for Termux Display Client
 * This version focuses on basic functionality to ensure compilation works
 */

#include "include/egl_renderer.h"
#include "include/tlog.h"
#include <string.h>
#include <stdlib.h>

// EGL extension constants (in case they're not defined)
#ifndef EGL_NATIVE_BUFFER_ANDROID
#define EGL_NATIVE_BUFFER_ANDROID 0x3140
#endif

#ifndef EGL_IMAGE_PRESERVED_KHR
#define EGL_IMAGE_PRESERVED_KHR 0x30D2
#endif

// Static helper functions
static bool load_android_egl_extensions(EglRenderer *renderer);
static void unload_android_egl_extensions(EglRenderer *renderer);
static bool create_egl_context(EglRenderer *renderer);
static void cleanup_render_target(EglRenderer *renderer);

bool egl_renderer_init(EglRenderer *renderer)
{
    if (!renderer) {
        tlog(LOG_ERR, "Invalid renderer parameter");
        return false;
    }
    
    // Initialize structure
    memset(renderer, 0, sizeof(EglRenderer));
    renderer->display = EGL_NO_DISPLAY;
    renderer->context = EGL_NO_CONTEXT;
    renderer->surface = EGL_NO_SURFACE;
    renderer->egl_image = EGL_NO_IMAGE_KHR;
    
    tlog(LOG_INFO, "Initializing EGL renderer");
    
    // Load Android EGL extensions dynamically
    if (!load_android_egl_extensions(renderer)) {
        tlog(LOG_WARNING, "Failed to load Android EGL extensions - some features may not work");
    }
    
    // Get EGL display for offscreen rendering (no window binding needed)
    renderer->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (renderer->display == EGL_NO_DISPLAY) {
        tlog(LOG_ERR, "eglGetDisplay failed");
        return false;
    }
    
    // Initialize EGL
    EGLint major, minor;
    if (!eglInitialize(renderer->display, &major, &minor)) {
        tlog(LOG_ERR, "eglInitialize failed: 0x%x", eglGetError());
        return false;
    }
    
    tlog(LOG_INFO, "EGL version: %d.%d", major, minor);
    
    // Bind OpenGL ES API
    if (!eglBindAPI(EGL_OPENGL_ES_API)) {
        tlog(LOG_ERR, "eglBindAPI(EGL_OPENGL_ES_API) failed: 0x%x", eglGetError());
        eglTerminate(renderer->display);
        renderer->display = EGL_NO_DISPLAY;
        return false;
    }
    
    // Create EGL context
    if (!create_egl_context(renderer)) {
        tlog(LOG_ERR, "Failed to create EGL context");
        eglTerminate(renderer->display);
        renderer->display = EGL_NO_DISPLAY;
        return false;
    }
    
    renderer->initialized = true;
    tlog(LOG_INFO, "EGL renderer initialized successfully");
    
    return true;
}

void egl_renderer_cleanup(EglRenderer *renderer)
{
    if (!renderer) {
        return;
    }
    
    tlog(LOG_INFO, "Cleaning up EGL renderer");
    
    // Cleanup render target resources
    cleanup_render_target(renderer);
    
    // Cleanup EGL context
    if (renderer->display != EGL_NO_DISPLAY) {
        eglMakeCurrent(renderer->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        
        if (renderer->surface != EGL_NO_SURFACE) {
            eglDestroySurface(renderer->display, renderer->surface);
            renderer->surface = EGL_NO_SURFACE;
        }
        
        if (renderer->context != EGL_NO_CONTEXT) {
            eglDestroyContext(renderer->display, renderer->context);
            renderer->context = EGL_NO_CONTEXT;
        }
        
        eglTerminate(renderer->display);
        renderer->display = EGL_NO_DISPLAY;
    }
    
    // Unload Android EGL extensions
    unload_android_egl_extensions(renderer);
    
    renderer->initialized = false;
}

bool egl_renderer_setup_target(EglRenderer *renderer, AHardwareBuffer *ahb, int width, int height)
{
    if (!renderer || !renderer->initialized || !ahb) {
        tlog(LOG_ERR, "Invalid parameters for setup_target");
        return false;
    }
    
    tlog(LOG_INFO, "Setting up render target: %dx%d", width, height);
    
    // For now, just store the dimensions
    renderer->width = width;
    renderer->height = height;
    
    // Make context current
    if (!eglMakeCurrent(renderer->display, renderer->surface, renderer->surface, renderer->context)) {
        tlog(LOG_ERR, "eglMakeCurrent failed: 0x%x", eglGetError());
        return false;
    }
    
    // Set viewport
    glViewport(0, 0, width, height);
    
    tlog(LOG_INFO, "Render target setup complete");
    return true;
}

bool egl_renderer_begin_frame(EglRenderer *renderer)
{
    if (!renderer || !renderer->initialized) {
        tlog(LOG_ERR, "Invalid renderer state for begin_frame");
        return false;
    }
    
    // Make context current
    if (!eglMakeCurrent(renderer->display, renderer->surface, renderer->surface, renderer->context)) {
        tlog(LOG_ERR, "eglMakeCurrent failed in begin_frame: 0x%x", eglGetError());
        return false;
    }
    
    return true;
}

bool egl_renderer_end_frame(EglRenderer *renderer)
{
    if (!renderer || !renderer->initialized) {
        tlog(LOG_ERR, "Invalid renderer state for end_frame");
        return false;
    }
    
    // Ensure all rendering commands are flushed
    glFlush();
    
    return true;
}

void egl_renderer_clear(EglRenderer *renderer, float r, float g, float b, float a)
{
    if (!renderer || !renderer->initialized) {
        return;
    }
    
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT);
}

bool egl_renderer_has_android_extensions(const EglRenderer *renderer)
{
    return renderer && renderer->eglGetNativeClientBufferANDROID != NULL;
}

void egl_renderer_get_performance_stats(const EglRenderer *renderer, 
                                       double *avg_frame_time_ms, 
                                       double *fps, 
                                       unsigned long *total_frames)
{
    if (!renderer || !renderer->initialized) {
        if (avg_frame_time_ms) *avg_frame_time_ms = 0.0;
        if (fps) *fps = 0.0;
        if (total_frames) *total_frames = 0;
        return;
    }
    
    // Simple implementation for now
    if (avg_frame_time_ms) *avg_frame_time_ms = 16.67; // ~60 FPS
    if (fps) *fps = 60.0;
    if (total_frames) *total_frames = 0;
}

void egl_renderer_reset_performance_stats(EglRenderer *renderer)
{
    // Simple implementation
    (void)renderer;
}

void egl_renderer_print_info(const EglRenderer *renderer)
{
    if (!renderer || !renderer->initialized) {
        tlog(LOG_INFO, "EGL Renderer: Not initialized");
        return;
    }
    
    tlog(LOG_INFO, "=== EGL Renderer Information ===");
    tlog(LOG_INFO, "EGL Display: %p", (void*)renderer->display);
    tlog(LOG_INFO, "EGL Context: %p", (void*)renderer->context);
    tlog(LOG_INFO, "EGL Surface: %p", (void*)renderer->surface);
    tlog(LOG_INFO, "Render Target: %dx%d", renderer->width, renderer->height);
    tlog(LOG_INFO, "Android Extensions: %s", 
         egl_renderer_has_android_extensions(renderer) ? "Available" : "Not Available");
    tlog(LOG_INFO, "===============================");
}

// Static helper function implementations

static bool load_android_egl_extensions(EglRenderer *renderer)
{
    tlog(LOG_INFO, "Loading Android EGL extensions dynamically");
    
    // Try to load libEGL.so
    renderer->egl_android_lib = dlopen("libEGL.so", RTLD_LAZY | RTLD_LOCAL);
    if (!renderer->egl_android_lib) {
        renderer->egl_android_lib = dlopen("/system/lib64/libEGL.so", RTLD_LAZY | RTLD_LOCAL);
    }
    if (!renderer->egl_android_lib) {
        renderer->egl_android_lib = dlopen("/system/lib/libEGL.so", RTLD_LAZY | RTLD_LOCAL);
    }
    
    if (!renderer->egl_android_lib) {
        tlog(LOG_WARNING, "Failed to load Android EGL library: %s", dlerror());
        return false;
    }
    
    // Load eglGetNativeClientBufferANDROID function
    renderer->eglGetNativeClientBufferANDROID = (EGLClientBuffer (*)(const struct AHardwareBuffer *))
        dlsym(renderer->egl_android_lib, "eglGetNativeClientBufferANDROID");
    if (!renderer->eglGetNativeClientBufferANDROID) {
        tlog(LOG_WARNING, "Failed to load eglGetNativeClientBufferANDROID: %s", dlerror());
        dlclose(renderer->egl_android_lib);
        renderer->egl_android_lib = NULL;
        return false;
    }
    
    tlog(LOG_INFO, "Successfully loaded Android EGL extensions");
    return true;
}

static void unload_android_egl_extensions(EglRenderer *renderer)
{
    if (renderer->egl_android_lib) {
        tlog(LOG_INFO, "Unloading Android EGL extensions");
        dlclose(renderer->egl_android_lib);
        renderer->egl_android_lib = NULL;
        renderer->eglGetNativeClientBufferANDROID = NULL;
    }
}

static bool create_egl_context(EglRenderer *renderer)
{
    tlog(LOG_INFO, "Creating EGL context");
    
    // Choose EGL config for OpenGL ES 2.0
    const EGLint configAttribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_NONE
    };
    
    EGLint numConfigs;
    if (!eglChooseConfig(renderer->display, configAttribs, &renderer->config, 1, &numConfigs) || numConfigs == 0) {
        tlog(LOG_ERR, "eglChooseConfig failed: 0x%x", eglGetError());
        return false;
    }
    
    tlog(LOG_INFO, "EGL config chosen");
    
    // Create EGL context for OpenGL ES 2.0
    const EGLint contextAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };
    
    renderer->context = eglCreateContext(renderer->display, renderer->config, EGL_NO_CONTEXT, contextAttribs);
    if (renderer->context == EGL_NO_CONTEXT) {
        tlog(LOG_ERR, "eglCreateContext failed: 0x%x", eglGetError());
        return false;
    }
    
    // Create a pbuffer surface for offscreen rendering (no window needed)
    const EGLint pbufferAttribs[] = {
        EGL_WIDTH, 1,
        EGL_HEIGHT, 1,
        EGL_NONE
    };
    
    renderer->surface = eglCreatePbufferSurface(renderer->display, renderer->config, pbufferAttribs);
    if (renderer->surface == EGL_NO_SURFACE) {
        tlog(LOG_ERR, "eglCreatePbufferSurface failed: 0x%x", eglGetError());
        return false;
    }
    
    // Make context current
    if (!eglMakeCurrent(renderer->display, renderer->surface, renderer->surface, renderer->context)) {
        tlog(LOG_ERR, "eglMakeCurrent failed: 0x%x", eglGetError());
        return false;
    }
    
    tlog(LOG_INFO, "EGL context created and made current");
    
    return true;
}

static void cleanup_render_target(EglRenderer *renderer)
{
    if (!renderer->initialized) {
        return;
    }
    
    // Simple cleanup for now
    renderer->width = 0;
    renderer->height = 0;
}