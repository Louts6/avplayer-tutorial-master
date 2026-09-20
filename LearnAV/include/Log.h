//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#ifndef LEARNAV_LOG_H
#define LEARNAV_LOG_H

#ifdef ANDROID
#include <android/log.h>

#define LOGD(TAG, FMT, ...) __android_log_print(ANDROID_LOG_DEBUG, TAG, FMT, ##__VA_ARGS__)
#define LOGI(TAG, FMT, ...) __android_log_print(ANDROID_LOG_INFO, TAG, FMT, ##__VA_ARGS__)
#define LOGW(TAG, FMT, ...) __android_log_print(ANDROID_LOG_WARN, TAG, FMT, ##__VA_ARGS__)
#define LOGE(TAG, FMT, ...) __android_log_print(ANDROID_LOG_ERROR, TAG, FMT, ##__VA_ARGS__)

#else
#include <cstdio>
#include <iostream>

#define AVM_LOG_TO_STREAM(stream, level, tag, fmt, ...)                       \
    do {                                                                      \
        char _avm_log_buf[1024];                                              \
        snprintf(_avm_log_buf, sizeof(_avm_log_buf), fmt, ##__VA_ARGS__);     \
        (stream) << level << " " << tag << ": " << _avm_log_buf << std::endl; \
    } while (0)

#define LOGD(TAG, FMT, ...) AVM_LOG_TO_STREAM(std::cout, "D", TAG, FMT, ##__VA_ARGS__)
#define LOGI(TAG, FMT, ...) AVM_LOG_TO_STREAM(std::cout, "I", TAG, FMT, ##__VA_ARGS__)
#define LOGW(TAG, FMT, ...) AVM_LOG_TO_STREAM(std::cerr, "W", TAG, FMT, ##__VA_ARGS__)
#define LOGE(TAG, FMT, ...) AVM_LOG_TO_STREAM(std::cerr, "E", TAG, FMT, ##__VA_ARGS__)

#endif  // ANDROID

#endif  // LEARNAV_LOG_H
