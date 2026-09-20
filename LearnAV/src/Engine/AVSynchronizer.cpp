//
// Copyright (c) 2024 Yellow. All rights reserved.
//
#include "AVSynchronizer.h"

#include <iostream>

namespace av {

AVSynchronizer::AVSynchronizer(std::shared_ptr<GLContext> glContext) {
    m_glContext = std::make_shared<GLContext>(glContext->GetSharedGLContext());
    Start();
}

AVSynchronizer::~AVSynchronizer() { Stop(); }

void AVSynchronizer::SetListener(Listener* listener) {
    std::lock_guard<std::mutex> lock(m_listenerMutex);
    m_listener = listener;
}

void AVSynchronizer::Start() {
    m_abort = false;
    m_audioStreamInfo.Reset();
    m_videoStreamInfo.Reset();
    m_syncThread = std::thread(&AVSynchronizer::ThreadLoop, this);
}

void AVSynchronizer::Stop() {
    m_abort = true;
    m_notifier.Notify();
    if (m_syncThread.joinable()) m_syncThread.join();
}

void AVSynchronizer::Reset() {
    m_reset = true;
    m_notifier.Notify();
}

void AVSynchronizer::NotifyAudioSamples(std::shared_ptr<IAudioSamples> audioSamples) {
    if (!audioSamples) return;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (audioSamples->flags & AVFrameFlag::kFlush) m_audioQueue.clear();
        m_audioQueue.push_back(audioSamples);
    }
    m_notifier.Notify();
}

void AVSynchronizer::NotifyVideoFrame(std::shared_ptr<IVideoFrame> videoFrame) {
    if (!videoFrame) return;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (videoFrame->flags & AVFrameFlag::kFlush) m_videoQueue.clear();
        m_videoQueue.push_back(videoFrame);
    }
    m_notifier.Notify();
}

void AVSynchronizer::NotifyAudioFinished() {
    auto audioSamples = std::make_shared<IAudioSamples>();
    audioSamples->flags |= AVFrameFlag::kEOS;
    NotifyAudioSamples(audioSamples);
}

void AVSynchronizer::NotifyVideoFinished() {
    auto videoFrame = std::make_shared<IVideoFrame>();
    videoFrame->flags |= AVFrameFlag::kEOS;
    NotifyVideoFrame(videoFrame);
}

void AVSynchronizer::ThreadLoop() {
    m_glContext->Initialize();
    m_glContext->MakeCurrent();

    for (;;) {
        m_notifier.Wait(100);
        if (m_abort) break;
        if (m_reset) {
            m_audioQueue.clear();
            m_videoQueue.clear();
            m_reset = false;
        }
        Synchronize();
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_videoQueue.clear();
    }

    m_glContext->DoneCurrent();
    m_glContext->Destroy();
}

void AVSynchronizer::Synchronize() {
    std::unique_lock<std::mutex> lock(m_mutex);

    // 处理音频帧
    while (!m_audioQueue.empty()) {
        auto audioSamples = m_audioQueue.front();
        if (audioSamples->flags & AVFrameFlag::kEOS) {
            m_audioStreamInfo.isFinished = true;
            m_audioQueue.pop_front();
            std::lock_guard<std::mutex> listenerLock(m_listenerMutex);
            if (m_listener) m_listener->OnAVSynchronizerNotifyAudioFinished();
        } else if (audioSamples->flags & AVFrameFlag::kFlush) {
            m_audioQueue.clear();
            m_videoQueue.clear();
        } else {
            m_audioStreamInfo.currentTimeStamp = audioSamples->GetTimeStamp();
            m_audioQueue.pop_front();
            std::lock_guard<std::mutex> listenerLock(m_listenerMutex);
            if (m_listener) m_listener->OnAVSynchronizerNotifyAudioSamples(audioSamples);
        }
    }

    // 处理视频帧
    while (!m_videoQueue.empty()) {
        auto videoFrame = m_videoQueue.front();
        if (videoFrame->flags & AVFrameFlag::kEOS) {
            m_videoStreamInfo.isFinished = true;
            m_videoQueue.pop_front();
            std::lock_guard<std::mutex> listenerLock(m_listenerMutex);
            if (m_listener) m_listener->OnAVSynchronizerNotifyVideoFinished();
            continue;
        } else if (videoFrame->flags & AVFrameFlag::kFlush) {
            m_audioQueue.clear();
            m_videoQueue.clear();
            continue;
        }

        // 比较音频和视频时间戳，同步音视频
        auto timeDiff = m_audioStreamInfo.currentTimeStamp - videoFrame->GetTimeStamp();
        if (timeDiff > syncThreshold) {
            // 视频帧落后太多，丢弃这一帧视频（或者快速播放）
            m_videoStreamInfo.currentTimeStamp = videoFrame->GetTimeStamp();
            m_videoQueue.pop_front();
            std::lock_guard<std::mutex> listenerLock(m_listenerMutex);
            if (m_listener) m_listener->OnAVSynchronizerNotifyVideoFrame(videoFrame);
            continue;
        } else if (timeDiff < -syncThreshold) {
            // 视频帧太快，等待下一帧视频
            break;
        } else {
            // 视频和音频在容忍范围内同步
            m_videoStreamInfo.currentTimeStamp = videoFrame->GetTimeStamp();
            m_videoQueue.pop_front();
            std::lock_guard<std::mutex> listenerLock(m_listenerMutex);
            if (m_listener) m_listener->OnAVSynchronizerNotifyVideoFrame(videoFrame);
            break;
        }
    }
}

}  // namespace av
