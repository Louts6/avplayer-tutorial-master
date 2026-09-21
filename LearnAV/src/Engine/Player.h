//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#ifndef LEARNAV_PLAYER_H
#define LEARNAV_PLAYER_H

#include <unordered_set>

#include "AVSynchronizer.h"
#include "Core/TaskPool.h"
#include "IPlayer.h"
#include "Interface/IAudioPipeline.h"
#include "Interface/IAudioSpeaker.h"
#include "Interface/IFileReader.h"
#include "Interface/IFileWriter.h"
#include "Interface/IVideoDisplayView.h"
#include "Interface/IVideoPipeline.h"

namespace av {

class Player : public IPlayer,
               public IFileReader::Listener,
               public AVSynchronizer::Listener,
               public IAudioPipeline::Listener,
               public IVideoPipeline::Listener {
public:
    Player(std::shared_ptr<GLContext> glContext);
    ~Player() override;

    void AttachDisplayView(std::shared_ptr<IVideoDisplayView> displayView) override;
    void DetachDisplayView(std::shared_ptr<IVideoDisplayView> displayView) override;

    void SetPlaybackListener(std::shared_ptr<IPlaybackListener> listener) override;

    bool Open(std::string& filePath) override;
    void Play() override;
    void Pause() override;
    void SeekTo(float progress) override;
    bool IsPlaying() override;

    std::shared_ptr<IVideoFilter> AddVideoFilter(VideoFilterType type) override;
    void RemoveVideoFilter(VideoFilterType type) override;

    bool StartRecording(const std::string& outputFilePath, int flags) override;
    void StopRecording() override;
    bool IsRecording() override;

    void SetUseGpu(bool useGpu) override;
    bool IsUsingGpu() override;

private:
    void InitTaskPoolGLContext();
    void DestroyTaskPoolGLContext();

    // 继承自IFileReader::Listener
    void OnFileReaderNotifyAudioSamples(std::shared_ptr<IAudioSamples>) override;
    void OnFileReaderNotifyVideoFrame(std::shared_ptr<IVideoFrame>) override;
    void OnFileReaderNotifyAudioFinished() override;
    void OnFileReaderNotifyVideoFinished() override;

    // 继承自AVSynchronizer::Listener
    void OnAVSynchronizerNotifyAudioSamples(std::shared_ptr<IAudioSamples>) override;
    void OnAVSynchronizerNotifyVideoFrame(std::shared_ptr<IVideoFrame>) override;
    void OnAVSynchronizerNotifyAudioFinished() override;
    void OnAVSynchronizerNotifyVideoFinished() override;

    // 继承自IAudioPipeline::Listener
    void OnAudioPipelineNotifyAudioSamples(std::shared_ptr<IAudioSamples>) override;
    void OnAudioPipelineNotifyFinished() override;

    // 继承自IVideoPipeline::Listener
    void OnVideoPipelineNotifyVideoFrame(std::shared_ptr<IVideoFrame>) override;
    void OnVideoPipelineNotifyFinished() override;

private:
    std::shared_ptr<GLContext> m_glContext;
    std::shared_ptr<GLContext> m_taskPoolGLContext;

    std::shared_ptr<IPlaybackListener> m_playbackListener;
    std::recursive_mutex m_playbackListenerMutex;

    // 文件读取器
    std::shared_ptr<IFileReader> m_fileReader;

    // 音画同步
    std::shared_ptr<AVSynchronizer> m_avSynchronizer;

    // 音视频处理管线
    std::shared_ptr<IAudioPipeline> m_audioPipeline;
    std::shared_ptr<IVideoPipeline> m_videoPipeline;

    // 音频扬声器
    std::shared_ptr<IAudioSpeaker> m_audioSpeaker;

    // 视频播放
    std::unordered_set<std::shared_ptr<IVideoDisplayView>> m_displayViews;
    std::recursive_mutex m_displayViewsMutex;

    // 视频录制
    std::shared_ptr<IFileWriter> m_fileWriter;
    std::mutex m_fileWriterMutex;

    // 用于执行需要GL环境的操作，例如销毁纹理等
    std::shared_ptr<TaskPool> m_taskPool;

    std::atomic<bool> m_isPlaying{false};
    std::atomic<bool> m_isRecording{false};
    std::atomic<bool> m_useGpu{false};  // 是否使用 GPU 编解码（默认 false）
};

}  // namespace av

#endif  // LEARNAV_PLAYER_H
