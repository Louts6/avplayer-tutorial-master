//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#ifndef LEARNAV_IVIDEODECODER_H
#define LEARNAV_IVIDEODECODER_H

#include "Define/IAVPacket.h"
#include "Define/IVideoFrame.h"

struct AVStream;

namespace av {

struct IVideoDecoder {
    struct Listener {
        virtual void OnNotifyVideoFrame(std::shared_ptr<IVideoFrame>) = 0;
        virtual ~Listener() = default;
    };

    virtual void SetListener(Listener* listener) = 0;
    virtual void SetStream(struct AVStream* stream) = 0;
    virtual void Decode(std::shared_ptr<IAVPacket> packet) = 0;
    virtual void Start() = 0;
    virtual void Pause() = 0;
    virtual void Stop() = 0;

    virtual int GetVideoWidth() = 0;
    virtual int GetVideoHeight() = 0;

    virtual ~IVideoDecoder() = default;

    static IVideoDecoder* Create();
};

}  // namespace av

#endif  // LEARNAV_IVIDEODECODER_H
