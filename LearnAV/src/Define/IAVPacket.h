//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#ifndef LEARNAV_IAVPACKET_H
#define LEARNAV_IAVPACKET_H

#include <functional>
#include <memory>

#include "BaseDef.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/rational.h>
}

struct AVPacket;

namespace av {

struct IAVPacket {
    int flags{0};                                          ///<  标志位
    struct AVPacket* avPacket{nullptr};                    ///<  指向FFmpeg AVPacket结构体的指针
    AVRational timeBase{AVRational{0, 0}};                 ///<  时间基数
    std::weak_ptr<std::function<void()>> releaseCallback;  ///<  释放的回调函数

    explicit IAVPacket(struct AVPacket* avPacket) : avPacket(avPacket) {}
    virtual ~IAVPacket() {
        if (avPacket) av_packet_free(&avPacket);  // 释放AVPacket
        if (auto lockedPtr = releaseCallback.lock()) {
            (*lockedPtr)();  // 调用释放回调函数
        }
    }

    float GetTimeStamp() const {
        return avPacket ? avPacket->pts * 1.0f * timeBase.num / timeBase.den : -1.0f;  ///< 获取时间戳
    }
};

}  // namespace av

#endif  // LEARNAV_IAVPACKET_H
