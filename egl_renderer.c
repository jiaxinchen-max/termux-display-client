/*
 * EGL Renderer Implementation for Termux Display Client
 */

#include "include/egl_renderer.h"
#include "include/tlog.h"
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>

// Static helper functions
static bool load_android_egl_extensions(EglRenderer *renderer);
static void unload_android_egl_extensions(EglRenderer *renderer);
static bool create_egl_context(EglRenderer *renderer);
static void cleanup_render_target(EglRenderer *renderer);
static double get_time_diff_ms(struct timespec *start, struct timespec *end);

bool egl_renderer_init(EglRenderer *renderer)
{
    if (!renderer) {
        LOGE("Invalid renderer parameter");
        return false;
    }
    
    // Initialize structure
    memset(renderer, 0, sizeof(EglRenderer));
    renderer->display = EGL_NO_DISPLAY;
    renderer->context = EGL_NO_CONTEXT;
    renderer->surface = EGL_NO_SURFACE;
    renderer->egl_image = EGL_NO_IMAGE_KHR;
    
    LOGI("Initializing EGL renderer");
    
    // Load Android EGL extensions dynamically
    if (!load_android_egl_extensions(renderer)) {
        LOGW("Failed to load Android EGL extensions - some features may not work");
    }
    
    // Get EGL display
    renderer->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (renderer->display == EGL_NO_DISPLAY) {
        LOGE("eglGetDisplay failed");
        return false;
    }
    
    // Initialize EGL
    EGLint major, minor;
    if (!eglInitialize(renderer->display, &major, &minor)) {
        LOGE("eglInitialize failed: 0x%x", eglGetError());
        return false;
    }
    
    LOGI("EGL version: %d.%d", major, minor);
    
    // Bind OpenGL ES API
    if (!eglBindAPI(EGL_OPENGL_ES_API)) {
        LOGE("eglBindAPI(EGL_OPENGL_ES_API) failed: 0x%x", eglGetError());
        eglTerminate(renderer->display);
        renderer->display = EGL_NO_DISPLAY;
        return false;
    }
    
    // Create EGL context
    if (!create_egl_context(renderer)) {
        LOGE("Failed to create EGL context");
        eglTerminate(renderer->display);
        renderer->display = EGL_NO_DISPLAY;
        return false;
    }
    
    // Initialize performance monitoring
    memset(&renderer->perf, 0, sizeof(renderer->perf));
    
    renderer->initialized = true;
    LOGI("EGL renderer initialized successfully");
    
    return true;
}

void egl_renderer_cleanup(EglRenderer *renderer)
{
    if (!renderer) {
        return;
    }
    
    LOGI("Cleaning up EGL renderer");
    
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
        LOGE("Invalid parameters for setup_target");
        return false;
    }
    
    // Check if Android EGL extensions are available
    if (!renderer->eglGetNativeClientBufferANDROID) {
        LOGE("eglGetNativeClientBufferANDROID not available - Android EGL extensions not loaded");
        return false;
    }
    
    LOGI("Setting up render target: %dx%d", width, height);
    
    // Make context current
    if (!eglMakeCurrent(renderer->display, renderer->surface, renderer->surface, renderer->context)) {
        LOGE("eglMakeCurrent failed: 0x%x", eglGetError());
        return false;
    }
    
    // Cleanup previous render target if exists
    cleanup_render_target(renderer);
    
    // Convert AHardwareBuffer to EGLClientBuffer
    EGLClientBuffer clientBuffer = renderer->eglGetNativeClientBufferANDROID(ahb);
    if (!clientBuffer) {
        LOGE("eglGetNativeClientBufferANDROID failed");
        return false;
    }
    
    // Create EGLImage from AHardwareBuffer
    const EGLint imageAttribs[] = {
        EGL_IMAGE_PRESERVED_KHR, EGL_TRUE,
        EGL_NONE
    };
    
    renderer->egl_image = eglCreateImageKHR(
        renderer->display,
        EGL_NO_CONTEXT,
        EGL_NATIVE_BUFFER_ANDROID,
        clientBuffer,
        imageAttribs
    );
    
    if (renderer->egl_image == EGL_NO_IMAGE_KHR) {
        LOGE("eglCreateImageKHR failed: 0x%x", eglGetError());
        return false;
    }
    
    LOGI("EGLImage created successfully");
    
    // Create OpenGL texture and bind EGLImage to it
    glGenTextures(1, &renderer->texture);
    glBindTexture(GL_TEXTURE_2D, renderer->texture);
    
    // Bind EGLImage to texture
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, (GLeglImageOES)renderer->egl_image);
    
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        LOGE("glEGLImageTargetTexture2DOES failed: 0x%x", error);
        cleanup_render_target(renderer);
        return false;
    }
    
    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    LOGI("OpenGL texture created and bound to EGLImage");
    
    // Create framebuffer and attach the texture
    glGenFramebuffers(1, &renderer->framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, renderer->framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, renderer->texture, 0);
    
    // Check framebuffer status
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        LOGE("Framebuffer incomplete: 0x%x", status);
        cleanup_render_target(renderer);
        return false;
    }
    
    LOGI("Framebuffer created and complete");
    
    // Store dimensions
    renderer->width = width;
    renderer->height = height;
    
    // Set viewport
    glViewport(0, 0, width, height);
    
    // Unbind framebuffer for now
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    LOGI("Render target setup complete");
    return true;
}

