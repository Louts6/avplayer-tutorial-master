//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#include "AndroidAudioSpeaker.h"

#include "Log.h"

static const char* kAndroidAudioSpeakerTag = "AndroidAudioSpeaker";

namespace av {

AndroidAudioSpeaker::AndroidAudioSpeaker(unsigned int channels, unsigned int sampleRate)
    : m_channels(channels), m_sampleRate(sampleRate), m_isPlaying(false) {
    InitOpenSLES();
}

AndroidAudioSpeaker::~AndroidAudioSpeaker() {
    Stop();
    DestroyOpenSLES();
}

void AndroidAudioSpeaker::InitOpenSLES() {
    SLresult result;

    // Create engine
    result = slCreateEngine(&m_engineObject, 0, nullptr, 0, nullptr, nullptr);
    if (result != SL_RESULT_SUCCESS) {
        LOGE(kAndroidAudioSpeakerTag, "Failed to create engine");
        return;
    }

    result = (*m_engineObject)->Realize(m_engineObject, SL_BOOLEAN_FALSE);
    if (result != SL_RESULT_SUCCESS) {
        LOGE(kAndroidAudioSpeakerTag, "Failed to realize engine");
        return;
    }

    result = (*m_engineObject)->GetInterface(m_engineObject, SL_IID_ENGINE, &m_engine);
    if (result != SL_RESULT_SUCCESS) {
        LOGE(kAndroidAudioSpeakerTag, "Failed to get engine interface");
        return;
    }

    // Create output mix
    result = (*m_engine)->CreateOutputMix(m_engine, &m_outputMixObject, 0, nullptr, nullptr);
    if (result != SL_RESULT_SUCCESS) {
        LOGE(kAndroidAudioSpeakerTag, "Failed to create output mix");
        return;
    }

    result = (*m_outputMixObject)->Realize(m_outputMixObject, SL_BOOLEAN_FALSE);
    if (result != SL_RESULT_SUCCESS) {
        LOGE(kAndroidAudioSpeakerTag, "Failed to realize output mix");
        return;
    }

    // Configure audio source
    SLDataLocator_AndroidSimpleBufferQueue locatorBufferQueue = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2};

    SLDataFormat_PCM formatPCM = {
        SL_DATAFORMAT_PCM,
        m_channels,
        m_sampleRate * 1000,  // milli Hz
        SL_PCMSAMPLEFORMAT_FIXED_16,
        SL_PCMSAMPLEFORMAT_FIXED_16,
        m_channels == 1 ? SL_SPEAKER_FRONT_CENTER : (SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT),
        SL_BYTEORDER_LITTLEENDIAN};

    SLDataSource audioSource = {&locatorBufferQueue, &formatPCM};

    // Configure audio sink
    SLDataLocator_OutputMix locatorOutputMix = {SL_DATALOCATOR_OUTPUTMIX, m_outputMixObject};
    SLDataSink audioSink = {&locatorOutputMix, nullptr};

    // Create audio player (request ANDROIDCONFIGURATION so we can set stream type before Realize)
    const SLInterfaceID ids[] = {SL_IID_BUFFERQUEUE, SL_IID_ANDROIDCONFIGURATION};
    const SLboolean req[] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};
    result = (*m_engine)->CreateAudioPlayer(m_engine, &m_playerObject, &audioSource, &audioSink, 2, ids, req);
    if (result != SL_RESULT_SUCCESS) {
        LOGE(kAndroidAudioSpeakerTag, "Failed to create audio player");
        return;
    }

    SLAndroidConfigurationItf playerConfig = nullptr;
    result = (*m_playerObject)->GetInterface(m_playerObject, SL_IID_ANDROIDCONFIGURATION, &playerConfig);
    if (result == SL_RESULT_SUCCESS && playerConfig) {
        SLint32 streamType = SL_ANDROID_STREAM_MEDIA;
        result = (*playerConfig)
                     ->SetConfiguration(playerConfig, SL_ANDROID_KEY_STREAM_TYPE, &streamType, sizeof(streamType));
        if (result != SL_RESULT_SUCCESS) {
            LOGI(kAndroidAudioSpeakerTag, "SetConfiguration STREAM_TYPE failed (non-fatal)");
        }
    }

    result = (*m_playerObject)->Realize(m_playerObject, SL_BOOLEAN_FALSE);
    if (result != SL_RESULT_SUCCESS) {
        LOGE(kAndroidAudioSpeakerTag, "Failed to realize audio player");
        return;
    }

    // Get player interface
    result = (*m_playerObject)->GetInterface(m_playerObject, SL_IID_PLAY, &m_player);
    if (result != SL_RESULT_SUCCESS) {
        LOGE(kAndroidAudioSpeakerTag, "Failed to get player interface");
        return;
    }

    // Get buffer queue interface
    result = (*m_playerObject)->GetInterface(m_playerObject, SL_IID_BUFFERQUEUE, &m_bufferQueue);
    if (result != SL_RESULT_SUCCESS) {
        LOGE(kAndroidAudioSpeakerTag, "Failed to get buffer queue interface");
        return;
    }

    // Register callback
    result = (*m_bufferQueue)->RegisterCallback(m_bufferQueue, PlayerCallback, this);
    if (result != SL_RESULT_SUCCESS) {
        LOGE(kAndroidAudioSpeakerTag, "Failed to register callback");
        return;
    }

    LOGI(kAndroidAudioSpeakerTag, "OpenSL ES initialized successfully");
}

