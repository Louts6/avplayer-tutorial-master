//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#include "AudioEncoder.h"

#include "Log.h"

namespace av {

static const char* kAudioEncoderTag = "AudioEncoder";

AudioEncoder::AudioEncoder() { m_thread = std::thread(&AudioEncoder::ThreadLoop, this); }

AudioEncoder::~AudioEncoder() {
    StopThread();

    if (m_avFrame) av_frame_free(&m_avFrame);
    if (m_avPacket) av_packet_free(&m_avPacket);
    if (m_encodeCtx) avcodec_free_context(&m_encodeCtx);
    if (m_swrContext) swr_free(&m_swrContext);
}

void AudioEncoder::SetListener(Listener* listener) {
    std::lock_guard<std::mutex> lock(m_listenerMutex);
    m_listener = listener;
}

bool AudioEncoder::Configure(FileWriterParameters& parameters, int flags) {
    const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
    if (!codec) return false;

    m_encodeCtx = avcodec_alloc_context3(codec);
    if (!m_encodeCtx) return false;

    m_encodeCtx->sample_rate = parameters.sampleRate;
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57, 28, 100)
    av_channel_layout_default(&m_encodeCtx->ch_layout, parameters.channels);
#else
    m_encodeCtx->channel_layout = av_get_default_channel_layout(parameters.channels);
    m_encodeCtx->channels = parameters.channels;
#endif
    m_encodeCtx->sample_fmt = AV_SAMPLE_FMT_FLTP;
    m_encodeCtx->bit_rate = 128000;
    m_encodeCtx->frame_size = 1024;

    if (avcodec_open2(m_encodeCtx, codec, nullptr) < 0) return false;

    m_avPacket = av_packet_alloc();
    if (!m_avPacket) return false;

    m_avFrame = av_frame_alloc();
    if (!m_avFrame) return false;

    m_avFrame->nb_samples = m_encodeCtx->frame_size;
    m_avFrame->format = m_encodeCtx->sample_fmt;
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57, 28, 100)
    av_channel_layout_copy(&m_avFrame->ch_layout, &m_encodeCtx->ch_layout);
#else
    m_avFrame->channel_layout = m_encodeCtx->channel_layout;
    m_avFrame->channels = m_encodeCtx->channels;
#endif
    m_avFrame->sample_rate = m_encodeCtx->sample_rate;
    int ret = av_frame_get_buffer(m_avFrame, 0);
    if (ret < 0) return false;

    m_swrContext = swr_alloc();
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57, 28, 100)
    AVChannelLayout in_ch = AV_CHANNEL_LAYOUT_STEREO;
    AVChannelLayout out_ch;
    av_channel_layout_copy(&out_ch, &m_encodeCtx->ch_layout);
    swr_alloc_set_opts2(&m_swrContext, &out_ch, m_encodeCtx->sample_fmt, m_encodeCtx->sample_rate,
                        &in_ch, AV_SAMPLE_FMT_S16, parameters.sampleRate, 0, nullptr);
#else
    av_opt_set_int(m_swrContext, "in_channel_layout", AV_CH_LAYOUT_STEREO, 0);
    av_opt_set_int(m_swrContext, "in_sample_rate", parameters.sampleRate, 0);
    av_opt_set_sample_fmt(m_swrContext, "in_sample_fmt", AV_SAMPLE_FMT_S16, 0);
    av_opt_set_int(m_swrContext, "out_channel_layout", m_encodeCtx->channel_layout, 0);
    av_opt_set_int(m_swrContext, "out_sample_rate", m_encodeCtx->sample_rate, 0);
    av_opt_set_sample_fmt(m_swrContext, "out_sample_fmt", m_encodeCtx->sample_fmt, 0);
#endif

    if (swr_init(m_swrContext) < 0) return false;

    return true;
}

void AudioEncoder::NotifyAudioSamples(std::shared_ptr<IAudioSamples> audioSamples) {
    if (!audioSamples) return;
    std::lock_guard<std::mutex> lock(m_mutex);
    m_audioSamplesQueue.push_back(audioSamples);
    m_cond.notify_all();
}

void AudioEncoder::ThreadLoop() {
    for (;;) {
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cond.wait(lock, [this] { return m_abort || !m_audioSamplesQueue.empty(); });
            if (m_abort) break;
        }

        std::shared_ptr<IAudioSamples> audioSamples;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_audioSamplesQueue.empty()) return;

            audioSamples = m_audioSamplesQueue.front();
            m_audioSamplesQueue.pop_front();
        }

        if (audioSamples) {
            auto isEndOfStream = audioSamples->flags & AVFrameFlag::kEOS;
            isEndOfStream ? EncodeAudioSamples(nullptr) : PrepareEncodeAudioSamples(audioSamples);
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_audioSamplesQueue.clear();
    }
}

void AudioEncoder::StopThread() {
    m_abort = true;
    m_cond.notify_all();
    if (m_thread.joinable()) m_thread.join();
}

void AudioEncoder::PrepareEncodeAudioSamples(std::shared_ptr<IAudioSamples> audioSamples) {
    if (!m_avFrame || !m_avPacket) return;

    int ret = av_frame_make_writable(m_avFrame);
    if (ret < 0) return;

    // 重采样时需要为每个声道分配输入数据
    const uint8_t* inData[1];                                                    // 输入的双声道数据
    inData[0] = reinterpret_cast<const uint8_t*>(audioSamples->pcmData.data());  // 输入交织的 PCM 数据

    // `swr_convert` 需要将每个声道的数据分离开
    uint8_t* outData[2] = {m_avFrame->data[0], m_avFrame->data[1]};  // 输出两个声道的 FLTP 数据

    // 计算输入样本数，交织数据的总样本数需要除以声道数
    int inSamples = audioSamples->pcmData.size() / audioSamples->channels;

    // 重采样，将交织的 S16 数据转换为 FLTP 格式
    int outSamples = swr_convert(m_swrContext, outData, m_avFrame->nb_samples, inData, inSamples);
    if (outSamples < 0) {
        LOGE(kAudioEncoderTag, "Error while resampling.");
        return;
    }

    // 设置帧的时间戳
    m_avFrame->pts = m_pts;
    EncodeAudioSamples(m_avFrame);
    m_pts += m_avFrame->nb_samples;
}

void AudioEncoder::EncodeAudioSamples(const AVFrame* avFrame) {
    int ret = avcodec_send_frame(m_encodeCtx, avFrame);
    if (ret < 0) return;

    while (ret >= 0) {
        ret = avcodec_receive_packet(m_encodeCtx, m_avPacket);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return;
        } else if (ret < 0) {
            LOGE(kAudioEncoderTag, "Error encoding audio frame: %d", ret);
            return;
        }

        auto avPacket = std::make_shared<IAVPacket>(av_packet_clone(m_avPacket));
        avPacket->timeBase = m_encodeCtx->time_base;

        std::lock_guard<std::mutex> lock(m_listenerMutex);
        if (m_listener) m_listener->OnAudioEncoderNotifyPacket(avPacket);

        av_packet_unref(m_avPacket);
    }

    if (!avFrame) {
        std::lock_guard<std::mutex> lock(m_listenerMutex);
        if (m_listener) m_listener->OnAudioEncoderNotifyFinished();
    }
}

}  // namespace av
