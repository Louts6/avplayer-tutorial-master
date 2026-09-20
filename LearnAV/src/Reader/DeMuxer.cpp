//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#include "DeMuxer.h"

#include "Log.h"

namespace av {

IDeMuxer* IDeMuxer::Create() { return new DeMuxer(); }

DeMuxer::DeMuxer() {
    m_videoStream.pipelineReleaseCallback =
        std::make_shared<std::function<void()>>([&]() { ReleaseVideoPipelineResource(); });
    m_audioStream.pipelineReleaseCallback =
        std::make_shared<std::function<void()>>([&]() { ReleaseAudioPipelineResource(); });

    m_thread = std::thread(&DeMuxer::ThreadLoop, this);
}

DeMuxer::~DeMuxer() {
    Stop();
    if (m_thread.joinable()) m_thread.join();

    std::lock_guard<std::mutex> lock(m_formatMutex);
    if (m_formatCtx) avformat_close_input(&m_formatCtx);
}

void DeMuxer::SetListener(IDeMuxer::Listener* listener) {
    std::lock_guard<std::recursive_mutex> lock(m_listenerMutex);
    m_listener = listener;
}

bool DeMuxer::Open(const std::string& url) {
    std::lock_guard<std::mutex> lock(m_formatMutex);
    if (avformat_open_input(&m_formatCtx, url.c_str(), nullptr, nullptr) != 0) {
        return false;
    }
    if (avformat_find_stream_info(m_formatCtx, nullptr) < 0) {
        return false;
    }
    for (unsigned int i = 0; i < m_formatCtx->nb_streams; ++i) {
        if (m_formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            m_audioStream.streamIndex = i;
            std::lock_guard<std::recursive_mutex> lock(m_listenerMutex);
            if (m_listener) m_listener->OnNotifyAudioStream(m_formatCtx->streams[i]);
        } else if (m_formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            m_videoStream.streamIndex = i;
            std::lock_guard<std::recursive_mutex> lock(m_listenerMutex);
            if (m_listener) m_listener->OnNotifyVideoStream(m_formatCtx->streams[i]);
        }
    }

    return true;
}

void DeMuxer::Start() {
    m_paused = false;
    m_notifier.Notify();
}

void DeMuxer::Pause() { m_paused = true; }

void DeMuxer::SeekTo(float progress) {
    m_seekProgress = progress;
    m_seek = true;
    m_notifier.Notify();
}

void DeMuxer::Stop() {
    m_abort = true;
    m_notifier.Notify();
}

float DeMuxer::GetDuration() {
    std::lock_guard<std::mutex> lock(m_formatMutex);
    if (!m_formatCtx) return 0;
    return m_formatCtx->duration / static_cast<float>(AV_TIME_BASE);
}

void DeMuxer::ThreadLoop() {
    for (;;) {
        m_notifier.Wait(100);
        if (m_abort) break;
        if (m_seek) ProcessSeek();

        if (!m_paused && (m_audioStream.pipelineResourceCount > 0 || m_videoStream.pipelineResourceCount > 0)) {
            if (!ReadAndSendPacket()) break;
        }
    }
}

void DeMuxer::ProcessSeek() {
    std::lock_guard<std::mutex> lock(m_formatMutex);
    if (!m_formatCtx) return;

    int64_t timestamp = static_cast<int64_t>(m_seekProgress * m_formatCtx->duration);
    if (av_seek_frame(m_formatCtx, -1, timestamp, AVSEEK_FLAG_BACKWARD) < 0) {
        LOGE("DeMuxer", "Seek failed");
    }

    {
        std::lock_guard<std::recursive_mutex> listenerLock(m_listenerMutex);
        auto audioPacket = std::make_shared<IAVPacket>(nullptr);
        audioPacket->flags |= AVFrameFlag::kFlush;
        m_listener->OnNotifyAudioPacket(audioPacket);

        auto videoPacket = std::make_shared<IAVPacket>(nullptr);
        videoPacket->flags |= AVFrameFlag::kFlush;
        m_listener->OnNotifyVideoPacket(videoPacket);
    }

    m_seekProgress = -1.0f;
    m_seek = false;
}

bool DeMuxer::ReadAndSendPacket() {
    std::lock_guard<std::mutex> lock(m_formatMutex);
    if (m_formatCtx == nullptr) {
        m_paused = true;
        return true;
    }

    AVPacket packet;
    auto ret = av_read_frame(m_formatCtx, &packet);
    if (ret >= 0) {
        std::lock_guard<std::recursive_mutex> lock(m_listenerMutex);
        if (m_listener) {
            if (packet.stream_index == m_audioStream.streamIndex) {
                --m_audioStream.pipelineResourceCount;
                auto sharedPacket = std::make_shared<IAVPacket>(av_packet_clone(&packet));
                sharedPacket->releaseCallback = m_audioStream.pipelineReleaseCallback;
                m_listener->OnNotifyAudioPacket(sharedPacket);
            } else if (packet.stream_index == m_videoStream.streamIndex) {
                --m_videoStream.pipelineResourceCount;
                auto sharedPacket = std::make_shared<IAVPacket>(av_packet_clone(&packet));
                sharedPacket->releaseCallback = m_videoStream.pipelineReleaseCallback;
                m_listener->OnNotifyVideoPacket(sharedPacket);
            }
        }
        av_packet_unref(&packet);
    } else if (ret == AVERROR_EOF) {
        m_paused = true;
    } else {
        return false;
    }

    return true;
}

void DeMuxer::ReleaseAudioPipelineResource() {
    ++m_audioStream.pipelineResourceCount;
    m_notifier.Notify();
}

void DeMuxer::ReleaseVideoPipelineResource() {
    ++m_videoStream.pipelineResourceCount;
    m_notifier.Notify();
}

}  // namespace av