//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#ifndef LEARNAV_VIDEOPIPELINE_H
#define LEARNAV_VIDEOPIPELINE_H

#include <condition_variable>
#include <list>
#include <mutex>
#include <thread>

#include "Interface/IVideoPipeline.h"
#include "VideoFilter/VideoFilter.h"

namespace av {

class VideoPipeline : public IVideoPipeline {
public:
    explicit VideoPipeline(std::shared_ptr<GLContext> glContext);
    ~VideoPipeline() override;

    std::shared_ptr<IVideoFilter> AddVideoFilter(VideoFilterType type) override;
    void RemoveVideoFilter(VideoFilterType type) override;

    void SetListener(Listener* listener) override;
    void NotifyVideoFrame(std::shared_ptr<IVideoFrame> videoFrame) override;
    void NotifyVideoFinished() override;
    void Stop() override;

private:
    void PrepareVideoFrame(std::shared_ptr<IVideoFrame> videoFrame);
    void PrepareTempTexture(int width, int height);
    void RenderVideoFilter(std::shared_ptr<IVideoFrame> videoFrame);

    void ThreadLoop();

private:
    struct TextureInfo {
        unsigned int id{0};
        int width{0};
        int height{0};
    };

    std::shared_ptr<GLContext> m_sharedGLContext;

    IVideoPipeline::Listener* m_listener{nullptr};
    std::recursive_mutex m_listenerMutex;

    std::list<std::shared_ptr<IVideoFrame>> m_frameQueue;
    std::mutex m_queueMutex;

    std::shared_ptr<VideoFilter> m_flipVerticalFilter;  // 垂直翻转滤镜

    std::list<std::shared_ptr<VideoFilter>> m_videoFilters;
    std::list<std::shared_ptr<VideoFilter>> m_removedVideoFilters;
    std::mutex m_videoFilterMutex;
    TextureInfo m_tempTexture;

    std::condition_variable m_queueCondVar;
    std::shared_ptr<std::thread> m_thread;
    bool m_abort{false};
};

}  // namespace av

#endif  // LEARNAV_VIDEOPIPELINE_H
