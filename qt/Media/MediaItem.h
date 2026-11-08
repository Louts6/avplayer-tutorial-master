//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#ifndef LEARNAV_MEDIAITEM_H
#define LEARNAV_MEDIAITEM_H

#include <QString>

namespace av {

/// 媒体类型：视频 / 图片 / PDF 文档
enum class MediaType {
    kVideo = 0,
    kImage = 1,
    kPdf   = 2,
};

/// 单个媒体条目。视频按正常时长播放；图片显示 3 秒；PDF 每页显示 3 秒。
struct MediaItem {
    MediaType type{MediaType::kVideo};
    QString  path;
    int      pageCount{1};   ///< PDF 页数；图片/视频恒为 1
    float    duration{0.0f}; ///< 总时长（秒）：图片=3，PDF=pageCount*3，视频=文件时长（未知时为 0）

    /// 图片 / PDF 单页停留时长（秒）
    static constexpr float kSlideDuration = 3.0f;

    static MediaType TypeFromExtension(const QString& filePath);
    static QString   TypeName(MediaType type);
};

}  // namespace av

#endif  // LEARNAV_MEDIAITEM_H
