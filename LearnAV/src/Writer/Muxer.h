//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#ifndef LEARNAV_MUXER_H
#define LEARNAV_MUXER_H

#include <atomic>
#include <condition_variable>
#include <list>
#include <mutex>
#include <thread>

#include "Interface/IMuxer.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
}

namespace av {

class Muxer : public IMuxer {
public:
    Muxer() = default;
    ~Muxer();

    //
    // 继承 IMuxer
    //
    bool Configure(const std::string& outputFilePath, FileWriterParameters& parameters, int flags) override;
    void NotifyAudioPacket(std::shared_ptr<IAVPacket> packet) override;
    void NotifyVideoPacket(std::shared_ptr<IAVPacket> packet) override;
    void NotifyAudioFinished() override;
    void NotifyVideoFinished() override;

private:
    void WriteInterleavedPackets();

private:
    struct StreamInfo {
        AVStream* avStream{nullptr};
        bool isExtraDataWritten{false};
        bool isFinished{false};
        std::list<std::shared_ptr<IAVPacket>> packetQueue;
    };

    std::mutex m_muxerMutex;
    AVFormatContext* m_formatContext{nullptr};

    StreamInfo m_audioStream;
    StreamInfo m_videoStream;
};

}  // namespace av

#endif  // LEARNAV_MUXER_H
