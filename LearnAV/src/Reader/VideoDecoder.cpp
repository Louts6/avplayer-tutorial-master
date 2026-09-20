//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#include "VideoDecoder.h"

#include "Log.h"

extern "C" {
#include <libavutil/imgutils.h>  // Include this header for av_image_get_buffer_size and av_image_fill_arrays
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

    // Find the decoder for the codec
    const AVCodec* codec = avcodec_find_decoder(stream->codecpar->codec_id);
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

        if (!m_swsContext) {
            m_swsContext = sws_getContext(frame->width, frame->height, (AVPixelFormat)frame->format, frame->width,
                                          frame->height, AV_PIX_FMT_RGBA, SWS_BILINEAR, nullptr, nullptr, nullptr);
            if (!m_swsContext) {
                av_frame_free(&frame);
                return;
            }
        }

        AVFrame* rgbFrame = av_frame_alloc();
        if (!rgbFrame) {
            av_frame_free(&frame);
            return;
        }

        int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGBA, frame->width, frame->height, 1);
        std::shared_ptr<uint8_t> buffer(new uint8_t[numBytes], std::default_delete<uint8_t[]>());
        if (av_image_fill_arrays(rgbFrame->data, rgbFrame->linesize, buffer.get(), AV_PIX_FMT_RGBA, frame->width,
                                 frame->height, 1) < 0) {
            av_frame_free(&frame);
            av_frame_free(&rgbFrame);
            return;
        }

        sws_scale(m_swsContext, frame->data, frame->linesize, 0, frame->height, rgbFrame->data, rgbFrame->linesize);

        auto videoFrame = std::make_shared<IVideoFrame>();
        videoFrame->width = frame->width;
        videoFrame->height = frame->height;
        videoFrame->data = std::move(buffer);
        videoFrame->pts = frame->pts;
        videoFrame->duration = frame->duration;
        videoFrame->timebaseNum = m_timeBase.num;
        videoFrame->timebaseDen = m_timeBase.den;
        videoFrame->releaseCallback = m_pipelineReleaseCallback;
        av_frame_free(&rgbFrame);

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

}  // namespace av