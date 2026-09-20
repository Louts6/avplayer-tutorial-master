//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#ifndef LEARNAV_ANDROIDDISPLAYVIEW_H
#define LEARNAV_ANDROIDDISPLAYVIEW_H

#include <GLES3/gl3.h>

#include <memory>
#include <mutex>

#include "Define/IVideoFrame.h"
#include "Interface/IVideoDisplayView.h"

namespace av {

class TaskPool;

class AndroidDisplayView : public IVideoDisplayView {
public:
    AndroidDisplayView();
    ~AndroidDisplayView() override;

    void SetTaskPool(std::shared_ptr<TaskPool> taskPool) override;
    void InitializeGL() override;
    void SetDisplaySize(int width, int height) override;
    void Render(std::shared_ptr<IVideoFrame> videoFrame, EContentMode mode) override;
    void Render(int width, int height, float red, float green, float blue) override;
    void Clear() override;

    // Android-specific methods
    void SetSurfaceSize(int width, int height);
    void OnDrawFrame();

private:
    GLuint m_shaderProgram{0};
    GLint m_uTexture1Location{-1};
    GLuint m_VAO{0};
    GLuint m_VBO{0};
    GLuint m_EBO{0};

    std::shared_ptr<IVideoFrame> m_videoFrame;
    std::mutex m_videoFrameMutex;
    EContentMode m_mode{EContentMode::kScaleAspectFit};

    std::shared_ptr<TaskPool> m_taskPool;

    // Store surface dimensions
    int m_surfaceWidth{0};
    int m_surfaceHeight{0};
};

}  // namespace av

#endif  // LEARNAV_ANDROIDDISPLAYVIEW_H
