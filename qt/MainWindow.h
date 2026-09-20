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
#include "UI/ControllerWidget.h"
#include "UI/PlayerWidget.h"

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

private:
    class PlaybackListener : public av::IPlaybackListener {
    public:
        explicit PlaybackListener(MainWindow *window);
        void NotifyPlaybackStarted() override;
        void NotifyPlaybackTimeChanged(float timeStamp, float duration) override;
        void NotifyPlaybackPaused() override;
        void NotifyPlaybackEOF() override;

    private:
        MainWindow *m_window{nullptr};
    };

    PlayerWidget *m_playerWidget{nullptr};
    QSlider *m_progressSlider{nullptr};
    ControllerWidget *m_controllerWidget{nullptr};
    std::shared_ptr<av::IPlayer> m_player;
};

#endif  // LEARNAV_MAINWINDOW_H
