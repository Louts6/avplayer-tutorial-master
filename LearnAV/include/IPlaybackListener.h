//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#ifndef LEARNAV_IPLAYBACKLISTENER_H
#define LEARNAV_IPLAYBACKLISTENER_H

#include <string>

namespace av {
struct IPlaybackListener {
    virtual void NotifyPlaybackStarted() = 0;
    virtual void NotifyPlaybackTimeChanged(float timeStamp, float duration) = 0;
    virtual void NotifyPlaybackPaused() = 0;
    virtual void NotifyPlaybackEOF() = 0;

    virtual ~IPlaybackListener() = default;
};
};  // namespace av

#endif  // LEARNAV_IPLAYBACKLISTENER_H
