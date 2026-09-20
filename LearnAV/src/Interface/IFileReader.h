//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#ifndef LEARNAV_IFILEREADER_H
#define LEARNAV_IFILEREADER_H

#include "Define/IAudioSamples.h"
#include "Define/IVideoFrame.h"

namespace av {

struct IFileReader {
    struct Listener {
        virtual void OnFileReaderNotifyAudioSamples(std::shared_ptr<IAudioSamples>) = 0;
        virtual void OnFileReaderNotifyVideoFrame(std::shared_ptr<IVideoFrame>) = 0;

        virtual void OnFileReaderNotifyAudioFinished() = 0;
        virtual void OnFileReaderNotifyVideoFinished() = 0;
        virtual ~Listener() = default;
    };

    virtual void SetListener(Listener* listener) = 0;

    virtual bool Open(std::string& filePath) = 0;
    virtual void Start() = 0;
    virtual void Pause() = 0;
    virtual void SeekTo(float progress) = 0;
    virtual void Stop() = 0;

    virtual float GetDuration() = 0;
    virtual int GetVideoWidth() = 0;
    virtual int GetVideoHeight() = 0;

    virtual ~IFileReader() = default;

    static IFileReader* Create();
};

}  // namespace av

#endif  // LEARNAV_IFILEREADER_H
