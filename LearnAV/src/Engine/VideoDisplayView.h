//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#ifndef LEARNAV_VIDEODISPLAYVIEW_H
#define LEARNAV_VIDEODISPLAYVIEW_H

#include <mutex>

#include "Interface/IVideoDisplayView.h"

namespace av {

class VideoDisplayView : public IVideoDisplayView {
public:
    VideoDisplayView() = default;
    ~VideoDisplayView();

    void SetTaskPool(std::shared_ptr<TaskPool> taskPool) override;
    void InitializeGL() override;
    void SetDisplaySize(int width, int height) override;
    void Render(std::shared_ptr<IVideoFrame> videoFrame, EContentMode mode) override;
    void Render(int width, int height, float red, float green, float blue) override;
    void Clear() override;

private:
    std::shared_ptr<TaskPool> m_taskPool;

    std::shared_ptr<IVideoFrame> m_videoFrame;
    std::mutex m_videoFrameMutex;
    EContentMode m_mode;

    unsigned int m_shaderProgram{0};
    unsigned int m_VAO{0};
    unsigned int m_VBO{0};
    unsigned int m_EBO{0};
};

}  // namespace av

#endif  // LEARNAV_VIDEODISPLAYVIEW_H
