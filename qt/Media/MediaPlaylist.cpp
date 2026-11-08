//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#include "MediaPlaylist.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include <QPdfDocument>

namespace av {

MediaPlaylist::MediaPlaylist(QObject* parent) : QObject(parent) {}

bool MediaPlaylist::isSupported(const QString& filePath) {
    const QString suffix = QFileInfo(filePath).suffix().toLower();
    static const QStringList exts = {"mp4", "avi", "mov", "mkv", "flv", "wmv", "ts", "m4v", "webm",
                                     "jpg", "jpeg", "png", "bmp", "gif", "webp", "tif", "tiff", "pdf"};
    return exts.contains(suffix);
}

void MediaPlaylist::normalize(MediaItem& item) {
    item.type = MediaItem::TypeFromExtension(item.path);
    if (item.type == MediaType::kPdf) {
        if (item.pageCount <= 0) {
            QPdfDocument doc;
            if (doc.load(item.path) == QPdfDocument::Error::None) {
                item.pageCount = doc.pageCount();
            } else {
                item.pageCount = 1;
            }
        }
        item.duration = item.pageCount * MediaItem::kSlideDuration;
    } else if (item.type == MediaType::kImage) {
        item.pageCount = 1;
        item.duration  = MediaItem::kSlideDuration;
    } else {
        item.pageCount = 1;
        item.duration  = 0.0f;  // 视频时长由播放器提供
    }
}

void MediaPlaylist::addItem(const QString& filePath) {
    if (!QFileInfo::exists(filePath) || !isSupported(filePath)) return;
    MediaItem item;
    item.path = filePath;
    normalize(item);
    m_items.append(item);
    emit changed();
}

void MediaPlaylist::addItems(const QStringList& filePaths) {
    bool added = false;
    for (const QString& p : filePaths) {
        if (QFileInfo::exists(p) && isSupported(p)) {
            MediaItem item;
            item.path = p;
            normalize(item);
            m_items.append(item);
            added = true;
        }
    }
    if (added) emit changed();
}

void MediaPlaylist::rebuildFromDirectory(const QString& dirPath) {
    QDir dir(dirPath);
    if (!dir.exists()) return;
    QStringList filters;
    for (const QString& ext :
         QStringList{"*.mp4", "*.avi", "*.mov", "*.mkv", "*.flv", "*.wmv", "*.ts", "*.m4v", "*.webm",
                     "*.jpg", "*.jpeg", "*.png", "*.bmp", "*.gif", "*.webp", "*.tif", "*.tiff", "*.pdf"}) {
        filters << ext;
    }
    QFileInfoList entries = dir.entryInfoList(filters, QDir::Files, QDir::Name);
    m_items.clear();
    for (const QFileInfo& fi : entries) {
        MediaItem item;
        item.path = fi.absoluteFilePath();
        normalize(item);
        m_items.append(item);
    }
    emit changed();
}

void MediaPlaylist::removeAt(int index) {
    if (index < 0 || index >= m_items.size()) return;
    m_items.removeAt(index);
    emit changed();
}

void MediaPlaylist::clear() {
    if (m_items.isEmpty()) return;
    m_items.clear();
    emit changed();
}

bool MediaPlaylist::moveUp(int index) {
    if (index <= 0 || index >= m_items.size()) return false;
    m_items.swapItemsAt(index, index - 1);
    emit changed();
    return true;
}

bool MediaPlaylist::moveDown(int index) {
    if (index < 0 || index >= m_items.size() - 1) return false;
    m_items.swapItemsAt(index, index + 1);
    emit changed();
    return true;
}

bool MediaPlaylist::saveToXml(const QString& filePath) const {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    QXmlStreamWriter writer(&file);
    writer.setAutoFormatting(true);
    writer.writeStartDocument();
    writer.writeStartElement("Playlist");
    for (const MediaItem& item : m_items) {
        writer.writeStartElement("Item");
        writer.writeAttribute("type", QString::number(static_cast<int>(item.type)));
        writer.writeAttribute("path", item.path);
        writer.writeAttribute("pages", QString::number(item.pageCount));
        writer.writeEndElement();
    }
    writer.writeEndElement();
    writer.writeEndDocument();
    return true;
}

bool MediaPlaylist::loadFromXml(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return false;
    QXmlStreamReader reader(&file);
    QVector<MediaItem> loaded;
    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isStartElement() && reader.name() == QStringLiteral("Item")) {
            const auto attrs = reader.attributes();
            MediaItem item;
            item.type = static_cast<MediaType>(attrs.value("type").toInt());
            item.path = attrs.value("path").toString();
            item.pageCount = attrs.value("pages").toInt();
            if (QFileInfo::exists(item.path) && isSupported(item.path)) {
                normalize(item);
                loaded.append(item);
            }
        }
    }
    if (reader.hasError()) return false;
    m_items = loaded;
    emit changed();
    return true;
}

QStringList MediaPlaylist::missingPaths() const {
    QStringList missing;
    for (const MediaItem& item : m_items) {
        if (!QFileInfo::exists(item.path)) missing << item.path;
    }
    return missing;
}

}  // namespace av
