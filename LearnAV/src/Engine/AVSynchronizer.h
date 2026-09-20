//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#ifndef LEARNAV_AVSYNCHRONIZER_H
#define LEARNAV_AVSYNCHRONIZER_H

#include <list>
#include <thread>

#include "Core/SyncNotifier.h"
#include "Define/IAudioSamples.h"
#include "Define/IVideoFrame.h"
#include "IGLContext.h"

namespace av {

class AVSynchronizer {
public:
    struct Listener {
        virtual void OnAVSynchronizerNotifyAudioSamples(std::shared_ptr<IAudioSamples>) = 0;
        virtual void OnAVSynchronizerNotifyVideoFrame(std::shared_ptr<IVideoFrame>) = 0;
        virtual void OnAVSynchronizerNotifyAudioFinished() = 0;
        virtual void OnAVSynchronizerNotifyVideoFinished() = 0;
        virtual ~Listener() = default;
    };

    explicit AVSynchronizer(std::shared_ptr<GLContext> glContext);
    ~AVSynchronizer();

    void SetListener(Listener* listener);
    void Start();
    void Stop();
    void Reset();

    void NotifyAudioSamples(std::shared_ptr<IAudioSamples> audioSamples);
    void NotifyVideoFrame(std::shared_ptr<IVideoFrame> videoFrame);

    void NotifyAudioFinished();
    void NotifyVideoFinished();

private:
    void ThreadLoop();
    void Synchronize();

private:
    struct StreamInfo {
        float currentTimeStamp{0.0f};
        bool isFinished{false};
        void Reset() {
            currentTimeStamp = 0.0f;
            isFinished = false;
        }
    };

    const double syncThreshold = 0.05;  // 50 ms threshold for synchronization
    std::shared_ptr<GLContext> m_glContext;

    SyncNotifier m_notifier;

    std::mutex m_mutex;
    std::list<std::shared_ptr<IAudioSamples>> m_audioQueue;
    std::list<std::shared_ptr<IVideoFrame>> m_videoQueue;

    StreamInfo m_audioStreamInfo;
    StreamInfo m_videoStreamInfo;

    Listener* m_listener{nullptr};
    std::mutex m_listenerMutex;

    std::thread m_syncThread;
    std::atomic<bool> m_abort{false};
    std::atomic<bool> m_reset{false};
};

}  // namespace av

#endif  // LEARNAV_AVSYNCHRONIZER_H
