//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#include "Muxer.h"

#include "Log.h"

namespace av {

static const char* kMuxerTag = "Muxer";

Muxer::~Muxer() {
    if (m_formatContext) {
        av_write_trailer(m_formatContext);
        avio_closep(&m_formatContext->pb);
        avformat_free_context(m_formatContext);
        m_formatContext = nullptr;
    }
}

bool Muxer::Configure(const std::string& outputFilePath, FileWriterParameters& parameters, int flags) {
    std::lock_guard<std::mutex> lock(m_muxerMutex);

    // 创建输出上下文
    if (avformat_alloc_output_context2(&m_formatContext, nullptr, "mp4", outputFilePath.c_str()) < 0) {
        LOGE(kMuxerTag, "Could not create output context");
        return false;
    }

    // 添加音频流
    m_audioStream.avStream = avformat_new_stream(m_formatContext, nullptr);
    if (!m_audioStream.avStream) {
        LOGE(kMuxerTag, "Failed to create audio stream");
        return false;
    }

    // 设置音频流参数
    const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
    if (codec) {
        AVCodecContext* codecContext = avcodec_alloc_context3(codec);
        if (codecContext) {
            codecContext->sample_rate = parameters.sampleRate;
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57, 28, 100)
            av_channel_layout_default(&codecContext->ch_layout, parameters.channels);
#else
            codecContext->channel_layout = av_get_default_channel_layout(parameters.channels);
            codecContext->channels = parameters.channels;
#endif
            codecContext->sample_fmt = AV_SAMPLE_FMT_FLTP;
            codecContext->bit_rate = 128000;
            codecContext->frame_size = 1024;

            if (avcodec_open2(codecContext, codec, nullptr) == 0) {
                avcodec_parameters_from_context(m_audioStream.avStream->codecpar, codecContext);
            }
            avcodec_free_context(&codecContext);
        }
    }

    // 添加视频流
    m_videoStream.avStream = avformat_new_stream(m_formatContext, nullptr);
    if (!m_videoStream.avStream) {
        LOGE(kMuxerTag, "Failed to create video stream");
        return false;
    }

    // 设置视频流参数
    codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (codec) {
        AVCodecContext* codecContext = avcodec_alloc_context3(codec);
        if (codecContext) {
            codecContext->bit_rate = parameters.width * parameters.height * 4;
            codecContext->width = parameters.width;
            codecContext->height = parameters.height;
            codecContext->time_base = {1, parameters.fps};
            codecContext->framerate = {parameters.fps, 1};
            codecContext->gop_size = 30;
            codecContext->max_b_frames = 1;
            codecContext->pix_fmt = AV_PIX_FMT_YUV420P;
            if (avcodec_open2(codecContext, codec, nullptr) == 0) {
                avcodec_parameters_from_context(m_videoStream.avStream->codecpar, codecContext);
            }
            avcodec_free_context(&codecContext);
        }
    }

    // 打开输出文件
    if (!(m_formatContext->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&m_formatContext->pb, outputFilePath.c_str(), AVIO_FLAG_WRITE) < 0) {
            LOGE(kMuxerTag, "Could not open output file");
            return false;
        }
    }

    // 写文件头
    AVDictionary* movOpt = nullptr;
    av_dict_set(&movOpt, "movflags", "faststart", 0);
    auto ret = avformat_write_header(m_formatContext, &movOpt);
    if (ret < 0) {
        char errbuf[128];
        av_strerror(ret, errbuf, sizeof(errbuf));
        LOGE(kMuxerTag, "Error occurred when opening output file: %s", errbuf);
        return false;
    }

    return true;
}

void Muxer::NotifyAudioPacket(std::shared_ptr<IAVPacket> packet) {
    if (!packet || !packet->avPacket) return;

    std::lock_guard<std::mutex> lock(m_muxerMutex);
    if (m_audioStream.avStream && !m_audioStream.isFinished) {
        auto avPacket = packet->avPacket;
        avPacket->stream_index = m_audioStream.avStream->index;
        avPacket->pts = av_rescale_q(avPacket->pts, packet->timeBase, m_audioStream.avStream->time_base);
        avPacket->dts = av_rescale_q(avPacket->dts, packet->timeBase, m_audioStream.avStream->time_base);
        avPacket->duration = av_rescale_q(avPacket->duration, packet->timeBase, m_audioStream.avStream->time_base);
        m_audioStream.packetQueue.push_back(packet);
        WriteInterleavedPackets();
    }
}

void Muxer::NotifyVideoPacket(std::shared_ptr<IAVPacket> packet) {
    if (!packet || !packet->avPacket) return;

    std::lock_guard<std::mutex> lock(m_muxerMutex);
    if (m_videoStream.avStream && !m_videoStream.isFinished) {
        auto avPacket = packet->avPacket;
        avPacket->stream_index = m_videoStream.avStream->index;
        avPacket->pts = av_rescale_q(avPacket->pts, packet->timeBase, m_videoStream.avStream->time_base);
        avPacket->dts = av_rescale_q(avPacket->dts, packet->timeBase, m_videoStream.avStream->time_base);
        avPacket->duration = av_rescale_q(avPacket->duration, packet->timeBase, m_videoStream.avStream->time_base);
        m_videoStream.packetQueue.push_back(packet);
        WriteInterleavedPackets();
    }
}

void Muxer::WriteInterleavedPackets() {
    while (!m_audioStream.packetQueue.empty() && !m_videoStream.packetQueue.empty()) {
        auto audioPacket = m_audioStream.packetQueue.front();
        auto videoPacket = m_videoStream.packetQueue.front();

        // 分别使用每个包的时间基
        double audioTime = audioPacket->avPacket->pts * av_q2d(m_audioStream.avStream->time_base);
        double videoTime = videoPacket->avPacket->pts * av_q2d(m_videoStream.avStream->time_base);
        LOGI(kMuxerTag, "audioTime: %f, videoTime: %f", audioTime, videoTime);

        if (audioTime <= videoTime) {
            m_audioStream.packetQueue.pop_front();
            av_interleaved_write_frame(m_formatContext, audioPacket->avPacket);
            av_packet_unref(audioPacket->avPacket);  // 释放写入后的包
        } else {
            m_videoStream.packetQueue.pop_front();
            av_interleaved_write_frame(m_formatContext, videoPacket->avPacket);
            av_packet_unref(videoPacket->avPacket);  // 释放写入后的包
        }
    }
}

void Muxer::NotifyAudioFinished() {
    LOGI(kMuxerTag, "NotifyAudioFinished");
    std::lock_guard<std::mutex> lock(m_muxerMutex);
    m_audioStream.isFinished = true;
}

void Muxer::NotifyVideoFinished() {
    LOGI(kMuxerTag, "NotifyVideoFinished");
    std::lock_guard<std::mutex> lock(m_muxerMutex);
    m_videoStream.isFinished = true;
}

}  // namespace av
