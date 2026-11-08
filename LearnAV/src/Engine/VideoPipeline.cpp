//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#include "VideoPipeline.h"

#include "Log.h"
#include "Utils/GLUtils.h"

extern "C" {
#include <libavutil/imgutils.h>  // av_image_get_buffer_size
#include <libavutil/hwcontext.h> // av_hwframe_transfer_data
#include <libswscale/swscale.h>
}

static const char* kVideoPipelineTag = "VideoPipeline";

namespace av {

IVideoPipeline* IVideoPipeline::Create(std::shared_ptr<GLContext> glContext) { return new VideoPipeline(glContext); }

VideoPipeline::VideoPipeline(std::shared_ptr<GLContext> glContext)
    : m_sharedGLContext(glContext) {
    Start();
}

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
    if (m_thread && m_thread->joinable()) m_thread->join();
    m_thread.reset();
}

void VideoPipeline::Start() {
    std::lock_guard<std::mutex> lock(m_queueMutex);
    // 若线程已在运行则无需重启
    if (m_thread && m_thread->joinable()) return;
    m_abort = false;
    m_frameQueue.clear();
    m_thread = std::make_shared<std::thread>(&VideoPipeline::ThreadLoop, this);
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

    // GPU 硬件帧：优先走 CUDA-GL 零拷贝，失败则回退到 GPU→CPU 传输
    if (frame->IsHardwareFrame()) {
        if (!PrepareHardwareFrame(frame)) ConvertHardwareFrameToCpu(frame);
    }

    // 上传（或回退后上传）RGBA 源纹理；GPU 零拷贝路径已直接生成纹理
    if (frame->textureId == 0) {
        if (!frame->data) return;

        glGenTextures(1, &frame->textureId);
        glBindTexture(GL_TEXTURE_2D, frame->textureId);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, frame->width, frame->height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     frame->data.get());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    // 垂直翻转画面
    if (!m_flipVerticalFilter) {
        m_flipVerticalFilter = std::shared_ptr<VideoFilter>(VideoFilter::Create(VideoFilterType::kFlipVertical));
    }
    if (m_flipVerticalFilter) {
        m_flipVerticalFilter->Render(frame, m_tempTexture.id);
    }

    std::swap(frame->textureId, m_tempTexture.id);
}

bool VideoPipeline::PrepareHardwareFrame(std::shared_ptr<IVideoFrame> frame) {
    if (!m_cudaRenderer) m_cudaRenderer = std::make_unique<CudaFrameRenderer>();

    unsigned int texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, frame->width, frame->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    if (!m_cudaRenderer->RenderHardwareFrame(frame->hardwareFrame.get(), texture, frame->width, frame->height)) {
        glDeleteTextures(1, &texture);
        return false;
    }

    frame->textureId = texture;
    return true;
}

void VideoPipeline::ConvertHardwareFrameToCpu(std::shared_ptr<IVideoFrame> frame) {
    auto* hwFrame = static_cast<AVFrame*>(frame->hardwareFrame.get());
    AVFrame* swFrame = av_frame_alloc();
    if (!swFrame) return;

    if (av_hwframe_transfer_data(swFrame, hwFrame, 0) < 0) {
        LOGE(kVideoPipelineTag, "Failed to transfer hardware frame to CPU, frame dropped");
        av_frame_free(&swFrame);
        return;
    }

    const int format = swFrame->format;
    if (!m_swsContext || m_swsWidth != swFrame->width || m_swsHeight != swFrame->height || m_swsFormat != format) {
        if (m_swsContext) sws_freeContext(m_swsContext);
        m_swsContext = sws_getContext(swFrame->width, swFrame->height, static_cast<AVPixelFormat>(format),
                                      swFrame->width, swFrame->height, AV_PIX_FMT_RGBA, SWS_BILINEAR, nullptr,
                                      nullptr, nullptr);
        m_swsWidth = swFrame->width;
        m_swsHeight = swFrame->height;
        m_swsFormat = format;
    }
    if (!m_swsContext) {
        av_frame_free(&swFrame);
        return;
    }

    const int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGBA, swFrame->width, swFrame->height, 1);
    std::shared_ptr<uint8_t> buffer(new uint8_t[numBytes], std::default_delete<uint8_t[]>());

    uint8_t* dstData[4] = {buffer.get(), nullptr, nullptr, nullptr};
    int dstLinesize[4] = {swFrame->width * 4, 0, 0, 0};
    sws_scale(m_swsContext, swFrame->data, swFrame->linesize, 0, swFrame->height, dstData, dstLinesize);

    frame->width = swFrame->width;
    frame->height = swFrame->height;
    frame->data = std::move(buffer);
    av_frame_free(&swFrame);
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
    if (!m_sharedGLContext->Initialize()) {
        LOGE(kVideoPipelineTag, "Failed to initialize GL context on pipeline thread, video rendering disabled");
        return;
    }

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

    // GPU 零拷贝与回退路径资源必须在 GL 线程释放
    if (m_cudaRenderer) {
        m_cudaRenderer->Release();
        m_cudaRenderer.reset();
    }
    if (m_swsContext) {
        sws_freeContext(m_swsContext);
        m_swsContext = nullptr;
    }

    m_sharedGLContext->DoneCurrent();
    m_sharedGLContext->Destroy();
}

}  // namespace av