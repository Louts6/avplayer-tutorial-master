//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#include "VideoEncoder.h"

#include "Log.h"
#include "Utils/GLUtils.h"
#include "VideoFilter/VideoFilter.h"

namespace av {

static const char* kVideoEncoderTag = "VideoEncoder";

VideoEncoder::VideoEncoder(std::shared_ptr<GLContext> glContext) {
    m_sharedGLContext = std::make_shared<GLContext>(glContext->GetSharedGLContext());
    m_thread = std::thread(&VideoEncoder::ThreadLoop, this);
}

VideoEncoder::~VideoEncoder() {
    StopThread();
    if (m_swsCtx) sws_freeContext(m_swsCtx);
    if (m_avFrame) av_frame_free(&m_avFrame);
    if (m_avPacket) av_packet_free(&m_avPacket);
    if (m_encodeCtx) avcodec_free_context(&m_encodeCtx);
}

void VideoEncoder::SetListener(Listener* listener) {
    std::lock_guard<std::mutex> lock(m_listenerMutex);
    m_listener = listener;
}

bool VideoEncoder::Configure(FileWriterParameters& parameters, int flags) {
    const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!codec) {
        LOGE(kVideoEncoderTag, "Codec not found");
        return false;
    }

    m_encodeCtx = avcodec_alloc_context3(codec);
    if (!m_encodeCtx) {
        LOGE(kVideoEncoderTag, "Could not allocate video codec context");
        return false;
    }

    m_encodeCtx->bit_rate = parameters.width * parameters.height * 4;
    m_encodeCtx->width = parameters.width;
    m_encodeCtx->height = parameters.height;
    m_encodeCtx->time_base = {1, parameters.fps};
    m_encodeCtx->framerate = {parameters.fps, 1};
    m_encodeCtx->gop_size = 30;
    m_encodeCtx->max_b_frames = 1;
    m_encodeCtx->pix_fmt = AV_PIX_FMT_YUV420P;

    if (avcodec_open2(m_encodeCtx, codec, nullptr) < 0) {
        LOGE(kVideoEncoderTag, "Could not open codec");
        return false;
    }

    m_avFrame = av_frame_alloc();
    if (!m_avFrame) {
        LOGE(kVideoEncoderTag, "Could not allocate video frame");
        return false;
    }
    m_avFrame->format = m_encodeCtx->pix_fmt;
    m_avFrame->width = m_encodeCtx->width;
    m_avFrame->height = m_encodeCtx->height;
    m_avFrame->pts = 0;

    auto ret = av_frame_get_buffer(m_avFrame, 1);
    if (ret < 0) {
        LOGE(kVideoEncoderTag, "av_frame_get_buffer failed!");
        if (m_avFrame) av_frame_free(&m_avFrame);
        return false;
    }

    m_avPacket = av_packet_alloc();
    if (!m_avPacket) {
        LOGE(kVideoEncoderTag, "Could not allocate AVPacket");
        return false;
    }

    return true;
}

void VideoEncoder::NotifyVideoFrame(std::shared_ptr<IVideoFrame> videoFrame) {
    if (!videoFrame) return;
    std::lock_guard<std::mutex> lock(m_mutex);
    m_videoFrameQueue.push_back(videoFrame);
    m_cond.notify_all();
}

void VideoEncoder::ThreadLoop() {
    m_sharedGLContext->Initialize();
    m_sharedGLContext->MakeCurrent();

    unsigned int fbo{0};
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    for (;;) {
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cond.wait(lock, [this] { return m_abort || !m_videoFrameQueue.empty(); });
            if (m_abort) break;
        }

        std::shared_ptr<IVideoFrame> videoFrame;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_videoFrameQueue.empty()) return;

            videoFrame = m_videoFrameQueue.front();
            m_videoFrameQueue.pop_front();
        }

        if (videoFrame) {
            auto isEndOfStream = videoFrame->flags & AVFrameFlag::kEOS;
            isEndOfStream ? EncodeVideoFrame(nullptr) : PrepareAndEncodeVideoFrame(videoFrame);
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_videoFrameQueue.clear();
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
    m_sharedGLContext->DoneCurrent();
    m_sharedGLContext->Destroy();
}

void VideoEncoder::StopThread() {
    m_abort = true;
    m_cond.notify_all();
    if (m_thread.joinable()) m_thread.join();
}

void VideoEncoder::FlipVideoFrame(std::shared_ptr<IVideoFrame> videoFrame) {
    if (!m_textureId) {
        m_textureId = GLUtils::GenerateTexture(m_encodeCtx->width, m_encodeCtx->height, GL_RGBA, GL_RGBA);
    }

    // 垂直翻转画面
    if (!m_flipVerticalFilter) {
        m_flipVerticalFilter = std::shared_ptr<VideoFilter>(VideoFilter::Create(VideoFilterType::kFlipVertical));
    }
    if (m_flipVerticalFilter) {
        m_flipVerticalFilter->Render(videoFrame, m_textureId);
    }
}

void VideoEncoder::ConvertTextureToFrame(unsigned int textureId, int width, int height) {
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureId, 0);

    // 确保缓冲区大小合适
    if (m_rgbaData.size() != width * height * 4) m_rgbaData.resize(width * height * 4);

    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, m_rgbaData.data());

    uint8_t* srcSlice[1] = {m_rgbaData.data()};
    int srcStride[1] = {4 * width};

    if (!m_swsCtx) {
        m_swsCtx = sws_getContext(width, height, AV_PIX_FMT_RGBA, m_encodeCtx->width, m_encodeCtx->height,
                                  AV_PIX_FMT_YUV420P, SWS_BILINEAR, nullptr, nullptr, nullptr);
        if (!m_swsCtx) {
            throw std::runtime_error("Could not reinitialize SwsContext with new dimensions");
        }
    }

    int result = sws_scale(m_swsCtx, srcSlice, srcStride, 0, height, m_avFrame->data, m_avFrame->linesize);
    if (result <= 0) throw std::runtime_error("sws_scale failed");
}

void VideoEncoder::PrepareAndEncodeVideoFrame(std::shared_ptr<IVideoFrame> videoFrame) {
    if (!m_avFrame || !m_avPacket) return;

    FlipVideoFrame(videoFrame);

    ConvertTextureToFrame(m_textureId, m_encodeCtx->width, m_encodeCtx->height);
    ++m_avFrame->pts;
    EncodeVideoFrame(m_avFrame);
}

void VideoEncoder::EncodeVideoFrame(const AVFrame* avFrame) {
    int ret = avcodec_send_frame(m_encodeCtx, avFrame);
    if (ret < 0) return;

    while (ret >= 0) {
        ret = avcodec_receive_packet(m_encodeCtx, m_avPacket);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return;
        } else if (ret < 0) {
            LOGE(kVideoEncoderTag, "Error encoding audio frame: %d", ret);
            return;
        }

        auto avPacket = std::make_shared<IAVPacket>(av_packet_clone(m_avPacket));
        avPacket->timeBase = m_encodeCtx->time_base;

        std::lock_guard<std::mutex> lock(m_listenerMutex);
        if (m_listener) m_listener->OnVideoEncoderNotifyPacket(avPacket);

        av_packet_unref(m_avPacket);
    }

    if (!avFrame) {
        std::lock_guard<std::mutex> lock(m_listenerMutex);
        if (m_listener) m_listener->OnVideoEncoderNotifyFinished();
    }
}

}  // namespace av