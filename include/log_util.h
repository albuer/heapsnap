#include <android/log.h>

#ifndef __LOG_UTIL_H__
#define __LOG_UTIL_H__

#if 0
#define LOG_TAG "INJECT"
#define LOGD(fmt, args...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, fmt, ##args)
#define LOGE(fmt, args...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, fmt, ##args)
#define LOGI(fmt, args...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, fmt, ##args)
#define LOGW(fmt, args...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, fmt, ##args)
#else
#define LOGD(fmt, args...) fprintf(stdout, fmt, ##args)
#define LOGE(fmt, args...) fprintf(stderr, fmt, ##args)
#define LOGI(fmt, args...) fprintf(stdout, fmt, ##args)
#define LOGW(fmt, args...) fprintf(stderr, fmt, ##args)
#endif

#endif
