//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#ifndef LEARNAV_IVIDEOPIPELINE_H
#define LEARNAV_IVIDEOPIPELINE_H

#include "Define/IVideoFrame.h"
#include "IGLContext.h"
#include "IVideoFilter.h"

namespace av {

struct IVideoPipeline {
    struct Listener {
        virtual void OnVideoPipelineNotifyVideoFrame(std::shared_ptr<IVideoFrame>) = 0;
        virtual void OnVideoPipelineNotifyFinished() = 0;
        virtual ~Listener() = default;
    };

    virtual std::shared_ptr<IVideoFilter> AddVideoFilter(VideoFilterType type) = 0;
    virtual void RemoveVideoFilter(VideoFilterType type) = 0;

    virtual void SetListener(Listener* listener) = 0;
    virtual void NotifyVideoFrame(std::shared_ptr<IVideoFrame> videoFrame) = 0;
    virtual void NotifyVideoFinished() = 0;
    virtual void Stop() = 0;
    virtual ~IVideoPipeline() = default;

    static IVideoPipeline* Create(std::shared_ptr<GLContext> glContext);
};

}  // namespace av

#endif  // LEARNAV_IVIDEOPIPELINE_H
