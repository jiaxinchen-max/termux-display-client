/*
 * Simplified GPU-accelerated rendering example for Termux Display Client
 */

#include <stdio.h>
#include <signal.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

#include "include/render.h"
#include "include/tlog.h"
#include "include/egl_renderer.h"

volatile sig_atomic_t keep_running = 1;
extern LorieBuffer* lorieBuffer;
extern struct lorie_shared_server_state *serverState;

static EglRenderer renderer;
static int frame_number = 0;
static bool renderer_initialized = false;

// Simple animation parameters
static float animation_time = 0.0f;
static const float animation_speed = 2.0f;

static int setup_gpu_renderer() {
    tlog(LOG_INFO, "Setting up GPU renderer");
    
    // Initialize EGL renderer
    if (!egl_renderer_init(&renderer)) {
        tlog(LOG_ERR, "Failed to initialize EGL renderer");
        return -1;
    }
    
    // Check if Android extensions are available
    if (!egl_renderer_has_android_extensions(&renderer)) {
        tlog(LOG_WARNING, "Android EGL extensions not available - some features may be limited");
    }
    
    // Print detailed renderer information
    egl_renderer_print_info(&renderer);
    
    renderer_initialized = true;
    tlog(LOG_INFO, "GPU renderer setup complete");
    return 0;
}

static void cleanup_gpu_renderer() {
    if (renderer_initialized) {
        tlog(LOG_INFO, "Cleaning up GPU renderer");
        egl_renderer_cleanup(&renderer);
        renderer_initialized = false;
    }
}

static int render_animated_frame() {
    if (!renderer_initialized) {
        tlog(LOG_ERR, "Renderer not initialized");
        return -1;
    }
    
    // Lock the server state
    lorie_mutex_lock(&serverState->lock, &serverState->lockingPid);
    
    const LorieBuffer_Desc *desc = LorieBuffer_description(lorieBuffer);
    int bufferWidth = desc->width;
    int bufferHeight = desc->height;
    
    // Setup render target if not already done or if buffer changed
    static int last_width = 0, last_height = 0;
    if (last_width != bufferWidth || last_height != bufferHeight) {
        tlog(LOG_INFO, "Setting up render target: %dx%d", bufferWidth, bufferHeight);
        if (!egl_renderer_setup_target(&renderer, desc->buffer, bufferWidth, bufferHeight)) {
            tlog(LOG_ERR, "Failed to setup render target");
            lorie_mutex_unlock(&serverState->lock, &serverState->lockingPid);
            return -1;
        }
        last_width = bufferWidth;
        last_height = bufferHeight;
    }
    
    // Begin frame rendering
    if (!egl_renderer_begin_frame(&renderer)) {
        tlog(LOG_ERR, "Failed to begin frame");
        lorie_mutex_unlock(&serverState->lock, &serverState->lockingPid);
        return -1;
    }
    
    // Create animated colors based on time
    float r = (sin(animation_time) + 1.0f) * 0.5f;
    float g = (sin(animation_time + 2.094f) + 1.0f) * 0.5f;  // 2π/3
    float b = (sin(animation_time + 4.188f) + 1.0f) * 0.5f;  // 4π/3
    
    // Clear the screen with animated color
    egl_renderer_clear(&renderer, r, g, b, 1.0f);
    
    // End frame rendering
    if (!egl_renderer_end_frame(&renderer)) {
        tlog(LOG_ERR, "Failed to end frame");
        lorie_mutex_unlock(&serverState->lock, &serverState->lockingPid);
        return -1;
    }
    
    // Signal that a new frame is ready
    serverState->waitForNextFrame = false;
    serverState->drawRequested = 1;
    pthread_cond_signal(&serverState->cond);
    
    lorie_mutex_unlock(&serverState->lock, &serverState->lockingPid);
    
    // Update animation time
    animation_time += animation_speed * 0.1f;  // Assuming ~10 FPS
    frame_number++;
    
    // Report performance statistics periodically
    if (frame_number % 100 == 0) {
        double avg_frame_time, fps;
        unsigned long total_frames;
        egl_renderer_get_performance_stats(&renderer, &avg_frame_time, &fps, &total_frames);
        tlog(LOG_INFO, "Performance: Frame %d, Avg: %.2f ms/frame, FPS: %.2f", 
             frame_number, avg_frame_time, fps);
    }
    
    return 0;
}

