//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#ifndef LEARNAV_ANDROIDAUDIOSPEAKER_H
#define LEARNAV_ANDROIDAUDIOSPEAKER_H

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

#include <deque>
#include <memory>
#include <mutex>
#include <thread>

#include "Interface/IAudioSpeaker.h"

namespace av {

class AndroidAudioSpeaker : public IAudioSpeaker {
public:
    AndroidAudioSpeaker(unsigned int channels, unsigned int sampleRate);
    ~AndroidAudioSpeaker() override;

    void PlayAudioSamples(std::shared_ptr<IAudioSamples> samples) override;
    void Stop() override;

private:
    void InitOpenSLES();
    void DestroyOpenSLES();
    void ProcessAudioQueue();

    static void PlayerCallback(SLAndroidSimpleBufferQueueItf bq, void* context);

    unsigned int m_channels;
    unsigned int m_sampleRate;
    bool m_isPlaying;

    // OpenSL ES objects
    SLObjectItf m_engineObject{nullptr};
    SLEngineItf m_engine{nullptr};
    SLObjectItf m_outputMixObject{nullptr};
    SLObjectItf m_playerObject{nullptr};
    SLPlayItf m_player{nullptr};
    SLAndroidSimpleBufferQueueItf m_bufferQueue{nullptr};

    // Audio queue (deque allows push_front on Enqueue failure to avoid dropping)
    std::mutex m_mutex;
    std::deque<std::shared_ptr<IAudioSamples>> m_audioQueue;
    std::shared_ptr<IAudioSamples> m_currentBuffer;

    /// Number of buffers currently enqueued in OpenSL (max kOpenSLQueueDepth). Used to pace feeding.
    int m_buffersInFlight{0};

    static constexpr int kOpenSLQueueDepth = 2;  // must match SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE
    static constexpr int kMaxQueueSize = 10;
};

}  // namespace av

#endif  // LEARNAV_ANDROIDAUDIOSPEAKER_H
