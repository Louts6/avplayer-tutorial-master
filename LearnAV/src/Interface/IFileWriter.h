//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#ifndef LEARNAV_IFILEWRITER_H
#define LEARNAV_IFILEWRITER_H

#include "Define/FileWriterParameters.h"
#include "Define/IAudioSamples.h"
#include "Define/IVideoFrame.h"
#include "IGLContext.h"

namespace av {

struct IFileWriter {
    virtual bool StartWriter(const std::string& outputFilePath, FileWriterParameters& parameters, int flags) = 0;
    virtual void StopWriter() = 0;

    virtual void NotifyAudioSamples(std::shared_ptr<IAudioSamples> audioSamples) = 0;
    virtual void NotifyVideoFrame(std::shared_ptr<IVideoFrame> videoFrame) = 0;

    virtual void NotifyAudioFinished() = 0;
    virtual void NotifyVideoFinished() = 0;

    virtual ~IFileWriter() = default;

    static IFileWriter* Create(std::shared_ptr<GLContext> glContext);
};

}  // namespace av

#endif  // LEARNAV_IFILEWRITER_H
