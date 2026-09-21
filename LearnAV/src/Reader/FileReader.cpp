//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#include "FileReader.h"

#include "Log.h"

namespace av {

static const char* kFileReaderTag = "FileReader";

IFileReader* IFileReader::Create() { return new FileReader(); }

FileReader::FileReader() {
    // 解封装器
    m_deMuxer = std::shared_ptr<IDeMuxer>(IDeMuxer::Create());

    // 音视频解码器
    m_audioDecoder = std::shared_ptr<IAudioDecoder>(IAudioDecoder::Create(2, 44100));
    m_videoDecoder = std::shared_ptr<IVideoDecoder>(IVideoDecoder::Create());

    // 串联各个模块
    m_deMuxer->SetListener(this);
    m_audioDecoder->SetListener(this);
    m_videoDecoder->SetListener(this);
}

FileReader::~FileReader() {
    m_deMuxer->Pause();
    m_audioDecoder = nullptr;
    m_videoDecoder = nullptr;
    m_deMuxer = nullptr;
}

void FileReader::SetListener(IFileReader::Listener* listener) {
    std::lock_guard<std::recursive_mutex> lock(m_listenerMutex);
    m_listener = listener;
}

bool FileReader::Open(std::string& filePath) {
    if (!m_deMuxer) return false;
    LOGI(kFileReaderTag, "Open filePath: %s", filePath.c_str());
    return m_deMuxer->Open(filePath);
}

void FileReader::Start() {
    if (m_deMuxer) m_deMuxer->Start();
    if (m_audioDecoder) m_audioDecoder->Start();
    if (m_videoDecoder) m_videoDecoder->Start();
}

void FileReader::Pause() {
    if (m_deMuxer) m_deMuxer->Pause();
    if (m_audioDecoder) m_audioDecoder->Pause();
    if (m_videoDecoder) m_videoDecoder->Pause();
}

void FileReader::SeekTo(float position) {
    if (m_deMuxer) m_deMuxer->SeekTo(position);
}

void FileReader::Stop() {
    if (m_deMuxer) m_deMuxer->Stop();
    if (m_audioDecoder) m_audioDecoder->Stop();
    if (m_videoDecoder) m_videoDecoder->Stop();
}

float FileReader::GetDuration() { return m_deMuxer ? m_deMuxer->GetDuration() : 0; }

int FileReader::GetVideoWidth() { return m_videoDecoder ? m_videoDecoder->GetVideoWidth() : 0; }

int FileReader::GetVideoHeight() { return m_videoDecoder ? m_videoDecoder->GetVideoHeight() : 0; }

void FileReader::SetUseGpu(bool useGpu) {
    if (m_videoDecoder) m_videoDecoder->SetUseGpu(useGpu);
}

// 继承自IDeMuxer::Listener
void FileReader::OnDeMuxStart() { LOGI(kFileReaderTag, "OnDeMuxStart"); }

void FileReader::OnDeMuxStop() { LOGI(kFileReaderTag, "OnDeMuxStop"); }

void FileReader::OnDeMuxEOF() { LOGI(kFileReaderTag, "OnDeMuxEOF"); }

void FileReader::OnDeMuxError(int code, const char* msg) { LOGI(kFileReaderTag, "OnDeMuxError"); }

void FileReader::OnNotifyAudioStream(struct AVStream* stream) {
    if (m_audioDecoder) m_audioDecoder->SetStream(stream);
}

void FileReader::OnNotifyVideoStream(struct AVStream* stream) {
    if (m_videoDecoder) m_videoDecoder->SetStream(stream);
}

void FileReader::OnNotifyAudioPacket(std::shared_ptr<IAVPacket> packet) {
    if (m_audioDecoder) m_audioDecoder->Decode(packet);
}

void FileReader::OnNotifyVideoPacket(std::shared_ptr<IAVPacket> packet) {
    if (m_videoDecoder) m_videoDecoder->Decode(packet);
}

// 继承自IAudioDecoder::Listener
void FileReader::OnNotifyAudioSamples(std::shared_ptr<IAudioSamples> audioSamples) {
    std::lock_guard<std::recursive_mutex> lock(m_listenerMutex);
    if (m_listener) m_listener->OnFileReaderNotifyAudioSamples(audioSamples);
}

// 继承自IVideoDecoder::Listener
void FileReader::OnNotifyVideoFrame(std::shared_ptr<IVideoFrame> videoFrame) {
    std::lock_guard<std::recursive_mutex> lock(m_listenerMutex);
    if (m_listener) m_listener->OnFileReaderNotifyVideoFrame(videoFrame);
}

}  // namespace av