bool egl_renderer_begin_frame(EglRenderer *renderer)
{
    if (!renderer || !renderer->initialized || renderer->framebuffer == 0) {
        LOGE("Invalid renderer state for begin_frame");
        return false;
    }
    
    // Start performance timing
    clock_gettime(CLOCK_MONOTONIC, &renderer->perf.frame_start_time);
    
    // Make context current
    if (!eglMakeCurrent(renderer->display, renderer->surface, renderer->surface, renderer->context)) {
        LOGE("eglMakeCurrent failed in begin_frame: 0x%x", eglGetError());
        return false;
    }
    
    // Bind our framebuffer for rendering
    glBindFramebuffer(GL_FRAMEBUFFER, renderer->framebuffer);
    
    return true;
}

bool egl_renderer_end_frame(EglRenderer *renderer)
{
    if (!renderer || !renderer->initialized) {
        LOGE("Invalid renderer state for end_frame");
        return false;
    }
    
    // Ensure all rendering commands are flushed to the AHardwareBuffer
    glFlush();
    glFinish();  // Wait for GPU to complete
    
    // Unbind framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    // Update performance statistics
    struct timespec frame_end_time;
    clock_gettime(CLOCK_MONOTONIC, &frame_end_time);
    
    double frame_time = get_time_diff_ms(&renderer->perf.frame_start_time, &frame_end_time);
    renderer->perf.last_frame_time = frame_time;
    renderer->perf.total_render_time += frame_time;
    renderer->perf.frame_count++;
    
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

// Static helper function implementations

static bool load_android_egl_extensions(EglRenderer *renderer)
{
    LOGI("Loading Android EGL extensions dynamically");
    
    // Try to load libEGL.so first (standard location)
    renderer->egl_android_lib = dlopen("libEGL.so", RTLD_LAZY | RTLD_LOCAL);
    if (!renderer->egl_android_lib) {
        // Try alternative locations
        renderer->egl_android_lib = dlopen("libEGL.so.1", RTLD_LAZY | RTLD_LOCAL);
    }
    if (!renderer->egl_android_lib) {
        renderer->egl_android_lib = dlopen("/system/lib64/libEGL.so", RTLD_LAZY | RTLD_LOCAL);
    }
    if (!renderer->egl_android_lib) {
        renderer->egl_android_lib = dlopen("/system/lib/libEGL.so", RTLD_LAZY | RTLD_LOCAL);
    }
    
    if (!renderer->egl_android_lib) {
        LOGW("Failed to load Android EGL library: %s", dlerror());
        return false;
    }
    
    // Load eglGetNativeClientBufferANDROID function
    renderer->eglGetNativeClientBufferANDROID = (EGLClientBuffer (*)(const struct AHardwareBuffer *))
        dlsym(renderer->egl_android_lib, "eglGetNativeClientBufferANDROID");
    if (!renderer->eglGetNativeClientBufferANDROID) {
        LOGW("Failed to load eglGetNativeClientBufferANDROID: %s", dlerror());
        dlclose(renderer->egl_android_lib);
        renderer->egl_android_lib = NULL;
        return false;
    }
    
    LOGI("Successfully loaded Android EGL extensions");
    return true;
}

static void unload_android_egl_extensions(EglRenderer *renderer)
{
    if (renderer->egl_android_lib) {
        LOGI("Unloading Android EGL extensions");
        dlclose(renderer->egl_android_lib);
        renderer->egl_android_lib = NULL;
        renderer->eglGetNativeClientBufferANDROID = NULL;
    }
}

static bool create_egl_context(EglRenderer *renderer)
{
    LOGI("Creating EGL context");
    
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
        LOGE("eglChooseConfig failed: 0x%x", eglGetError());
        return false;
    }
    
    LOGI("EGL config chosen");
    
    // Create EGL context for OpenGL ES 2.0
    const EGLint contextAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };
    
    renderer->context = eglCreateContext(renderer->display, renderer->config, EGL_NO_CONTEXT, contextAttribs);
    if (renderer->context == EGL_NO_CONTEXT) {
        LOGE("eglCreateContext failed: 0x%x", eglGetError());
        return false;
    }
    
    // Create a pbuffer surface for makeCurrent (since we render to FBO)
    const EGLint pbufferAttribs[] = {
        EGL_WIDTH, 1,
        EGL_HEIGHT, 1,
        EGL_NONE
    };
    
    renderer->surface = eglCreatePbufferSurface(renderer->display, renderer->config, pbufferAttribs);
    if (renderer->surface == EGL_NO_SURFACE) {
        LOGE("eglCreatePbufferSurface failed: 0x%x", eglGetError());
        return false;
    }
    
    // Make context current
    if (!eglMakeCurrent(renderer->display, renderer->surface, renderer->surface, renderer->context)) {
        LOGE("eglMakeCurrent failed: 0x%x", eglGetError());
        return false;
    }
    
    LOGI("EGL context created and made current");
    LOGI("GL_VENDOR: %s", (const char*)glGetString(GL_VENDOR));
    LOGI("GL_RENDERER: %s", (const char*)glGetString(GL_RENDERER));
    LOGI("GL_VERSION: %s", (const char*)glGetString(GL_VERSION));
    
    return true;
}

