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
    m_thread = std::thread(&VideoDecoder::ThreadLoop, this);
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

    std::lock_guard<std::mutex> lock(m_codecContextMutex);
    CleanupContext();

    // 根据开关选择解码器：GPU（NVIDIA CUVID）或 CPU 软件解码
    const AVCodec* codec = nullptr;
    m_isGpuDecoder = false;
    if (m_useGpu && stream->codecpar->codec_id == AV_CODEC_ID_H264) {
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
    if (!codec) return;

    // Allocate a new codec context
    m_codecContext = avcodec_alloc_context3(codec);
    if (!m_codecContext) return;

    // Copy codec parameters from the stream to the codec context
    if (avcodec_parameters_to_context(m_codecContext, stream->codecpar) < 0) {
        avcodec_free_context(&m_codecContext);
        return;
    }

    // Open the codec
    if (avcodec_open2(m_codecContext, codec, nullptr) < 0) {
        avcodec_free_context(&m_codecContext);
        return;
    }

    m_timeBase = stream->time_base;
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
    m_paused = false;
    m_notifier.Notify();
}

void VideoDecoder::Pause() { m_paused = true; }

void VideoDecoder::Stop() {
    m_abort = true;
    m_notifier.Notify();
    if (m_thread.joinable()) m_thread.join();
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
        avcodec_flush_buffers(m_codecContext);

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
    if (m_swsContext) sws_freeContext(m_swsContext);
}

void VideoDecoder::SetUseGpu(bool useGpu) { m_useGpu = useGpu; }

bool VideoDecoder::IsUsingGpu() { return m_isGpuDecoder; }

}  // namespace av