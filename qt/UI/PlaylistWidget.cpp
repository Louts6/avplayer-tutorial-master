//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#include "PlaylistWidget.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QVBoxLayout>

PlaylistWidget::PlaylistWidget(QWidget* parent) : QWidget(parent) {
    m_list = new QListWidget(this);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);

    m_btnAddFiles  = new QPushButton(QStringLiteral("添加文件"), this);
    m_btnAddFolder = new QPushButton(QStringLiteral("添加目录"), this);
    m_btnRemove    = new QPushButton(QStringLiteral("移除"), this);
    m_btnUp        = new QPushButton(QStringLiteral("上移"), this);
    m_btnDown      = new QPushButton(QStringLiteral("下移"), this);
    m_btnPlay      = new QPushButton(QStringLiteral("播放"), this);

    connect(m_btnAddFiles,  &QPushButton::clicked, this, &PlaylistWidget::onAddFiles);
    connect(m_btnAddFolder, &QPushButton::clicked, this, &PlaylistWidget::onAddFolder);
    connect(m_btnRemove,    &QPushButton::clicked, this, &PlaylistWidget::onRemove);
    connect(m_btnUp,        &QPushButton::clicked, this, &PlaylistWidget::onMoveUp);
    connect(m_btnDown,      &QPushButton::clicked, this, &PlaylistWidget::onMoveDown);
    connect(m_btnPlay,      &QPushButton::clicked, this, &PlaylistWidget::onPlay);
    connect(m_list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem*) { onPlay(); });

    auto* btnRow1 = new QHBoxLayout();
    btnRow1->addWidget(m_btnAddFiles);
    btnRow1->addWidget(m_btnAddFolder);
    btnRow1->addWidget(m_btnRemove);

    auto* btnRow2 = new QHBoxLayout();
    btnRow2->addWidget(m_btnUp);
    btnRow2->addWidget(m_btnDown);
    btnRow2->addWidget(m_btnPlay);

    auto* vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(4, 4, 4, 4);
    vbox->addWidget(m_list, 1);
    vbox->addLayout(btnRow1);
    vbox->addLayout(btnRow2);
}

void PlaylistWidget::setPlaylist(std::shared_ptr<av::MediaPlaylist> playlist) {
    if (m_playlist) disconnect(m_playlist.get(), &av::MediaPlaylist::changed, this, &PlaylistWidget::onPlaylistChanged);
    m_playlist = std::move(playlist);
    if (m_playlist) connect(m_playlist.get(), &av::MediaPlaylist::changed, this, &PlaylistWidget::onPlaylistChanged);
    refresh();
}

void PlaylistWidget::refresh() {
    m_list->clear();
    if (!m_playlist) return;
    for (int i = 0; i < m_playlist->count(); ++i) {
        const auto& item = m_playlist->at(i);
        QString label = QStringLiteral("[%1] %2")
                            .arg(av::MediaItem::TypeName(item.type))
                            .arg(QFileInfo(item.path).fileName());
        if (item.type == av::MediaType::kPdf) label += QStringLiteral(" (%1页)").arg(item.pageCount);
        m_list->addItem(label);
    }
}

int PlaylistWidget::currentIndex() const { return m_list->currentRow(); }

void PlaylistWidget::setCurrentIndex(int index) {
    if (index >= 0 && index < m_list->count()) m_list->setCurrentRow(index);
}

void PlaylistWidget::onPlaylistChanged() { refresh(); }

void PlaylistWidget::onAddFiles() {
    if (!m_playlist) return;
    QStringList files = QFileDialog::getOpenFileNames(
        this, QStringLiteral("选择媒体文件"), QString(),
        QStringLiteral("媒体文件 (*.mp4 *.avi *.mov *.mkv *.flv *.wmv *.ts *.m4v *.webm "
                       "*.jpg *.jpeg *.png *.bmp *.gif *.webp *.tif *.tiff *.pdf)"));
    if (!files.isEmpty()) m_playlist->addItems(files);
}

void PlaylistWidget::onAddFolder() {
    if (!m_playlist) return;
    QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("选择媒体目录"));
    if (!dir.isEmpty()) m_playlist->rebuildFromDirectory(dir);
}

void PlaylistWidget::onRemove() {
    if (!m_playlist) return;
    int row = currentIndex();
    if (row >= 0) m_playlist->removeAt(row);
}

void PlaylistWidget::onMoveUp() {
    if (m_playlist) m_playlist->moveUp(currentIndex());
}

void PlaylistWidget::onMoveDown() {
    if (m_playlist) m_playlist->moveDown(currentIndex());
}

void PlaylistWidget::onPlay() {
    int row = currentIndex();
    if (row >= 0) emit playRequested(row);
}