static void cleanup_render_target(EglRenderer *renderer)
{
    if (!renderer->initialized) {
        return;
    }
    
    // Make context current for cleanup
    eglMakeCurrent(renderer->display, renderer->surface, renderer->surface, renderer->context);
    
    if (renderer->framebuffer) {
        glDeleteFramebuffers(1, &renderer->framebuffer);
        renderer->framebuffer = 0;
    }
    
    if (renderer->texture) {
        glDeleteTextures(1, &renderer->texture);
        renderer->texture = 0;
    }
    
    if (renderer->egl_image != EGL_NO_IMAGE_KHR) {
        eglDestroyImageKHR(renderer->display, renderer->egl_image);
        renderer->egl_image = EGL_NO_IMAGE_KHR;
    }
    
    renderer->width = 0;
    renderer->height = 0;
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
    
    if (total_frames) {
        *total_frames = renderer->perf.frame_count;
    }
    
    if (renderer->perf.frame_count > 0) {
        double avg_time = renderer->perf.total_render_time / renderer->perf.frame_count;
        if (avg_frame_time_ms) {
            *avg_frame_time_ms = avg_time;
        }
        if (fps) {
            *fps = (avg_time > 0.0) ? (1000.0 / avg_time) : 0.0;
        }
    } else {
        if (avg_frame_time_ms) *avg_frame_time_ms = 0.0;
        if (fps) *fps = 0.0;
    }
}

void egl_renderer_reset_performance_stats(EglRenderer *renderer)
{
    if (!renderer) return;
    
    memset(&renderer->perf, 0, sizeof(renderer->perf));
}

void egl_renderer_print_info(const EglRenderer *renderer)
{
    if (!renderer || !renderer->initialized) {
        LOGI("EGL Renderer: Not initialized");
        return;
    }
    
    LOGI("=== EGL Renderer Information ===");
    LOGI("EGL Display: %p", renderer->display);
    LOGI("EGL Context: %p", renderer->context);
    LOGI("EGL Surface: %p", renderer->surface);
    LOGI("Render Target: %dx%d", renderer->width, renderer->height);
    LOGI("Framebuffer ID: %u", renderer->framebuffer);
    LOGI("Texture ID: %u", renderer->texture);
    LOGI("EGL Image: %p", renderer->egl_image);
    
    // EGL information
    const char *egl_vendor = eglQueryString(renderer->display, EGL_VENDOR);
    const char *egl_version = eglQueryString(renderer->display, EGL_VERSION);
    const char *egl_extensions = eglQueryString(renderer->display, EGL_EXTENSIONS);
    
    LOGI("EGL Vendor: %s", egl_vendor ? egl_vendor : "Unknown");
    LOGI("EGL Version: %s", egl_version ? egl_version : "Unknown");
    LOGI("EGL Extensions: %s", egl_extensions ? egl_extensions : "None");
    
    // OpenGL information
    const char *gl_vendor = (const char*)glGetString(GL_VENDOR);
    const char *gl_renderer = (const char*)glGetString(GL_RENDERER);
    const char *gl_version = (const char*)glGetString(GL_VERSION);
    const char *gl_extensions = (const char*)glGetString(GL_EXTENSIONS);
    
    LOGI("GL Vendor: %s", gl_vendor ? gl_vendor : "Unknown");
    LOGI("GL Renderer: %s", gl_renderer ? gl_renderer : "Unknown");
    LOGI("GL Version: %s", gl_version ? gl_version : "Unknown");
    LOGI("GL Extensions: %s", gl_extensions ? gl_extensions : "None");
    
    // Android extensions availability
    LOGI("Android EGL Extensions: %s", 
         egl_renderer_has_android_extensions(renderer) ? "Available" : "Not Available");
    
    // Performance statistics
    double avg_frame_time, fps;
    unsigned long total_frames;
    egl_renderer_get_performance_stats(renderer, &avg_frame_time, &fps, &total_frames);
    
    LOGI("=== Performance Statistics ===");
    LOGI("Total Frames: %lu", total_frames);
    LOGI("Average Frame Time: %.2f ms", avg_frame_time);
    LOGI("Average FPS: %.2f", fps);
    LOGI("Last Frame Time: %.2f ms", renderer->perf.last_frame_time);
    LOGI("===============================");
}

static double get_time_diff_ms(struct timespec *start, struct timespec *end)
{
    double start_ms = start->tv_sec * 1000.0 + start->tv_nsec / 1000000.0;
    double end_ms = end->tv_sec * 1000.0 + end->tv_nsec / 1000000.0;
    return end_ms - start_ms;
}