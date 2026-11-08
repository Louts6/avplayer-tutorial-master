//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#ifndef LEARNAV_MAINWINDOW_H
#define LEARNAV_MAINWINDOW_H

#ifdef _WIN32
#include <glad/glad.h>
#endif
#include <QDockWidget>
#include <QLabel>
#include <QMainWindow>
#include <QOpenGLContext>
#include <QSlider>
#include <QSplitter>

#include "IPlayer.h"
#include "Media/MediaController.h"
#include "Media/MediaPlaylist.h"
#include "UI/ControllerWidget.h"
#include "UI/PlayerWidget.h"
#include "UI/PlaylistWidget.h"

class MainWindow : public QWidget {
    Q_OBJECT
public:
    MainWindow();
    ~MainWindow() override;

protected:
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private slots:
    void onSliderMoved(int value);
    void onPlaylistPlayRequested(int index);
    void onControllerCurrentChanged(int index);
    void onControllerProgress(float current, float total);
    void onPlaylistChanged();

private:
    PlayerWidget     *m_playerWidget{nullptr};
    QSlider          *m_progressSlider{nullptr};
    ControllerWidget *m_controllerWidget{nullptr};
    PlaylistWidget   *m_playlistWidget{nullptr};

    std::shared_ptr<av::IPlayer>        m_player;
    std::shared_ptr<av::MediaPlaylist>  m_playlist;
    std::shared_ptr<av::MediaController> m_controller;

    QString m_playlistXmlPath;
};

#endif  // LEARNAV_MAINWINDOW_H