void AndroidAudioSpeaker::DestroyOpenSLES() {
    if (m_playerObject) {
        (*m_playerObject)->Destroy(m_playerObject);
        m_playerObject = nullptr;
        m_player = nullptr;
        m_bufferQueue = nullptr;
    }

    if (m_outputMixObject) {
        (*m_outputMixObject)->Destroy(m_outputMixObject);
        m_outputMixObject = nullptr;
    }

    if (m_engineObject) {
        (*m_engineObject)->Destroy(m_engineObject);
        m_engineObject = nullptr;
        m_engine = nullptr;
    }
}

void AndroidAudioSpeaker::PlayAudioSamples(std::shared_ptr<IAudioSamples> samples) {
    if (!samples || !m_player || !m_bufferQueue) {
        return;
    }

    bool needStartAndProcess = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_audioQueue.size() >= kMaxQueueSize) {
            LOGI(kAndroidAudioSpeakerTag, "Audio queue full, dropping oldest sample");
            m_audioQueue.pop_front();
        }

        m_audioQueue.push_back(samples);

        if (!m_isPlaying) {
            m_isPlaying = true;
            needStartAndProcess = true;
        }
    }
    if (needStartAndProcess) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_buffersInFlight = 0;
        }
        (*m_bufferQueue)->Clear(m_bufferQueue);
        ProcessAudioQueue();
        (*m_player)->SetPlayState(m_player, SL_PLAYSTATE_PLAYING);
    } else {
        // Only feed when OpenSL has room; otherwise callback will feed when a buffer finishes.
        bool hasRoom = false;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            hasRoom = (m_buffersInFlight < kOpenSLQueueDepth);
        }
        if (hasRoom) {
            ProcessAudioQueue();
        }
    }
}

void AndroidAudioSpeaker::ProcessAudioQueue() {
    std::shared_ptr<IAudioSamples> toPlay;
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (!m_bufferQueue) {
            return;
        }
        if (m_buffersInFlight >= kOpenSLQueueDepth) {
            return;  // Pace: don't enqueue more than OpenSL can hold; callback will feed when one finishes
        }
        while (!m_audioQueue.empty() && m_audioQueue.front()->pcmData.empty()) {
            m_audioQueue.pop_front();
        }
        if (m_audioQueue.empty()) {
            return;
        }

        toPlay = m_audioQueue.front();
        m_audioQueue.pop_front();
        m_currentBuffer = toPlay;
        m_buffersInFlight++;
    }

    const SLuint32 numBytes = static_cast<SLuint32>(toPlay->pcmData.size() * sizeof(int16_t));
    if (numBytes == 0) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_currentBuffer.reset();
        m_buffersInFlight--;
        return;
    }

    SLresult result = (*m_bufferQueue)->Enqueue(m_bufferQueue, toPlay->pcmData.data(), numBytes);
    if (result != SL_RESULT_SUCCESS) {
        LOGE(kAndroidAudioSpeakerTag, "Failed to enqueue buffer: %u", static_cast<unsigned>(result));
        std::lock_guard<std::mutex> lock(m_mutex);
        m_buffersInFlight--;
        m_audioQueue.push_front(toPlay);  // Put back so we don't drop the sample
        m_currentBuffer.reset();
    }
}

void AndroidAudioSpeaker::PlayerCallback(SLAndroidSimpleBufferQueueItf bq, void* context) {
    auto* speaker = static_cast<AndroidAudioSpeaker*>(context);
    if (speaker) {
        {
            std::lock_guard<std::mutex> lock(speaker->m_mutex);
            speaker->m_currentBuffer.reset();
            speaker->m_buffersInFlight--;  // One slot free; ProcessAudioQueue can enqueue one more
        }
        speaker->ProcessAudioQueue();
    }
}

void AndroidAudioSpeaker::Stop() {
    if (m_player) {
        (*m_player)->SetPlayState(m_player, SL_PLAYSTATE_STOPPED);
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_isPlaying = false;
    m_buffersInFlight = 0;
    m_currentBuffer.reset();
    m_audioQueue.clear();
}

}  // namespace av
