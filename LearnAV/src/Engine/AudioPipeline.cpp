//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#include "AudioPipeline.h"

namespace av {
IAudioPipeline* IAudioPipeline::Create(unsigned int channels, unsigned int sampleRate) {
    return new AudioPipeline(channels, sampleRate);
}

AudioPipeline::AudioPipeline(unsigned int channels, unsigned int sampleRate)
    : m_channels(channels), m_sampleRate(sampleRate) {}

void AudioPipeline::SetListener(Listener* listener) {
    std::lock_guard<std::recursive_mutex> lock(m_listenerMutex);
    m_listener = listener;
}

void AudioPipeline::NotifyAudioSamples(std::shared_ptr<IAudioSamples> audioSamples) {
    // 此处可以添加音频处理逻辑，比如音频增益、混音等
    std::lock_guard<std::recursive_mutex> lock(m_listenerMutex);
    if (m_listener) m_listener->OnAudioPipelineNotifyAudioSamples(audioSamples);
}

void AudioPipeline::NotifyAudioFinished() {
    std::lock_guard<std::recursive_mutex> lock(m_listenerMutex);
    if (m_listener) m_listener->OnAudioPipelineNotifyFinished();
}

}  // namespace av