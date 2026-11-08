//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#ifndef LEARNAV_PLAYLISTWIDGET_H
#define LEARNAV_PLAYLISTWIDGET_H

#include <QListWidget>
#include <QPushButton>
#include <QWidget>
#include <memory>

#include "Media/MediaPlaylist.h"

class PlaylistWidget : public QWidget {
    Q_OBJECT
public:
    explicit PlaylistWidget(QWidget* parent = nullptr);

    void setPlaylist(std::shared_ptr<av::MediaPlaylist> playlist);
    int  currentIndex() const;
    void setCurrentIndex(int index);

signals:
    void playRequested(int index);
    void removeRequested(int index);

private slots:
    void onAddFiles();
    void onAddFolder();
    void onRemove();
    void onMoveUp();
    void onMoveDown();
    void onPlay();
    void onPlaylistChanged();

private:
    void refresh();

    QListWidget*                          m_list{nullptr};
    QPushButton*                          m_btnAddFiles{nullptr};
    QPushButton*                          m_btnAddFolder{nullptr};
    QPushButton*                          m_btnRemove{nullptr};
    QPushButton*                          m_btnUp{nullptr};
    QPushButton*                          m_btnDown{nullptr};
    QPushButton*                          m_btnPlay{nullptr};
    std::shared_ptr<av::MediaPlaylist>    m_playlist;
};

#endif  // LEARNAV_PLAYLISTWIDGET_H
