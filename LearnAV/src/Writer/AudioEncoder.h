//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#ifndef LEARNAV_AUDIOENCODER_H
#define LEARNAV_AUDIOENCODER_H

#include <atomic>
#include <condition_variable>
#include <list>
#include <memory>
#include <mutex>
#include <thread>

#include "Interface/IAudioEncoder.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
}

namespace av {

class AudioEncoder : public IAudioEncoder {
public:
    AudioEncoder();
    ~AudioEncoder();

    // 继承自 IAudioEncoder
    void SetListener(Listener* listener) override;
    bool Configure(FileWriterParameters& parameters, int flags) override;
    void NotifyAudioSamples(std::shared_ptr<IAudioSamples> audioSamples) override;

private:
    void ThreadLoop();
    void StopThread();
    void PrepareEncodeAudioSamples(std::shared_ptr<IAudioSamples> audioSamples);
    void EncodeAudioSamples(const AVFrame* frame);

private:
    Listener* m_listener{nullptr};
    std::mutex m_listenerMutex;

    // AVPacket 队列
    std::list<std::shared_ptr<IAudioSamples>> m_audioSamplesQueue;
    std::condition_variable m_cond;
    std::mutex m_mutex;

    std::thread m_thread;
    std::atomic<bool> m_abort{false};

    // 编码 & 重采样
    AVCodecContext* m_encodeCtx{nullptr};
    SwrContext* m_swrContext{nullptr};

    // AVPacket & AVFrame
    AVFrame* m_avFrame{nullptr};
    AVPacket* m_avPacket{nullptr};

    int64_t m_pts{0};
};

}  // namespace av

#endif  // LEARNAV_AUDIOENCODER_H
