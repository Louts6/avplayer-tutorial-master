//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#include "Player.h"

#include "Core/SyncNotifier.h"
#include "Log.h"

static const char* kPlayerTag = "Player";

namespace av {

IPlayer* IPlayer::Create(std::shared_ptr<GLContext> glContext) { return new Player(glContext); }

Player::Player(std::shared_ptr<GLContext> glContext) : m_glContext(glContext), m_taskPoolGLContext(glContext) {
    m_taskPool = std::make_shared<TaskPool>();
    InitTaskPoolGLContext();

    // 文件读取器
    m_fileReader = std::shared_ptr<IFileReader>(IFileReader::Create());

    // 音视频同步器
    m_avSynchronizer = std::make_shared<AVSynchronizer>(m_glContext);

    // 音视频处理管线
    m_audioPipeline = std::shared_ptr<IAudioPipeline>(IAudioPipeline::Create(2, 44100));
    m_videoPipeline = std::shared_ptr<IVideoPipeline>(IVideoPipeline::Create(m_glContext));

    // 音频输出设备
    m_audioSpeaker = std::shared_ptr<IAudioSpeaker>(IAudioSpeaker::Create(2, 44100));

    // 串联各个模块
    m_fileReader->SetListener(this);
    m_avSynchronizer->SetListener(this);
    m_audioPipeline->SetListener(this);
    m_videoPipeline->SetListener(this);
}

Player::~Player() {
    m_fileReader->Stop();
    m_avSynchronizer->Stop();
    m_videoPipeline->Stop();
    if (m_audioSpeaker) m_audioSpeaker->Stop();
    for (auto display : m_displayViews) {
        display->Clear();
    }
    m_displayViews.clear();

    m_fileReader = nullptr;
    m_avSynchronizer = nullptr;
    m_audioPipeline = nullptr;
    m_videoPipeline = nullptr;
    m_audioSpeaker = nullptr;
    m_fileWriter = nullptr;

    DestroyTaskPoolGLContext();
}

void Player::InitTaskPoolGLContext() {
    m_taskPool->SubmitTask([this]() {
        m_taskPoolGLContext->Initialize();
        m_taskPoolGLContext->MakeCurrent();
    });
}

void Player::DestroyTaskPoolGLContext() {
    SyncNotifier notifier;
    m_taskPool->SubmitTask([&]() {
        m_taskPoolGLContext->MakeCurrent();
        m_taskPoolGLContext->Destroy();
        notifier.Notify();
    });
    notifier.Wait();
}

void Player::AttachDisplayView(std::shared_ptr<IVideoDisplayView> displayView) {
    std::lock_guard<std::recursive_mutex> lock(m_displayViewsMutex);
    displayView->SetTaskPool(m_taskPool);
    m_displayViews.insert(displayView);
}

void Player::DetachDisplayView(std::shared_ptr<IVideoDisplayView> displayView) {
    std::lock_guard<std::recursive_mutex> lock(m_displayViewsMutex);
    displayView->Clear();
    m_displayViews.erase(displayView);
}

void Player::SetPlaybackListener(std::shared_ptr<IPlaybackListener> listener) {
    std::lock_guard<std::recursive_mutex> lock(m_playbackListenerMutex);
    m_playbackListener = listener;
}

bool Player::Open(std::string& filePath) {
    if (!m_fileReader) return false;
    m_fileReader->SetUseGpu(m_useGpu);
    return m_fileReader->Open(filePath);
}

void Player::Play() {
    if (m_fileReader) m_fileReader->Start();
    m_isPlaying = true;

    std::lock_guard<std::recursive_mutex> lock(m_playbackListenerMutex);
    if (m_playbackListener) m_playbackListener->NotifyPlaybackStarted();
}

void Player::Pause() {
    if (!m_fileReader) return;
    m_fileReader->Pause();
    m_isPlaying = false;

    std::lock_guard<std::recursive_mutex> lock(m_playbackListenerMutex);
    if (m_playbackListener) m_playbackListener->NotifyPlaybackPaused();
}

void Player::SeekTo(float progress) {
    if (IsPlaying()) Pause();
    if (!m_fileReader) return;
    m_fileReader->SeekTo(progress);
    m_avSynchronizer->Reset();
}

bool Player::IsPlaying() { return m_isPlaying; }

std::shared_ptr<IVideoFilter> Player::AddVideoFilter(VideoFilterType type) {
    return m_videoPipeline ? m_videoPipeline->AddVideoFilter(type) : nullptr;
}

void Player::RemoveVideoFilter(VideoFilterType type) {
    if (m_videoPipeline) m_videoPipeline->RemoveVideoFilter(type);
}

bool Player::StartRecording(const std::string& outputFilePath, int flags) {
    std::lock_guard<std::mutex> lock(m_fileWriterMutex);
    if (m_fileWriter) m_fileWriter->StopWriter();
    m_fileWriter = std::shared_ptr<IFileWriter>(IFileWriter::Create(m_glContext));
    m_fileWriter->SetUseGpu(m_useGpu);

    FileWriterParameters parameters;
    parameters.width = m_fileReader->GetVideoWidth();
    parameters.height = m_fileReader->GetVideoHeight();
    m_isRecording = m_fileWriter->StartWriter(outputFilePath, parameters, flags);
    return m_isRecording;
}

void Player::StopRecording() {
    std::lock_guard<std::mutex> lock(m_fileWriterMutex);
    if (m_fileWriter) {
        m_fileWriter->StopWriter();
        m_fileWriter = nullptr;
    }
    m_isRecording = false;
}

bool Player::IsRecording() { return m_isRecording; }

void Player::SetUseGpu(bool useGpu) { m_useGpu = useGpu; }

bool Player::IsUsingGpu() { return m_useGpu; }

// 继承自IFileReader::Listener
void Player::OnFileReaderNotifyAudioSamples(std::shared_ptr<IAudioSamples> audioSamples) {
    if (m_avSynchronizer) m_avSynchronizer->NotifyAudioSamples(audioSamples);
}

void Player::OnFileReaderNotifyVideoFrame(std::shared_ptr<IVideoFrame> videoFrame) {
    if (m_avSynchronizer) m_avSynchronizer->NotifyVideoFrame(videoFrame);
}

void Player::OnFileReaderNotifyAudioFinished() {
    if (m_avSynchronizer) m_avSynchronizer->NotifyAudioFinished();
}

void Player::OnFileReaderNotifyVideoFinished() {
    if (m_avSynchronizer) m_avSynchronizer->NotifyVideoFinished();
}

// 继承自AVSynchronizer::Listener
void Player::OnAVSynchronizerNotifyAudioSamples(std::shared_ptr<IAudioSamples> audioSamples) {
    if (m_audioPipeline) m_audioPipeline->NotifyAudioSamples(audioSamples);
}

void Player::OnAVSynchronizerNotifyVideoFrame(std::shared_ptr<IVideoFrame> videoFrame) {
    if (m_videoPipeline) m_videoPipeline->NotifyVideoFrame(videoFrame);
}

void Player::OnAVSynchronizerNotifyAudioFinished() {
    if (m_audioPipeline) m_audioPipeline->NotifyAudioFinished();
}

void Player::OnAVSynchronizerNotifyVideoFinished() {
    if (m_videoPipeline) m_videoPipeline->NotifyVideoFinished();
}

// 继承自IAudioPipeline::Listener
void Player::OnAudioPipelineNotifyAudioSamples(std::shared_ptr<IAudioSamples> audioSamples) {
    if (m_audioSpeaker) m_audioSpeaker->PlayAudioSamples(audioSamples);

    {
        std::lock_guard<std::mutex> lock(m_fileWriterMutex);
        if (m_fileWriter) m_fileWriter->NotifyAudioSamples(audioSamples);
    }

    std::lock_guard<std::recursive_mutex> lock(m_playbackListenerMutex);
    if (m_playbackListener) {
        m_playbackListener->NotifyPlaybackTimeChanged(audioSamples->GetTimeStamp(), m_fileReader->GetDuration());
    }
}

void Player::OnAudioPipelineNotifyFinished() {
    {
        std::lock_guard<std::recursive_mutex> lock(m_playbackListenerMutex);
        if (m_playbackListener) m_playbackListener->NotifyPlaybackEOF();
    }

    std::lock_guard<std::mutex> lock(m_fileWriterMutex);
    if (m_fileWriter) m_fileWriter->NotifyAudioFinished();
}

// 继承自IVideoPipeline::Listener
void Player::OnVideoPipelineNotifyVideoFrame(std::shared_ptr<IVideoFrame> videoFrame) {
    if (!videoFrame) {
        LOGE(kPlayerTag, "OnVideoPipelineNotifyVideoFrame: received null frame");
        return;
    }

    for (const auto& display : m_displayViews) {
        display->Render(videoFrame, IVideoDisplayView::EContentMode::kScaleAspectFit);
    }
    std::lock_guard<std::mutex> lock(m_fileWriterMutex);
    if (m_fileWriter) m_fileWriter->NotifyVideoFrame(videoFrame);
}

void Player::OnVideoPipelineNotifyFinished() {
    std::lock_guard<std::mutex> lock(m_fileWriterMutex);
    if (m_fileWriter) m_fileWriter->NotifyVideoFinished();
}

}  // namespace av