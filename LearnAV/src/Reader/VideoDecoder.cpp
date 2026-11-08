//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#include "VideoDecoder.h"

#include "Log.h"

extern "C" {
#include <libavutil/imgutils.h>  // Include this header for av_image_get_buffer_size and av_image_fill_arrays
#include <libavutil/hwcontext.h> // for av_hwframe_transfer_data
}

static const char* kVideoDecoderTag = "VideoDecoder";

namespace av {

IVideoDecoder* IVideoDecoder::Create() { return new VideoDecoder(); }

VideoDecoder::VideoDecoder() {
    m_pipelineReleaseCallback = std::make_shared<std::function<void()>>([&]() { ReleaseVideoPipelineResource(); });
    Start();
}

VideoDecoder::~VideoDecoder() {
    Stop();

    std::lock_guard<std::mutex> lock(m_codecContextMutex);
    CleanupContext();
}

void VideoDecoder::SetListener(Listener* listener) {
    std::lock_guard<std::recursive_mutex> lock(m_listenerMutex);
    m_listener = listener;
}

void VideoDecoder::SetStream(struct AVStream* stream) {
    if (!stream) return;

    {
        std::lock_guard<std::mutex> lock(m_packetQueueMutex);
        m_packetQueue.clear();
    }
    m_pipelineResourceCount = 3;

    std::lock_guard<std::mutex> lock(m_codecContextMutex);
    CleanupContext();

    // GPU 解码仅在开关打开且为 H.264 时启用，其余情况走原有 CPU 软解路径
    const bool useGpu = m_useGpu && stream->codecpar->codec_id == AV_CODEC_ID_H264;
    if (OpenCodec(stream, useGpu)) m_timeBase = stream->time_base;
}

bool VideoDecoder::OpenCodec(const AVStream* stream, bool useGpu) {
    const AVCodec* codec = nullptr;
    m_isGpuDecoder = false;

    if (useGpu) {
        codec = avcodec_find_decoder_by_name("h264_cuvid");
        if (codec) {
            m_isGpuDecoder = true;
            LOGI(kVideoDecoderTag, "Using GPU decoder: h264_cuvid");
        } else {
            LOGW(kVideoDecoderTag, "GPU decoder (h264_cuvid) not found, fallback to CPU");
        }
    }
    if (!codec) {
        codec = avcodec_find_decoder(stream->codecpar->codec_id);
        if (codec) LOGI(kVideoDecoderTag, "Using CPU decoder: %s", codec->name);
    }
    if (!codec) return false;

    m_codecContext = avcodec_alloc_context3(codec);
    if (!m_codecContext) return false;

    if (avcodec_parameters_to_context(m_codecContext, stream->codecpar) < 0) {
        avcodec_free_context(&m_codecContext);
        return false;
    }

    if (m_isGpuDecoder) {
        // 标准硬件解码流程：显式创建 CUDA 设备上下文并挂到解码器上。
        // h264_cuvid 内部会自建上下文，但显式创建可保证解码与 CUDA-GL 互操作使用
        // 同一个 CUDA 上下文（vaapi/d3d11va 等硬解同样依赖该字段）。
        if (av_hwdevice_ctx_create(&m_hwDeviceCtx, AV_HWDEVICE_TYPE_CUDA, nullptr, nullptr, 0) < 0) {
            LOGW(kVideoDecoderTag, "Failed to create CUDA device context, fallback to CPU decoder");
            avcodec_free_context(&m_codecContext);
            m_isGpuDecoder = false;

            codec = avcodec_find_decoder(stream->codecpar->codec_id);
            if (!codec) return false;
            m_codecContext = avcodec_alloc_context3(codec);
            if (!m_codecContext) return false;
            if (avcodec_parameters_to_context(m_codecContext, stream->codecpar) < 0) {
                avcodec_free_context(&m_codecContext);
                return false;
            }
        } else {
            m_codecContext->hw_device_ctx = av_buffer_ref(m_hwDeviceCtx);
        }
    }

    if (avcodec_open2(m_codecContext, codec, nullptr) < 0) {
        avcodec_free_context(&m_codecContext);
        av_buffer_unref(&m_hwDeviceCtx);
        m_isGpuDecoder = false;
        return false;
    }

    return true;
}

void VideoDecoder::Decode(std::shared_ptr<IAVPacket> packet) {
    if (!packet) return;

    std::lock_guard<std::mutex> lock(m_packetQueueMutex);
    if (packet->flags & AVFrameFlag::kFlush) {
        m_packetQueue.clear();
    }

    m_packetQueue.push_back(packet);
    m_notifier.Notify();
}

void VideoDecoder::Start() {
    if (m_thread && m_thread->joinable()) {
        m_paused = false;
        m_notifier.Notify();
        return;
    }
    m_abort = false;
    m_paused = false;
    m_thread = std::make_shared<std::thread>(&VideoDecoder::ThreadLoop, this);
    m_notifier.Notify();
}

void VideoDecoder::Pause() { m_paused = true; }

void VideoDecoder::Stop() {
    m_abort = true;
    m_notifier.Notify();
    if (m_thread && m_thread->joinable()) m_thread->join();
    m_thread.reset();
}

int VideoDecoder::GetVideoWidth() {
    std::lock_guard<std::mutex> lock(m_codecContextMutex);
    return m_codecContext ? m_codecContext->width : 0;
}

int VideoDecoder::GetVideoHeight() {
    std::lock_guard<std::mutex> lock(m_codecContextMutex);
    return m_codecContext ? m_codecContext->height : 0;
}

void VideoDecoder::ThreadLoop() {
    for (;;) {
        m_notifier.Wait(100);
        if (m_abort) break;
        CheckFlushPacket();
        if (!m_paused && m_pipelineResourceCount > 0) DecodeAVPacket();
    }

    {
        std::lock_guard<std::mutex> lock(m_packetQueueMutex);
        m_packetQueue.clear();
    }
}

void VideoDecoder::CheckFlushPacket() {
    std::lock_guard<std::mutex> lock(m_packetQueueMutex);
    if (m_packetQueue.empty()) return;

    auto packet = m_packetQueue.front();
    if (packet->flags & AVFrameFlag::kFlush) {
        m_packetQueue.pop_front();
        if (m_codecContext) avcodec_flush_buffers(m_codecContext);

        auto videoFrame = std::make_shared<IVideoFrame>();
        videoFrame->flags |= AVFrameFlag::kFlush;
        std::lock_guard<std::recursive_mutex> lock(m_listenerMutex);
        if (m_listener) m_listener->OnNotifyVideoFrame(videoFrame);
    }
}

void VideoDecoder::DecodeAVPacket() {
    std::shared_ptr<IAVPacket> packet;
    {
        std::lock_guard<std::mutex> lock(m_packetQueueMutex);
        if (m_packetQueue.empty()) return;

        packet = m_packetQueue.front();
        m_packetQueue.pop_front();
    }

    std::lock_guard<std::mutex> lock(m_codecContextMutex);
    if (packet->avPacket && avcodec_send_packet(m_codecContext, packet->avPacket) < 0) {
        LOGE(kVideoDecoderTag, "Error sending video packet for decoding.");
        return;
    }

    AVFrame* frame = av_frame_alloc();
    if (!frame) {
        LOGE(kVideoDecoderTag, "Could not allocate video frame.");
        return;
    }

    while (true) {
        int ret = avcodec_receive_frame(m_codecContext, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            LOGE(kVideoDecoderTag, "Error during decoding.");
            av_frame_free(&frame);
            return;
        }

        // GPU 解码帧：直接把 CUDA 硬件帧交给管线，由 CUDA-GL 互操作完成转换与上屏，
        // 避免 av_hwframe_transfer_data(GPU→CPU) + sws_scale(CPU) + glTexImage2D(CPU→GPU) 往返拷贝。
        if (frame->format == AV_PIX_FMT_CUDA && frame->hw_frames_ctx) {
            AVFrame* hwFrame = av_frame_alloc();
            if (!hwFrame) {
                av_frame_free(&frame);
                return;
            }
            av_frame_move_ref(hwFrame, frame);  // 转移所有权，frame 可继续接收下一帧

            auto videoFrame = std::make_shared<IVideoFrame>();
            videoFrame->width = hwFrame->width;
            videoFrame->height = hwFrame->height;
            videoFrame->pts = hwFrame->pts;
            videoFrame->duration = hwFrame->duration;
            videoFrame->timebaseNum = m_timeBase.num;
            videoFrame->timebaseDen = m_timeBase.den;
            videoFrame->pixelFormat = hwFrame->format;
            videoFrame->hardwareFrame = std::shared_ptr<void>(hwFrame, [](void* ptr) {
                AVFrame* f = static_cast<AVFrame*>(ptr);
                av_frame_free(&f);
            });
            videoFrame->releaseCallback = m_pipelineReleaseCallback;

            --m_pipelineResourceCount;

            {
                std::lock_guard<std::recursive_mutex> lock(m_listenerMutex);
                if (m_listener) m_listener->OnNotifyVideoFrame(videoFrame);
            }
            continue;
        }

        // 如果是 GPU 解码帧（AV_PIX_FMT_CUDA），需要从显存拷贝到内存
        AVFrame* cpuFrame = frame;
        AVFrame* transferredFrame = nullptr;
        if (frame->format == AV_PIX_FMT_CUDA) {
            transferredFrame = av_frame_alloc();
            if (!transferredFrame) {
                av_frame_free(&frame);
                return;
            }
            if (av_hwframe_transfer_data(transferredFrame, frame, 0) < 0) {
                LOGE(kVideoDecoderTag, "Failed to transfer CUDA frame to CPU.");
                av_frame_free(&transferredFrame);
                av_frame_free(&frame);
                return;
            }
            av_frame_copy_props(transferredFrame, frame);
            cpuFrame = transferredFrame;
        }

        if (!m_swsContext) {
            m_swsContext = sws_getContext(cpuFrame->width, cpuFrame->height, (AVPixelFormat)cpuFrame->format,
                                          cpuFrame->width, cpuFrame->height, AV_PIX_FMT_RGBA, SWS_BILINEAR, nullptr,
                                          nullptr, nullptr);
            if (!m_swsContext) {
                if (transferredFrame) av_frame_free(&transferredFrame);
                av_frame_free(&frame);
                return;
            }
        }

        AVFrame* rgbFrame = av_frame_alloc();
        if (!rgbFrame) {
            if (transferredFrame) av_frame_free(&transferredFrame);
            av_frame_free(&frame);
            return;
        }

        int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGBA, cpuFrame->width, cpuFrame->height, 1);
        std::shared_ptr<uint8_t> buffer(new uint8_t[numBytes], std::default_delete<uint8_t[]>());
        if (av_image_fill_arrays(rgbFrame->data, rgbFrame->linesize, buffer.get(), AV_PIX_FMT_RGBA, cpuFrame->width,
                                 cpuFrame->height, 1) < 0) {
            av_frame_free(&rgbFrame);
            if (transferredFrame) av_frame_free(&transferredFrame);
            av_frame_free(&frame);
            return;
        }

        sws_scale(m_swsContext, cpuFrame->data, cpuFrame->linesize, 0, cpuFrame->height, rgbFrame->data,
                  rgbFrame->linesize);

        auto videoFrame = std::make_shared<IVideoFrame>();
        videoFrame->width = cpuFrame->width;
        videoFrame->height = cpuFrame->height;
        videoFrame->data = std::move(buffer);
        videoFrame->pts = cpuFrame->pts;
        videoFrame->duration = cpuFrame->duration;
        videoFrame->timebaseNum = m_timeBase.num;
        videoFrame->timebaseDen = m_timeBase.den;
        videoFrame->releaseCallback = m_pipelineReleaseCallback;
        av_frame_free(&rgbFrame);

        if (transferredFrame) av_frame_free(&transferredFrame);

        --m_pipelineResourceCount;

        {
            std::lock_guard<std::recursive_mutex> lock(m_listenerMutex);
            if (m_listener) m_listener->OnNotifyVideoFrame(videoFrame);
        }
    }

    av_frame_free(&frame);
}

void VideoDecoder::ReleaseVideoPipelineResource() {
    ++m_pipelineResourceCount;
    m_notifier.Notify();
}

void VideoDecoder::CleanupContext() {
    if (m_codecContext) avcodec_free_context(&m_codecContext);
    if (m_hwDeviceCtx) av_buffer_unref(&m_hwDeviceCtx);
    if (m_swsContext) sws_freeContext(m_swsContext);
}

void VideoDecoder::SetUseGpu(bool useGpu) { m_useGpu = useGpu; }

bool VideoDecoder::IsUsingGpu() { return m_isGpuDecoder; }

}  // namespace av