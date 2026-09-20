//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#ifndef LEARNAV_IVIDEOENCODER_H
#define LEARNAV_IVIDEOENCODER_H

#include "Define/FileWriterParameters.h"
#include "Define/IAVPacket.h"
#include "Define/IVideoFrame.h"

namespace av {

struct IVideoEncoder {
    struct Listener {
        virtual void OnVideoEncoderNotifyPacket(std::shared_ptr<IAVPacket>) = 0;
        virtual void OnVideoEncoderNotifyFinished() = 0;
        virtual ~Listener() = default;
    };

    virtual void SetListener(Listener* listener) = 0;
    virtual bool Configure(FileWriterParameters& parameters, int flags) = 0;
    virtual void NotifyVideoFrame(std::shared_ptr<IVideoFrame> videoFrame) = 0;
};

}  // namespace av

#endif  // LEARNAV_IVIDEOENCODER_H
