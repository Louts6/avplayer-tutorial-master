//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#ifndef LEARNAV_IVIDEOFRAME_H
#define LEARNAV_IVIDEOFRAME_H

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

#include "Log.h"
#ifdef ANDROID
#include <GLES3/gl3.h>
#endif

#include "BaseDef.h"
#include "IGLContext.h"

namespace av {
struct IVideoFrame {
    int flags{0};                   ///<  标志位
    unsigned int width{0};          ///< 视频帧的宽度
    unsigned int height{0};         ///< 视频帧的高度
    int64_t pts{0};                 ///< 显示时间戳
    int64_t duration{0};            ///< 持续时间
    int32_t timebaseNum{1};         ///< 时间基数的分子
    int32_t timebaseDen{1};         ///< 时间基数的分母
    std::shared_ptr<uint8_t> data;  ///< RGBA 数据（CPU 路径），为了简化代码，统一使用 RGBA 数据
    unsigned int textureId{0};      ///< OpenGL 纹理 ID

    /// GPU 硬件帧（AV_PIX_FMT_CUDA 的 AVFrame*，自定义删除器）。非空时 data 为空，
    /// 由 VideoPipeline 通过 CUDA-GL 互操作直接上屏，避免 GPU→CPU→GPU 往返拷贝。
    std::shared_ptr<void> hardwareFrame;
    int pixelFormat{0};  ///< 0 表示 CPU RGBA；否则为 AV_PIX_FMT_* 像素格式

    bool IsHardwareFrame() const { return hardwareFrame != nullptr; }

    std::weak_ptr<std::function<void()>> releaseCallback;  ///< 释放的回调函数

    float GetTimeStamp() const { return pts * 1.0f * timebaseNum / timebaseDen; }  ///< 获取时间戳
    virtual ~IVideoFrame() {
        if (textureId > 0) glDeleteTextures(1, &textureId);
        hardwareFrame.reset();  // 先归还硬件帧（解码器 surface pool），再释放管线配额
        if (auto lockedPtr = releaseCallback.lock()) {
            (*lockedPtr)();
        }
    }
};

}  // namespace av

#endif  // LEARNAV_IVIDEOFRAME_H
