//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#include "MainWindow.h"

#include <QApplication>
#include <QSplitter>
#include <QVBoxLayout>
#include <iostream>

MainWindow::PlaybackListener::PlaybackListener(MainWindow *window) : m_window(window) {}

void MainWindow::PlaybackListener::NotifyPlaybackStarted() { std::cout << "NotifyPlaybackStarted" << std::endl; }

void MainWindow::PlaybackListener::NotifyPlaybackTimeChanged(float timeStamp, float duration) {
    m_window->m_progressSlider->setValue(static_cast<int>(timeStamp / duration * 1000));
}

void MainWindow::PlaybackListener::NotifyPlaybackPaused() { std::cout << "NotifyPlaybackPaused" << std::endl; }

void MainWindow::PlaybackListener::NotifyPlaybackEOF() { std::cout << "NotifyPlaybackEOF" << std::endl; }

MainWindow::MainWindow() : QWidget() {
    // 创建共享的OpenGL上下文
    auto sharedContext = std::make_shared<av::GLContext>(QOpenGLContext::globalShareContext());
    m_player = std::shared_ptr<av::IPlayer>(av::IPlayer::Create(sharedContext));
    m_player->SetPlaybackListener(std::make_shared<PlaybackListener>(this));

    // 设置布局
    auto *vbox = new QVBoxLayout();
    vbox->setContentsMargins(0, 0, 0, 0);
    setLayout(vbox);

    // 添加播放器主界面
    m_playerWidget = new PlayerWidget(this, m_player);
    vbox->addWidget(m_playerWidget);

    // 添加进度条
    m_progressSlider = new QSlider(Qt::Horizontal, this);
    m_progressSlider->setRange(0, 1000);
    m_progressSlider->setValue(0);
    // 连接进度条的 sliderMoved 信号到自定义的槽函数
    connect(m_progressSlider, &QSlider::sliderMoved, this, &MainWindow::onSliderMoved);
    vbox->addWidget(m_progressSlider);

    // 添加播放控件
    m_controllerWidget = new ControllerWidget(this, m_player);
    vbox->addWidget(m_controllerWidget);

    // 设置窗口最小尺寸
    setMinimumWidth(1280);
    setMinimumHeight(720);
}

MainWindow::~MainWindow() = default;

void MainWindow::resizeEvent(QResizeEvent *event) {
    // 处理窗口大小调整事件
    QWidget::resizeEvent(event);
}

void MainWindow::mousePressEvent(QMouseEvent *event) {
    // 处理鼠标按下事件
    QWidget::mousePressEvent(event);
}

void MainWindow::mouseMoveEvent(QMouseEvent *event) {
    // 处理鼠标移动事件
    QWidget::mouseMoveEvent(event);
}

void MainWindow::mouseReleaseEvent(QMouseEvent *event) {
    // 处理鼠标释放事件
    QWidget::mouseReleaseEvent(event);
}

void MainWindow::mouseDoubleClickEvent(QMouseEvent *event) {
    // 处理鼠标双击事件
    QWidget::mouseDoubleClickEvent(event);
    if (windowState() == Qt::WindowMaximized) {
        showNormal();  // 如果窗口已最大化，则恢复正常大小
    } else {
        showMaximized();  // 否则，最大化窗口
    }
}

void MainWindow::onSliderMoved(int value) {
    // 处理进度条滑动事件
    m_player->SeekTo(static_cast<float>(value) / 1000);
}