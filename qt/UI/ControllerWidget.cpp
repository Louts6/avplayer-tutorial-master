//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#include "ControllerWidget.h"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPushButton>
#include <QStandardPaths>

#include "IPlayer.h"

#define DEBUG_PATH

ControllerWidget::ControllerWidget(QWidget* parent, std::shared_ptr<av::IPlayer> player)
    : QWidget(parent), m_player(player) {
#if !defined(DEBUG_PATH)
    // 初始化导入按钮
    m_buttonImport = new QPushButton("导入", this);
    connect(m_buttonImport, &QPushButton::clicked, this, &ControllerWidget::onImportButtonClicked);
#endif

    // 初始化播放按钮
    m_buttonPlay = new QPushButton("播放", this);
    connect(m_buttonPlay, &QPushButton::clicked, this, &ControllerWidget::onPlayButtonClicked);

    // 初始化导出按钮
    m_buttonExport = new QPushButton("录制", this);
    connect(m_buttonExport, &QPushButton::clicked, this, &ControllerWidget::onExportButtonClicked);

    // 初始化视频滤镜按钮 - 垂直翻转
    m_buttonVideoFilterFlipVertical = new QPushButton("添加垂直翻转滤镜", this);
    connect(m_buttonVideoFilterFlipVertical, &QPushButton::clicked, this,
            &ControllerWidget::onVideoFilterFlipVerticalButtonClicked);

    // 初始化视频滤镜按钮 - 灰度
    m_buttonVideoFilterGray = new QPushButton("添加黑白滤镜", this);
    connect(m_buttonVideoFilterGray, &QPushButton::clicked, this, &ControllerWidget::onVideoFilterGrayButtonClicked);

    // 初始化视频滤镜按钮 - 反转
    m_buttonVideoFilterInvert = new QPushButton("添加反色滤镜", this);
    connect(m_buttonVideoFilterInvert, &QPushButton::clicked, this,
            &ControllerWidget::onVideoFilterInvertButtonClicked);

    m_buttonVideoFilterSticker = new QPushButton("添加贴纸滤镜", this);
    connect(m_buttonVideoFilterSticker, &QPushButton::clicked, this,
            &ControllerWidget::onVideoFilterStickerButtonClicked);

    // 布局设置
    auto* layout = new QHBoxLayout(this);
    if (m_buttonImport) layout->addWidget(m_buttonImport);
    layout->addWidget(m_buttonPlay);
    layout->addWidget(m_buttonExport);
    layout->addStretch();
    layout->addWidget(m_buttonVideoFilterFlipVertical);
    layout->addWidget(m_buttonVideoFilterGray);
    layout->addWidget(m_buttonVideoFilterInvert);
    layout->addWidget(m_buttonVideoFilterSticker);
    setLayout(layout);

#ifdef DEBUG_PATH
    std::string videoFilePathStd = std::string(RESOURCE_DIR) + "/video/video0.mp4";
    m_player->Open(videoFilePathStd);
#endif
}

void ControllerWidget::onImportButtonClicked() {
    if (!m_player) return;

    QString videoFilePath = QFileDialog::getOpenFileName(
        nullptr, tr("打开视频文件"), QStandardPaths::writableLocation(QStandardPaths::HomeLocation),
        "media files(*.avi *.mp4 *.mov)", nullptr, QFileDialog::DontUseNativeDialog);

    if (videoFilePath.isEmpty()) return;

    std::string videoFilePathStd = videoFilePath.toStdString();
    m_player->Open(videoFilePathStd);
}

void ControllerWidget::onPlayButtonClicked() {
    if (!m_player) return;

    if (m_player->IsPlaying()) {
        m_player->Pause();
        m_buttonPlay->setText("播放");
    } else {
        m_player->Play();
        m_buttonPlay->setText("暂停");
    }
}

void ControllerWidget::onExportButtonClicked() {
    if (!m_player) return;

    if (m_player->IsRecording()) {
        m_player->StopRecording();
        m_buttonExport->setText("录制");
    } else {
        if (!m_player->IsPlaying()) {
            QMessageBox::information(nullptr, "提示", "非播放状态下无法录制视频！");
            return;
        }

#ifdef DEBUG_PATH
        // 如果cache 目录不存在，创建cache目录
        QDir dir(CACHE_DIR);
        if (!dir.exists()) dir.mkpath(".");
        // 当前时间作为文件名
        auto fileName = std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + ".mp4";
        std::string outputPath = std::string(CACHE_DIR) + "/" + fileName;
        m_player->StartRecording(outputPath, 0);
        m_buttonExport->setText("停止");
        return;
#endif

        QString dirName = QFileDialog::getSaveFileName(
            this, tr("选择导出路径"), QStandardPaths::writableLocation(QStandardPaths::DownloadLocation));
        if (dirName.isEmpty()) {
            return;
        }
        std::string dirNameStd = dirName.toStdString();
        m_player->StartRecording(dirNameStd, 0);
        m_buttonExport->setText("停止");
    }
}

void ControllerWidget::onVideoFilterFlipVerticalButtonClicked() {
    if (!m_player) return;

    if (m_buttonVideoFilterFlipVertical->text() == "添加垂直翻转滤镜") {
        auto videoFilter = m_player->AddVideoFilter(av::VideoFilterType::kFlipVertical);
        m_buttonVideoFilterFlipVertical->setText("移除垂直翻转滤镜");
    } else {
        m_player->RemoveVideoFilter(av::VideoFilterType::kFlipVertical);
        m_buttonVideoFilterFlipVertical->setText("添加垂直翻转滤镜");
    }
}

void ControllerWidget::onVideoFilterGrayButtonClicked() {
    if (!m_player) return;

    if (m_buttonVideoFilterGray->text() == "添加黑白滤镜") {
        auto videoFilter = m_player->AddVideoFilter(av::VideoFilterType::kGray);
        m_buttonVideoFilterGray->setText("移除黑白滤镜");
    } else {
        m_player->RemoveVideoFilter(av::VideoFilterType::kGray);
        m_buttonVideoFilterGray->setText("添加黑白滤镜");
    }
}

void ControllerWidget::onVideoFilterInvertButtonClicked() {
    if (!m_player) return;

    if (m_buttonVideoFilterInvert->text() == "添加反色滤镜") {
        auto videoFilter = m_player->AddVideoFilter(av::VideoFilterType::kInvert);
        m_buttonVideoFilterInvert->setText("移除反色滤镜");
    } else {
        m_player->RemoveVideoFilter(av::VideoFilterType::kInvert);
        m_buttonVideoFilterInvert->setText("添加反色滤镜");
    }
}

void ControllerWidget::onVideoFilterStickerButtonClicked() {
    if (!m_player) return;

    if (m_buttonVideoFilterSticker->text() == "添加贴纸滤镜") {
        auto videoFilter = m_player->AddVideoFilter(av::VideoFilterType::kSticker);
        if (videoFilter) {
            videoFilter->SetString("StickerPath", std::string(RESOURCE_DIR) + "/sticker/Sticker0.png");
            videoFilter->SetString("ModelPath", std::string(RESOURCE_DIR) + "/pack/Megatron");
        }
        m_buttonVideoFilterSticker->setText("移除贴纸滤镜");
    } else {
        m_player->RemoveVideoFilter(av::VideoFilterType::kSticker);
        m_buttonVideoFilterSticker->setText("添加贴纸滤镜");
    }
}