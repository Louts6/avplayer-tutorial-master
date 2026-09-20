//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#ifndef LEARNAV_AUDIODECODER_H
#define LEARNAV_AUDIODECODER_H

#include <condition_variable>
#include <list>
#include <mutex>
#include <thread>

#include "Core/SyncNotifier.h"
#include "Interface/IAudioDecoder.h"
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
}

namespace av {

class AudioDecoder : public IAudioDecoder {
public:
    AudioDecoder(unsigned int channels, unsigned int sampleRate);
    ~AudioDecoder() override;

    void SetListener(Listener* listener) override;
    void SetStream(struct AVStream* stream) override;
    void Decode(std::shared_ptr<IAVPacket> packet) override;
    void Start() override;
    void Pause() override;
    void Stop() override;

private:
    void ThreadLoop();
    void CheckFlushPacket();
    void DecodeAVPacket();
    void ReleaseAudioPipelineResource();
    void CleanupContext();

private:
    unsigned int m_targetChannels;
    unsigned int m_targetSampleRate;

    // 数据回调
    IAudioDecoder::Listener* m_listener{nullptr};
    std::recursive_mutex m_listenerMutex;

    // 解码 & 重采样
    std::mutex m_codecContextMutex;
    AVCodecContext* m_codecContext{nullptr};  // 解码器上下文
    SwrContext* m_swrContext{nullptr};        // 重采样上下文
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

#endif  // LEARNAV_AUDIODECODER_H
