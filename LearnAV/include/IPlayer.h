//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#ifndef LEARNAV_IPLAYER_H
#define LEARNAV_IPLAYER_H

#include <memory>

#include "IGLContext.h"
#include "IPlaybackListener.h"
#include "IVideoFilter.h"

namespace av {

struct IVideoDisplayView;

struct IPlayer {
    virtual void AttachDisplayView(std::shared_ptr<IVideoDisplayView> displayView) = 0;
    virtual void DetachDisplayView(std::shared_ptr<IVideoDisplayView> displayView) = 0;

    virtual void SetPlaybackListener(std::shared_ptr<IPlaybackListener> listener) = 0;

    virtual bool Open(std::string &filePath) = 0;
    virtual void Play() = 0;
    virtual void Pause() = 0;
    virtual void SeekTo(float progress) = 0;
    virtual bool IsPlaying() = 0;

    virtual std::shared_ptr<IVideoFilter> AddVideoFilter(VideoFilterType type) = 0;
    virtual void RemoveVideoFilter(VideoFilterType type) = 0;

    virtual bool StartRecording(const std::string &outputFilePath, int flags) = 0;
    virtual void StopRecording() = 0;
    virtual bool IsRecording() = 0;

    // 设置是否使用 GPU 编解码（默认 false，即 CPU 编解码）
    // 注意：切换后需重新 Open 视频 / 重新开始录制才会生效
    virtual void SetUseGpu(bool useGpu) = 0;
    virtual bool IsUsingGpu() = 0;

    virtual ~IPlayer() = default;

    static IPlayer *Create(std::shared_ptr<GLContext> glContext);
};

}  // namespace av

#endif  // LEARNAV_IPLAYER_H