void timer_handler(int signum) {
    (void)signum;
    render_animated_frame();
}

void int_handler(int signum) {
    (void)signum;
    keep_running = 0;
}

void exit_handler(void) {
    cleanup_gpu_renderer();
    stopEventLoop();
    exit(0);
}

int main(int count, char** argv) {
    int fps = 10;
    int width = 1080;
    int height = 720;
    
    // Parse command line arguments
    for (int i = 1; i < count; i++) {
        char *arg = argv[i];
        char *value = NULL;
        char key[64] = {0};
        
        if (strncmp(arg, "--", 2) != 0) {
            continue;
        }
        
        char *eq = strchr(arg, '=');
        if (eq) {
            int key_len = eq - arg;
            if (key_len < 64) {
                strncpy(key, arg, key_len);
                value = eq + 1;
            }
        } else if (i + 1 < count) {
            strncpy(key, arg, 63);
            value = argv[++i];
        }
        
        if (!value) continue;
        
        if (strcmp(key, "--fps") == 0) {
            fps = atoi(value);
            if (fps <= 0 || fps > 60) {
                tlog(LOG_ERR, "Invalid fps: %d (range: 1-60). Using default 10", fps);
                fps = 10;
            }
        } else if (strcmp(key, "--width") == 0) {
            width = atoi(value);
            if (width <= 0) {
                tlog(LOG_ERR, "Invalid width: %d. Using default 1080", width);
                width = 1080;
            }
        } else if (strcmp(key, "--height") == 0) {
            height = atoi(value);
            if (height <= 0) {
                tlog(LOG_ERR, "Invalid height: %d. Using default 720", height);
                height = 720;
            }
        }
    }
    
    int interval_usec = 1000000 / fps;
    
    tlog(LOG_INFO, "Starting GPU-accelerated rendering demo");
    tlog(LOG_INFO, "Resolution: %dx%d, FPS: %d", width, height, fps);
    
    // Set screen configuration
    setScreenConfig(width, height, fps);
    
    // Connect to render service
    if (connectToRender() != 0) {
        tlog(LOG_ERR, "Failed to connect to render service");
        return 1;
    }
    
    // Setup GPU renderer
    if (setup_gpu_renderer() != 0) {
        tlog(LOG_ERR, "Failed to setup GPU renderer");
        return 1;
    }
    
    // Set exit callback
    setExitCallback(exit_handler);
    
    // Setup signal handlers
    struct sigaction sa_timer, sa_int;
    struct itimerval timer;
    
    sa_timer.sa_handler = timer_handler;
    sa_timer.sa_flags = SA_RESTART;
    sigemptyset(&sa_timer.sa_mask);
    if (sigaction(SIGALRM, &sa_timer, NULL) == -1) {
        tlog(LOG_ERR, "sigaction timer failed");
        cleanup_gpu_renderer();
        return 1;
    }
    
    sa_int.sa_handler = int_handler;
    sa_int.sa_flags = 0;
    sigemptyset(&sa_int.sa_mask);
    if (sigaction(SIGINT, &sa_int, NULL) == -1) {
        tlog(LOG_ERR, "sigaction int failed");
        cleanup_gpu_renderer();
        return 1;
    }
    
    // Setup timer
    timer.it_value.tv_sec = interval_usec / 1000000;
    timer.it_value.tv_usec = interval_usec % 1000000;
    timer.it_interval.tv_sec = interval_usec / 1000000;
    timer.it_interval.tv_usec = interval_usec % 1000000;
    
    if (setitimer(ITIMER_REAL, &timer, NULL) == -1) {
        tlog(LOG_ERR, "setitimer failed");
        cleanup_gpu_renderer();
        return 1;
    }
    
    tlog(LOG_INFO, "GPU renderer started successfully - press Ctrl+C to exit");
    
    // Main loop
    while (keep_running) {
        pause();
    }
    
    tlog(LOG_INFO, "Shutting down GPU renderer");
    cleanup_gpu_renderer();
    stopEventLoop();
    
    return 0;
}