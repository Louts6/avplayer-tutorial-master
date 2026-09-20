//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#ifndef LEARNAV_AUDIOSPEAKER_H
#define LEARNAV_AUDIOSPEAKER_H

#include <QAudioSink>
#include <QIODevice>
#include <QMutex>
#include <QObject>
#include <QQueue>
#include <QTimer>
#include <memory>

#include "Interface/IAudioSpeaker.h"

namespace av {

class AudioSpeaker : public QObject, public IAudioSpeaker {
    Q_OBJECT

public:
    AudioSpeaker(unsigned int channels, unsigned int sampleRate);
    ~AudioSpeaker() override;

    void PlayAudioSamples(std::shared_ptr<IAudioSamples> samples) override;
    void Stop() override;

private slots:
    void OnTimer();

private:
    void WritePendingData();
    qint64 CalculateBufferSize() const;

    unsigned int m_channels;
    unsigned int m_sampleRate;
    std::unique_ptr<QAudioSink> m_audioSink;
    QIODevice* m_audioIODevice;
    QTimer* m_timer;
    QMutex m_mutex;
    QQueue<std::shared_ptr<IAudioSamples>> m_audioQueue;
    bool m_isPlaying;

    // 缓冲区大小（字节），用于控制播放速率
    // 设置为约100ms的音频数据
    static constexpr int kBufferSizeMs = 100;
    // 定时器间隔（毫秒），用于定期检查缓冲区
    static constexpr int kTimerIntervalMs = 50;
};

}  // namespace av

#endif  // LEARNAV_AUDIOSPEAKER_H
