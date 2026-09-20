//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#ifndef LEARNAV_AUDIOPIPELINE_H
#define LEARNAV_AUDIOPIPELINE_H

#include "Interface/IAudioPipeline.h"
#include <mutex>

namespace av {

class AudioPipeline : public IAudioPipeline {
public:
    AudioPipeline(unsigned int channels, unsigned int sampleRate);
    ~AudioPipeline() override = default;

    void SetListener(Listener* listener) override;
    void NotifyAudioSamples(std::shared_ptr<IAudioSamples> audioSamples) override;
    void NotifyAudioFinished() override;

private:
    unsigned int m_channels{2};
    unsigned int m_sampleRate{44100};

    IAudioPipeline::Listener* m_listener{nullptr};
    std::recursive_mutex m_listenerMutex;
};

}  // namespace av

#endif  // LEARNAV_AUDIOPIPELINE_H
