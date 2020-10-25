#pragma once

#ifdef ANDROID
#include <android/log.h>
#define ACCELERATED_ARRAYS_LOG_TAG "accelerated-arrays"
#define log_debug(...) ((void)__android_log_print(ANDROID_LOG_DEBUG, ACCELERATED_ARRAYS_LOG_TAG, __VA_ARGS__))
#define log_info(...) ((void)__android_log_print(ANDROID_LOG_INFO, ACCELERATED_ARRAYS_LOG_TAG, __VA_ARGS__))
#define log_warn(...) ((void)__android_log_print(ANDROID_LOG_WARN, ACCELERATED_ARRAYS_LOG_TAG, __VA_ARGS__))
#define log_error(...) ((void)__android_log_print(ANDROID_LOG_ERROR, ACCELERATED_ARRAYS_LOG_TAG, __VA_ARGS__))
#else
// logging
#ifndef log_error
#include <cstdio>
// ## is a "gcc" hack for allowing empty __VA_ARGS__
// https://stackoverflow.com/questions/5891221/variadic-macros-with-zero-arguments
#define log_debug(...) do { std::printf("DEBUG: "); std::printf(__VA_ARGS__); std::printf("\n"); } while (0)
#define log_info(...) do { std::printf("INFO: "); std::printf(__VA_ARGS__); std::printf("\n"); } while (0)
#define log_warn(...) do { std::fprintf(stderr, "WARN: "); std::fprintf(stderr, __VA_ARGS__); std::fprintf(stderr, "\n"); } while (0)
#define log_error(...) do { std::fprintf(stderr, "ERROR: "); std::fprintf(stderr, __VA_ARGS__); std::fprintf(stderr, "\n"); } while (0)
#endif
#endif

//#define ACCELERATED_ARRAYS_LOG_TRACE
#ifdef ACCELERATED_ARRAYS_LOG_TRACE
#define LOG_TRACE(...) log_debug(__FILE__ ": " __VA_ARGS__)
#else
#define LOG_TRACE(...) (void)0
#endif
