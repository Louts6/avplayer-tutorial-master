//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#include "MediaController.h"

#include <QImage>
#include <QPainter>
#include <QPdfDocument>
#include <QOpenGLWidget>

#include "OpenGLView.h"
#include "Log.h"

static const char* kMediaTag = "MediaController";

namespace av {

/// 监听播放器 EOF，用于视频播放结束后自动切换到下一条。
class MediaController::EofListener : public IPlaybackListener {
public:
    explicit EofListener(MediaController* owner) : m_owner(owner) {}
    void NotifyPlaybackStarted() override {}
    void NotifyPlaybackTimeChanged(float timeStamp, float duration) override {
        if (m_owner) emit m_owner->progressChanged(timeStamp, duration);
    }
    void NotifyPlaybackPaused() override {}
    void NotifyPlaybackEOF() override {
        // 仅在正常播放中且非切换媒体时才切到下一条；
        // 用户主动停止(m_playing=false)或正在切源(m_busy=true)时忽略。
        if (m_owner && m_owner->isPlaying() && !m_owner->isBusy()) {
            QMetaObject::invokeMethod(m_owner, [owner = m_owner]() {
                if (owner->isPlaying() && !owner->isBusy()) owner->playNext();
            }, Qt::QueuedConnection);
        }
    }

private:
    MediaController* m_owner{nullptr};
};

MediaController::MediaController(std::shared_ptr<IPlayer> player,
                                 std::shared_ptr<IVideoDisplayView> display,
                                 OpenGLView* glView,
                                 QObject* parent)
    : QObject(parent), m_player(std::move(player)), m_display(std::move(display)), m_glView(glView) {
    m_slideTimer = new QTimer(this);
    m_slideTimer->setInterval(100);
    connect(m_slideTimer, &QTimer::timeout, this, &MediaController::onSlideTick);

    m_eofListener = std::make_shared<EofListener>(this);
    if (m_player) m_player->SetPlaybackListener(m_eofListener);
}

MediaController::~MediaController() { stop(); }

void MediaController::setPlaylist(std::shared_ptr<MediaPlaylist> playlist) { m_playlist = std::move(playlist); }

void MediaController::play(int index) {
    // 防重入：若上一次 playVideo 尚未完成，先等待其结束
    if (m_busy) return;
    playItem(index);
}

void MediaController::playItem(int index) {
    if (!m_playlist || index < 0 || index >= m_playlist->count()) {
        m_playing = false;
        emit playlistFinished();
        return;
    }
    m_currentIndex = index;
    m_playing      = true;
    m_paused       = false;
    emit currentItemChanged(index);

    const MediaItem& item = m_playlist->at(index);
    LOGI(kMediaTag, "playItem index=%d type=%d path=%s", index, static_cast<int>(item.type),
         item.path.toUtf8().constData());
    switch (item.type) {
        case MediaType::kVideo: playVideo(item); break;
        case MediaType::kImage: playImage(item); break;
        case MediaType::kPdf:   playPdf(item); break;
    }
}

void MediaController::playVideo(const MediaItem& item) {
    endSlide();
    if (!m_player) {
        playNext();
        return;
    }
    // 防重入：标记 busy，Stop→Open→Play 期间忽略新的 play 请求
    m_busy = true;
    // 切换到新视频前先停止上一条的播放状态，保证 demuxer / 解码器干净重启
    m_player->Stop();
    std::string path = item.path.toStdString();
    bool ok = m_player->Open(path);
    LOGI(kMediaTag, "playVideo Open=%d path=%s", ok ? 1 : 0, path.c_str());
    if (ok) {
        m_player->Play();
    } else {
        LOGE(kMediaTag, "Failed to open video: %s", path.c_str());
        m_busy = false;
        playNext();
        return;
    }
    m_busy = false;
}

void MediaController::playImage(const MediaItem& item) {
    if (m_player) m_player->Stop();
    QImage img(item.path);
    if (img.isNull()) {
        LOGE(kMediaTag, "Failed to load image: %s", item.path.toUtf8().constData());
        playNext();
        return;
    }
    showRgbaImage(img);
    beginSlide(MediaItem::kSlideDuration);
}

void MediaController::playPdf(const MediaItem& item) {
    if (m_player) m_player->Stop();
    m_pdfDoc = std::make_unique<QPdfDocument>();
    auto status = m_pdfDoc->load(item.path);
    if (status != QPdfDocument::Error::None) {
        LOGE(kMediaTag, "Failed to load pdf: %s", item.path.toUtf8().constData());
        m_pdfDoc.reset();
        playNext();
        return;
    }
    m_pdfPage = 0;
    // 渲染第一页
    QImage page = renderPdfPage(m_pdfPage);
    if (page.isNull()) {
        playNext();
        return;
    }
    showRgbaImage(page);
    beginSlide(MediaItem::kSlideDuration);
}

void MediaController::showRgbaImage(const QImage& image) {
    if (!m_display) return;
    // QImage 像素行序从上到下，OpenGL 纹理坐标原点在左下角，
    // 直接上传会导致画面上下颠倒，这里先垂直翻转。
    QImage rgba = image.convertToFormat(QImage::Format_RGBA8888).mirrored(false, true);
    auto frame  = std::make_shared<IVideoFrame>();
    frame->width  = rgba.width();
    frame->height = rgba.height();
    const size_t bytes = static_cast<size_t>(rgba.sizeInBytes());
    auto* buf = new uint8_t[bytes];
    std::memcpy(buf, rgba.constBits(), bytes);
    frame->data = std::shared_ptr<uint8_t>(buf, std::default_delete<uint8_t[]>());

    // 显示帧由 GUI 线程提交，需要让 QOpenGLWidget 的 GL 上下文当前，
    // 以保证上一帧纹理在析构时能被正确释放。
    if (m_glView) m_glView->makeCurrent();
    m_display->Render(frame, IVideoDisplayView::EContentMode::kScaleAspectFit);
    if (m_glView) m_glView->doneCurrent();
}

QImage MediaController::renderPdfPage(int page) {
    if (!m_pdfDoc) return QImage();
    QSizeF pts = m_pdfDoc->pagePointSize(page);
    if (pts.isEmpty()) return QImage();
    // 以宽度 1280 为基准等比缩放，保证清晰度
    const qreal targetWidth = 1280.0;
    qreal scale = targetWidth / pts.width();
    QSize renderSize = (pts * scale).toSize();
    QImage pageImg = m_pdfDoc->render(page, renderSize);
    if (pageImg.isNull()) return QImage();

    // Qt 的 QPdfDocument::render 输出带透明背景（alpha=0, RGB=0），
    // 直接上屏会显示为黑色。这里合成到白底上，保证 PDF 内容正常可见。
    QImage result(renderSize, QImage::Format_RGBA8888);
    result.fill(Qt::white);
    {
        QPainter painter(&result);
        painter.drawImage(0, 0, pageImg);
    }
    return result;
}

void MediaController::beginSlide(float durationSec) {
    m_slideDuration = durationSec;
    m_slideElapsed  = 0.0f;
    emit progressChanged(0.0f, durationSec);
    m_slideTimer->start();
}

void MediaController::endSlide() {
    m_slideTimer->stop();
    m_slideElapsed  = 0.0f;
    m_slideDuration = 0.0f;
    if (m_pdfDoc) {
        m_pdfDoc.reset();
        m_pdfPage = -1;
    }
}

void MediaController::onSlideTick() {
    if (!m_playing || m_paused) return;
    m_slideElapsed += m_slideTimer->interval() / 1000.0f;
    emit progressChanged(m_slideElapsed, m_slideDuration);
    if (m_slideElapsed < m_slideDuration) return;

    // 当前条目播放完毕
    if (!m_playlist) return;
    const MediaItem& item = m_playlist->at(m_currentIndex);
    if (item.type == MediaType::kPdf && m_pdfDoc && m_pdfPage + 1 < m_pdfDoc->pageCount()) {
        // 翻到 PDF 下一页
        ++m_pdfPage;
        QImage page = renderPdfPage(m_pdfPage);
        if (!page.isNull()) {
            showRgbaImage(page);
            m_slideElapsed = 0.0f;
            emit progressChanged(0.0f, m_slideDuration);
            return;
        }
    }
    playNext();
}

void MediaController::playNext() {
    // 若用户已主动停止，则不再自动切到下一条
    if (!m_playing) return;
    endSlide();
    if (!m_playlist || m_playlist->count() == 0) {
        m_playing = false;
        return;
    }
    // 循环播放：最后一条播完后回到第一条
    int nextIndex = (m_currentIndex + 1) % m_playlist->count();
    playItem(nextIndex);
}

void MediaController::pause() {
    if (!m_playing) return;
    m_paused = true;
    if (m_player && m_playlist && m_playlist->at(m_currentIndex).type == MediaType::kVideo) {
        m_player->Pause();
    } else {
        m_slideTimer->stop();
    }
}

void MediaController::resume() {
    if (!m_playing || !m_paused) return;
    m_paused = false;
    if (m_player && m_playlist && m_playlist->at(m_currentIndex).type == MediaType::kVideo) {
        m_player->Play();
    } else {
        m_slideTimer->start();
    }
}

void MediaController::stop() {
    // 先标记已停止，确保 Stop 过程中触发的 EOF 回调不会再启动下一条
    m_playing = false;
    m_paused  = false;
    m_busy    = false;
    endSlide();
    if (m_player) m_player->Stop();
}

}  // namespace av
