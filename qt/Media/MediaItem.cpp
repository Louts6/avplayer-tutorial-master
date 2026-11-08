//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#include "MediaItem.h"

#include <QFileInfo>

namespace av {

MediaType MediaItem::TypeFromExtension(const QString& filePath) {
    const QString suffix = QFileInfo(filePath).suffix().toLower();
    static const QStringList videoExts = {"mp4", "avi", "mov", "mkv", "flv", "wmv", "ts", "m4v", "webm"};
    static const QStringList imageExts = {"jpg", "jpeg", "png", "bmp", "gif", "webp", "tif", "tiff"};
    if (videoExts.contains(suffix)) return MediaType::kVideo;
    if (imageExts.contains(suffix)) return MediaType::kImage;
    if (suffix == "pdf") return MediaType::kPdf;
    return MediaType::kVideo;  // 未知按视频尝试
}

QString MediaItem::TypeName(MediaType type) {
    switch (type) {
        case MediaType::kVideo: return QStringLiteral("视频");
        case MediaType::kImage: return QStringLiteral("图片");
        case MediaType::kPdf:   return QStringLiteral("PDF");
    }
    return QStringLiteral("未知");
}

}  // namespace av
