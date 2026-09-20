//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#ifndef LEARNAV_DEMUXER_H
#define LEARNAV_DEMUXER_H

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "Core/SyncNotifier.h"
#include "Interface/IDeMuxer.h"
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

namespace av {

class DeMuxer : public IDeMuxer {
public:
    DeMuxer();
    ~DeMuxer() override;

    void SetListener(IDeMuxer::Listener* listener) override;

    bool Open(const std::string& url) override;
    void Start() override;
    void Pause() override;
    void SeekTo(float progress) override;
    void Stop() override;

    float GetDuration() override;

private:
    void ThreadLoop();
    void ProcessSeek();
    bool ReadAndSendPacket();

    void ReleaseAudioPipelineResource();
    void ReleaseVideoPipelineResource();

    struct StreamInfo {
        int streamIndex{-1};
        std::atomic<int> pipelineResourceCount{3};
        std::shared_ptr<std::function<void()>> pipelineReleaseCallback;
    };

private:
    IDeMuxer::Listener* m_listener{nullptr};
    std::recursive_mutex m_listenerMutex;
    std::thread m_thread;
    std::atomic<bool> m_paused{true};
    std::atomic<bool> m_abort{false};
    std::atomic<bool> m_seek{false};
    SyncNotifier m_notifier;

    AVFormatContext* m_formatCtx{nullptr};
    std::mutex m_formatMutex;
    StreamInfo m_audioStream;
    StreamInfo m_videoStream;

    float m_seekProgress{-1.0f};
};

}  // namespace av

#endif  // LEARNAV_DEMUXER_H
