//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#ifndef LEARNAV_IAUDIOSPEAKER_H
#define LEARNAV_IAUDIOSPEAKER_H

#include <memory>

#include "Define/IAudioSamples.h"

namespace av {

struct IAudioSpeaker {
    virtual void PlayAudioSamples(std::shared_ptr<IAudioSamples> samples) = 0;
    virtual void Stop() = 0;
    virtual ~IAudioSpeaker() = default;

    static IAudioSpeaker* Create(unsigned int channels, unsigned int sampleRate);
};

}  // namespace av

#endif  // LEARNAV_IAUDIOSPEAKER_H
