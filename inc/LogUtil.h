#ifndef LOGUTIL_H
#define LOGUTIL_H

#include <android/log.h>
#include <sys/time.h>

#define  LOG_TAG "RendererClient"

#define  LOG_E(...)  __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define  LOG_V(...)  __android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__)
#define  LOG_I(...)  __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define  LOG_D(...)  __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

#define FUN_BEGIN_TIME(FUN)\
    LOG_D("%s:%s func start", __FILE__, FUN); \
    long long t0 = GetSysCurrentTime();

#define FUN_END_TIME(FUN) \
    long long t1 = GetSysCurrentTime(); \
    LOG_D("%s:%s func cost time %ldms", __FILE__, FUN, (long)(t1-t0));

#define BEGIN_TIME(FUN)\
    LOG_D("%s func start", FUN); \
    long long t0 = GetSysCurrentTime();

#define END_TIME(FUN) \
    long long t1 = GetSysCurrentTime(); \
    LOG_D("%s func cost time %ldms", FUN, (long)(t1-t0));

static long long GetSysCurrentTime()
{
	struct timeval time;
	gettimeofday(&time, NULL);
	long long curTime = ((long long)(time.tv_sec))*1000+time.tv_usec/1000;
	return curTime;
}

static float getTimeSec(clockid_t clk_id = CLOCK_BOOTTIME){
    struct timespec ts;
    clock_gettime(clk_id, &ts);
    return (float)ts.tv_sec + (float)ts.tv_nsec * 1e-9;
}

#define DEBUG_LOG(...) LOG_D("%s:%d",  __FUNCTION__, __LINE__)

#endif //LOGUTIL_H