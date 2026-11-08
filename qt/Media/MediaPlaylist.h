//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#ifndef LEARNAV_MEDIAPLAYLIST_H
#define LEARNAV_MEDIAPLAYLIST_H

#include <QObject>
#include <QString>
#include <QVector>

#include "MediaItem.h"

namespace av {

/// 媒体播放列表：以 XML 作为文件索引，支持目录重建、增删、排序。
class MediaPlaylist : public QObject {
    Q_OBJECT
public:
    explicit MediaPlaylist(QObject* parent = nullptr);

    const QVector<MediaItem>& items() const { return m_items; }
    int  count() const { return m_items.size(); }
    const MediaItem& at(int index) const { return m_items.at(index); }

    /// 添加单个文件
    void addItem(const QString& filePath);
    /// 批量添加文件
    void addItems(const QStringList& filePaths);
    /// 从目录重建（扫描全部受支持文件），并清空原有列表
    void rebuildFromDirectory(const QString& dirPath);

    void removeAt(int index);
    void clear();

    /// 上移 / 下移（排序）
    bool moveUp(int index);
    bool moveDown(int index);

    /// 以 XML 保存到 filePath
    bool saveToXml(const QString& filePath) const;
    /// 从 XML 加载（不清空无效项时会保留原列表并追加有效项）
    bool loadFromXml(const QString& filePath);

    /// 当前列表所有项的路径是否都存在
    QStringList missingPaths() const;

signals:
    void changed();  ///< 列表内容发生变化（增删排序）

private:
    static bool isSupported(const QString& filePath);
    void normalize(MediaItem& item);

    QVector<MediaItem> m_items;
};

}  // namespace av

#endif  // LEARNAV_MEDIAPLAYLIST_H
