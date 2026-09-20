//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#ifndef LEARNAV_FILEWRITERPARAMETERS_H
#define LEARNAV_FILEWRITERPARAMETERS_H

namespace av {

struct FileWriterParameters {
    // 音频相关参数
    int sampleRate{44100};  ///< 采样率，默认值为44100 Hz
    int channels{2};        ///< 通道数，默认值为2（立体声）

    // 视频相关参数
    int width{720};    ///< 视频宽度，默认值为720像素
    int height{1280};  ///< 视频高度，默认值为1280像素
    int fps{30};       ///< 帧率，默认值为30帧每秒
};

}  // namespace av

#endif  // LEARNAV_FILEWRITERPARAMETERS_H
