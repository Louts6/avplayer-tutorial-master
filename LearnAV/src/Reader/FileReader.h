//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#ifndef LEARNAV_FILEREADER_H
#define LEARNAV_FILEREADER_H

#include "AudioDecoder.h"
#include "DeMuxer.h"
#include "Interface/IFileReader.h"
#include "VideoDecoder.h"

namespace av {

class FileReader : public IFileReader,
                   public IDeMuxer::Listener,
                   public IAudioDecoder::Listener,
                   public IVideoDecoder::Listener {
public:
    FileReader();
    ~FileReader() override;

    void SetListener(IFileReader::Listener* listener) override;

    bool Open(std::string& filePath) override;
    void Start() override;
    void Pause() override;
    void SeekTo(float progress) override;
    void Stop() override;

    float GetDuration() override;
    int GetVideoWidth() override;
    int GetVideoHeight() override;

private:
    // 继承自IDeMuxer::Listener
    void OnDeMuxStart() override;
    void OnDeMuxStop() override;
    void OnDeMuxEOF() override;
    void OnDeMuxError(int code, const char* msg) override;
    void OnNotifyAudioStream(struct AVStream* stream) override;
    void OnNotifyVideoStream(struct AVStream* stream) override;
    void OnNotifyAudioPacket(std::shared_ptr<IAVPacket> packet) override;
    void OnNotifyVideoPacket(std::shared_ptr<IAVPacket> packet) override;

    // 继承自IAudioDecoder::Listener
    void OnNotifyAudioSamples(std::shared_ptr<IAudioSamples>) override;

    // 继承自IVideoDecoder::Listener
    void OnNotifyVideoFrame(std::shared_ptr<IVideoFrame>) override;

private:
    // 数据回调
    IFileReader::Listener* m_listener{nullptr};
    std::recursive_mutex m_listenerMutex;

    // 解封装器
    std::shared_ptr<IDeMuxer> m_deMuxer;

    // 解码器
    std::shared_ptr<IAudioDecoder> m_audioDecoder;
    std::shared_ptr<IVideoDecoder> m_videoDecoder;
};

}  // namespace av

#endif  // LEARNAV_FILEREADER_H
