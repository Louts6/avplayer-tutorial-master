//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#include "VideoPipeline.h"

#include "Log.h"
#include "Utils/GLUtils.h"

static const char* kVideoPipelineTag = "VideoPipeline";

namespace av {

IVideoPipeline* IVideoPipeline::Create(std::shared_ptr<GLContext> glContext) { return new VideoPipeline(glContext); }

VideoPipeline::VideoPipeline(std::shared_ptr<GLContext> glContext)
    : m_sharedGLContext(glContext), m_thread(std::make_shared<std::thread>(&VideoPipeline::ThreadLoop, this)) {}

VideoPipeline::~VideoPipeline() { Stop(); }

void VideoPipeline::SetListener(Listener* listener) {
    std::lock_guard<std::recursive_mutex> lock(m_listenerMutex);
    m_listener = listener;
}

std::shared_ptr<IVideoFilter> VideoPipeline::AddVideoFilter(VideoFilterType type) {
    std::lock_guard<std::mutex> lock(m_videoFilterMutex);
    // 如果已经存在相同类型的滤镜，则不再添加
    for (auto& filter : m_videoFilters) {
        if (filter->GetType() == type) return filter;
    }

    auto filter = std::shared_ptr<VideoFilter>(VideoFilter::Create(type));
    if (filter) m_videoFilters.push_back(filter);
    return filter;
}

void VideoPipeline::RemoveVideoFilter(VideoFilterType type) {
    std::lock_guard<std::mutex> lock(m_videoFilterMutex);
    for (auto it = m_videoFilters.begin(); it != m_videoFilters.end(); ++it) {
        if ((*it)->GetType() == type) {
            m_removedVideoFilters.push_back(*it);
            it = m_videoFilters.erase(it);
            break;
        }
    }
}

void VideoPipeline::NotifyVideoFrame(std::shared_ptr<IVideoFrame> videoFrame) {
    std::lock_guard<std::mutex> lock(m_queueMutex);
    m_frameQueue.push_back(videoFrame);
    m_queueCondVar.notify_one();
}

void VideoPipeline::NotifyVideoFinished() {
    std::lock_guard<std::recursive_mutex> lock(m_listenerMutex);
    if (m_listener) m_listener->OnVideoPipelineNotifyFinished();
}

void VideoPipeline::Stop() {
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_abort = true;
        m_queueCondVar.notify_all();
    }
    if (m_thread->joinable()) m_thread->join();
}

void VideoPipeline::PrepareTempTexture(int width, int height) {
    if (m_tempTexture.id == 0) {
        glGenTextures(1, &m_tempTexture.id);
        glBindTexture(GL_TEXTURE_2D, m_tempTexture.id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        m_tempTexture.width = width;
        m_tempTexture.height = height;
    } else if (m_tempTexture.width != width || m_tempTexture.height != height) {
        glBindTexture(GL_TEXTURE_2D, m_tempTexture.id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        m_tempTexture.width = width;
        m_tempTexture.height = height;
    }
}

void VideoPipeline::PrepareVideoFrame(std::shared_ptr<IVideoFrame> frame) {
    // Ensure temp texture exists and matches frame size before any filter render (FBO attachment).
    PrepareTempTexture(frame->width, frame->height);

    glGenTextures(1, &frame->textureId);
    glBindTexture(GL_TEXTURE_2D, frame->textureId);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, frame->width, frame->height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 frame->data.get());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // 垂直翻转画面
    if (!m_flipVerticalFilter) {
        m_flipVerticalFilter = std::shared_ptr<VideoFilter>(VideoFilter::Create(VideoFilterType::kFlipVertical));
    }
    if (m_flipVerticalFilter) {
        m_flipVerticalFilter->Render(frame, m_tempTexture.id);
    }

    std::swap(frame->textureId, m_tempTexture.id);
}

void VideoPipeline::RenderVideoFilter(std::shared_ptr<IVideoFrame> frame) {
    PrepareTempTexture(frame->width, frame->height);

    std::lock_guard<std::mutex> lock(m_videoFilterMutex);
    m_removedVideoFilters.clear();
    if (m_videoFilters.empty()) return;

    auto inputTexture = frame->textureId;
    auto outputTexture = m_tempTexture.id;

    auto filterRenderCount = 0;
    for (auto& filter : m_videoFilters) {
        if (filter->Render(frame, outputTexture)) {
            ++filterRenderCount;
            std::swap(inputTexture, outputTexture);
        }
    }

    if (filterRenderCount % 2 == 1) std::swap(frame->textureId, m_tempTexture.id);
}

static void EnsureFBO(unsigned int& fbo) {
    if (fbo != 0) return;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    LOGI(kVideoPipelineTag, "VideoPipeline FBO created: %u", fbo);
}

void VideoPipeline::ThreadLoop() {
    m_sharedGLContext->Initialize();

    unsigned int fbo{0};

#ifdef ANDROID
    // Android：context 由渲染线程 SetSharedEGLContext 创建，必须在循环内 WaitUntilReady 后再 MakeCurrent，
    // FBO 延后到首次有 current context 时创建。
#else
    // Mac/Qt：QOpenGLContext 与 shared 同线程绑定，只能在 Initialize() 后在本线程 MakeCurrent 一次，
    // 不能在循环内再次 MakeCurrent（会触发 "Cannot make current in a different thread"）。
    m_sharedGLContext->MakeCurrent();
    EnsureFBO(fbo);
#endif

    for (;;) {
        std::shared_ptr<IVideoFrame> frame;
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_queueCondVar.wait(lock, [this] { return m_abort || !m_frameQueue.empty(); });
            if (m_abort) break;
            frame = m_frameQueue.front();
            m_frameQueue.pop_front();
        }

        if (frame) {
#ifdef ANDROID
            if (!m_sharedGLContext->WaitUntilReady(10000)) {
                continue;  // 超时则丢弃本帧，不向 display 通知 textureId=0
            }
            m_sharedGLContext->MakeCurrent();
            EnsureFBO(fbo);
#endif

            PrepareVideoFrame(frame);
            RenderVideoFilter(frame);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
            glFinish();

            std::lock_guard<std::recursive_mutex> lock(m_listenerMutex);
            if (m_listener) m_listener->OnVideoPipelineNotifyVideoFrame(frame);
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_frameQueue.clear();
    }

    if (fbo != 0) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(1, &fbo);
    }
    m_sharedGLContext->DoneCurrent();
    m_sharedGLContext->Destroy();
}

}  // namespace av