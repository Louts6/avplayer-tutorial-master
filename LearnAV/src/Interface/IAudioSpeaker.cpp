//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#include "Interface/IAudioSpeaker.h"

#ifdef ANDROID
#include "Android/AndroidAudioSpeaker.h"
namespace av {
IAudioSpeaker* IAudioSpeaker::Create(unsigned int channels, unsigned int sampleRate) {
    return new AndroidAudioSpeaker(channels, sampleRate);
}
}  // namespace av
#else
#include "Engine/AudioSpeaker.h"
namespace av {
IAudioSpeaker* IAudioSpeaker::Create(unsigned int channels, unsigned int sampleRate) {
    return new AudioSpeaker(channels, sampleRate);
}
}  // namespace av
#endif
