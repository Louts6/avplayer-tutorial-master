//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#ifndef LEARNAV_IDEMUXER_H
#define LEARNAV_IDEMUXER_H

#include <string>

#include "Define/IAVPacket.h"

struct AVStream;

namespace av {

struct IDeMuxer {
    struct Listener {
        virtual void OnDeMuxStart() = 0;
        virtual void OnDeMuxStop() = 0;
        virtual void OnDeMuxEOF() = 0;
        virtual void OnDeMuxError(int code, const char* msg) = 0;

        virtual void OnNotifyAudioStream(struct AVStream* stream) = 0;
        virtual void OnNotifyVideoStream(struct AVStream* stream) = 0;

        virtual void OnNotifyAudioPacket(std::shared_ptr<IAVPacket> packet) = 0;
        virtual void OnNotifyVideoPacket(std::shared_ptr<IAVPacket> packet) = 0;

        virtual ~Listener() = default;
    };

    virtual void SetListener(Listener* listener) = 0;

    virtual bool Open(const std::string& url) = 0;
    virtual void Start() = 0;
    virtual void Pause() = 0;
    virtual void SeekTo(float position) = 0;
    virtual void Stop() = 0;

    virtual float GetDuration() = 0;

    virtual ~IDeMuxer() = default;

    static IDeMuxer* Create();
};

}  // namespace av

#endif  // LEARNAV_IDEMUXER_H
