//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#include "AudioSpeaker.h"

#include <QAudioDevice>
#include <QAudioFormat>
#include <QMediaDevices>
#include <algorithm>
#include <iostream>

#include "Log.h"

namespace av {

AudioSpeaker::AudioSpeaker(unsigned int channels, unsigned int sampleRate)
    : m_channels(channels), m_sampleRate(sampleRate), m_audioIODevice(nullptr), m_timer(nullptr), m_isPlaying(false) {
    // 设置音频格式
    QAudioFormat format;
    format.setSampleRate(static_cast<int>(sampleRate));
    format.setChannelCount(static_cast<int>(channels));
    format.setSampleFormat(QAudioFormat::Int16);

    // 获取默认音频输出设备
    QMediaDevices devices;
    QAudioDevice device = devices.defaultAudioOutput();

    if (!device.isFormatSupported(format)) {
        LOGE("AudioSpeaker", "Audio format not supported, using nearest supported format.");
        format = device.preferredFormat();
    }

    // 计算并设置缓冲区大小（约100ms的音频数据）
    qint64 bufferSize = CalculateBufferSize();

    // 创建音频输出设备
    m_audioSink = std::make_unique<QAudioSink>(device, format);
    m_audioSink->setBufferSize(static_cast<int>(bufferSize));

    m_audioIODevice = m_audioSink->start();

    if (!m_audioIODevice) {
        LOGE("AudioSpeaker", "Failed to start audio output device.");
    } else {
        m_isPlaying = true;

        // 创建定时器，定期检查缓冲区并写入数据
        m_timer = new QTimer(this);
        connect(m_timer, &QTimer::timeout, this, &AudioSpeaker::OnTimer);
        m_timer->start(kTimerIntervalMs);
    }
}

AudioSpeaker::~AudioSpeaker() {
    Stop();
    if (m_audioSink) {
        m_audioSink->stop();
    }
}

qint64 AudioSpeaker::CalculateBufferSize() const {
    // 计算100ms音频数据所需的字节数
    // 采样率 * 通道数 * 每样本字节数 * 时间(秒)
    qint64 bytesPerSecond = static_cast<qint64>(m_sampleRate) * m_channels * sizeof(int16_t);
    qint64 bufferSize = bytesPerSecond * kBufferSizeMs / 1000;
    return bufferSize;
}

void AudioSpeaker::PlayAudioSamples(std::shared_ptr<IAudioSamples> samples) {
    if (!samples || !m_isPlaying) {
        return;
    }

    QMutexLocker locker(&m_mutex);

    // 将音频样本添加到队列
    m_audioQueue.enqueue(samples);

    // 尝试写入待处理的数据
    WritePendingData();
}

void AudioSpeaker::WritePendingData() {
    if (!m_audioIODevice || !m_isPlaying || !m_audioSink) {
        return;
    }

    // 从队列中取出数据并写入，直到缓冲区满或队列为空
    while (!m_audioQueue.isEmpty()) {
        // 实时检查缓冲区可用空间
        qint64 bytesFree = m_audioSink->bytesFree();
        if (bytesFree <= 0) {
            // 缓冲区已满，等待下次调用
            break;
        }

        auto samples = m_audioQueue.head();
        if (!samples) {
            m_audioQueue.dequeue();
            continue;
        }

        const int16_t* data = samples->pcmData.data() + samples->offset;
        size_t remainingSamples = samples->pcmData.size() - samples->offset;
        size_t dataSize = remainingSamples * sizeof(int16_t);

        if (dataSize == 0) {
            // 这个样本已经处理完，移除它
            m_audioQueue.dequeue();
            continue;
        }

        // 计算可以写入的字节数（不超过可用空间）
        qint64 bytesToWrite = std::min(static_cast<qint64>(dataSize), bytesFree);

        // 写入数据
        qint64 bytesWritten = m_audioIODevice->write(reinterpret_cast<const char*>(data), bytesToWrite);

        if (bytesWritten < 0) {
            LOGE("AudioSpeaker", "Failed to write audio data to device.");
            m_audioQueue.dequeue();
            break;
        }

        if (bytesWritten == 0) {
            // 无法写入更多数据，退出循环
            break;
        }

        // 更新已写入的字节数
        size_t samplesWritten = bytesWritten / sizeof(int16_t);
        samples->offset += samplesWritten;

        // 如果这个样本的所有数据都已写入，从队列中移除
        if (samples->offset >= samples->pcmData.size()) {
            m_audioQueue.dequeue();
        }
    }
}

void AudioSpeaker::OnTimer() {
    // 定时器触发时，检查缓冲区并写入待处理的数据
    QMutexLocker locker(&m_mutex);
    WritePendingData();
}

void AudioSpeaker::Stop() {
    QMutexLocker locker(&m_mutex);
    m_isPlaying = false;

    if (m_timer) {
        m_timer->stop();
        m_timer->disconnect();
    }

    if (m_audioSink) {
        m_audioSink->stop();
    }

    m_audioQueue.clear();
    m_audioIODevice = nullptr;
}

}  // namespace av
