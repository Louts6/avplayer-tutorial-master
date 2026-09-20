//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#ifndef LEARNAV_VIDEODECODER_H
#define LEARNAV_VIDEODECODER_H

#include <list>
#include <mutex>
#include <thread>

#include "Core/SyncNotifier.h"
#include "Define/IAVPacket.h"
#include "Interface/IVideoDecoder.h"
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

namespace av {

class VideoDecoder : public IVideoDecoder {
public:
    VideoDecoder();
    ~VideoDecoder() override;

    void SetListener(Listener* listener) override;
    void SetStream(struct AVStream* stream) override;
    void Decode(std::shared_ptr<IAVPacket> packet) override;
    void Start() override;
    void Pause() override;
    void Stop() override;

    int GetVideoWidth() override;
    int GetVideoHeight() override;

private:
    void ThreadLoop();
    void CheckFlushPacket();
    void DecodeAVPacket();
    void ReleaseVideoPipelineResource();
    void CleanupContext();

private:
    // 数据回调
    IVideoDecoder::Listener* m_listener{nullptr};
    std::recursive_mutex m_listenerMutex;

    // 解码 & 缩放
    std::mutex m_codecContextMutex;
    AVCodecContext* m_codecContext{nullptr};  // 解码器上下文
    SwsContext* m_swsContext{nullptr};        // 缩放上下文
    AVRational m_timeBase{AVRational{1, 1}};

    // AVPacket 队列
    std::list<std::shared_ptr<IAVPacket>> m_packetQueue;
    std::mutex m_packetQueueMutex;

    SyncNotifier m_notifier;

    std::thread m_thread;
    std::atomic<bool> m_paused{true};
    std::atomic<bool> m_abort{false};

    std::atomic<int> m_pipelineResourceCount{3};
    std::shared_ptr<std::function<void()>> m_pipelineReleaseCallback;
};

}  // namespace av

#endif  // LEARNAV_VIDEODECODER_H
