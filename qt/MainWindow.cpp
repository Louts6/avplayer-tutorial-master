//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#include "MainWindow.h"

#include <QApplication>
#include <QDir>
#include <QSplitter>
#include <QVBoxLayout>
#include <iostream>

#include "OpenGLView.h"

MainWindow::MainWindow() : QWidget() {
    // 创建共享的OpenGL上下文
    auto sharedContext = std::make_shared<av::GLContext>(QOpenGLContext::globalShareContext());
    m_player = std::shared_ptr<av::IPlayer>(av::IPlayer::Create(sharedContext));

    // 播放列表（XML 索引）
    m_playlistXmlPath = QDir::currentPath() + "/playlist.xml";
    m_playlist = std::make_shared<av::MediaPlaylist>();
    // 启动时从 XML 恢复；若文件不存在则保持空列表，等待用户添加
    m_playlist->loadFromXml(m_playlistXmlPath);

    // 设置布局
    auto *vbox = new QVBoxLayout();
    vbox->setContentsMargins(0, 0, 0, 0);
    setLayout(vbox);

    // 左右分栏：左侧播放器，右侧播放列表
    auto *hbox = new QHBoxLayout();

    // 播放器主界面
    m_playerWidget = new PlayerWidget(this, m_player);
    hbox->addWidget(m_playerWidget, 4);

    // 播放列表面板
    m_playlistWidget = new PlaylistWidget(this);
    m_playlistWidget->setPlaylist(m_playlist);
    hbox->addWidget(m_playlistWidget, 1);

    vbox->addLayout(hbox, 1);

    // 进度条
    m_progressSlider = new QSlider(Qt::Horizontal, this);
    m_progressSlider->setRange(0, 1000);
    m_progressSlider->setValue(0);
    connect(m_progressSlider, &QSlider::sliderMoved, this, &MainWindow::onSliderMoved);
    vbox->addWidget(m_progressSlider);

    // 播放控件
    m_controllerWidget = new ControllerWidget(this, m_player);
    vbox->addWidget(m_controllerWidget);

    // 媒体播放调度器：统一编排视频 / 图片 / PDF
    m_controller = std::make_shared<av::MediaController>(
        m_player, m_playerWidget->openGLView()->GetVideoDisplayView(), m_playerWidget->openGLView(), this);
    m_controller->setPlaylist(m_playlist);
    m_controllerWidget->setMediaController(m_controller.get());

    connect(m_playlistWidget, &PlaylistWidget::playRequested, this, &MainWindow::onPlaylistPlayRequested);
    connect(m_controller.get(), &av::MediaController::currentItemChanged, this, &MainWindow::onControllerCurrentChanged);
    connect(m_controller.get(), &av::MediaController::progressChanged, this, &MainWindow::onControllerProgress);
    connect(m_controller.get(), &av::MediaController::playlistFinished, this, [this]() {
        m_controllerWidget->findChild<QPushButton*>()->setText(QStringLiteral("播放"));
    });
    connect(m_playlist.get(), &av::MediaPlaylist::changed, this, &MainWindow::onPlaylistChanged);

    // 启动后自动播放第一个条目，便于验证视频/图片/PDF 切换
    QTimer::singleShot(1200, this, [this]() { onPlaylistPlayRequested(0); });

    setMinimumWidth(1280);
    setMinimumHeight(720);
}

MainWindow::~MainWindow() {
    // 退出前持久化播放列表索引
    if (m_playlist) m_playlist->saveToXml(m_playlistXmlPath);
}

void MainWindow::resizeEvent(QResizeEvent *event) { QWidget::resizeEvent(event); }
void MainWindow::mousePressEvent(QMouseEvent *event) { QWidget::mousePressEvent(event); }
void MainWindow::mouseMoveEvent(QMouseEvent *event) { QWidget::mouseMoveEvent(event); }
void MainWindow::mouseReleaseEvent(QMouseEvent *event) { QWidget::mouseReleaseEvent(event); }

void MainWindow::mouseDoubleClickEvent(QMouseEvent *event) {
    QWidget::mouseDoubleClickEvent(event);
    if (windowState() == Qt::WindowMaximized) {
        showNormal();
    } else {
        showMaximized();
    }
}

void MainWindow::onSliderMoved(int value) {
    // 仅视频支持 seek；图片/PDF 的进度由定时器驱动，不可拖动
    if (m_controller && m_playlist) {
        int idx = m_controller->currentIndex();
        if (idx >= 0 && m_playlist->at(idx).type == av::MediaType::kVideo) {
            m_player->SeekTo(static_cast<float>(value) / 1000);
        }
    }
}

void MainWindow::onPlaylistPlayRequested(int index) {
    if (m_controller) {
        m_controller->play(index);
        m_controllerWidget->findChild<QPushButton*>()->setText(QStringLiteral("暂停"));
    }
}

void MainWindow::onControllerCurrentChanged(int index) {
    if (m_playlistWidget) m_playlistWidget->setCurrentIndex(index);
}

void MainWindow::onControllerProgress(float current, float total) {
    if (total > 0.0f) {
        m_progressSlider->setValue(static_cast<int>(current / total * 1000));
    } else {
        m_progressSlider->setValue(0);
    }
}

void MainWindow::onPlaylistChanged() {
    // 列表变化时自动保存 XML 索引，支持目录重建后恢复
    if (m_playlist) m_playlist->saveToXml(m_playlistXmlPath);
}
