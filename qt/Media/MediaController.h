//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#ifndef LEARNAV_MEDIACONTROLLER_H
#define LEARNAV_MEDIACONTROLLER_H

#include <QObject>
#include <QTimer>
#include <memory>

#include "IPlayer.h"
#include "Interface/IVideoDisplayView.h"
#include "MediaPlaylist.h"

QT_BEGIN_NAMESPACE
class QPdfDocument;
QT_END_NAMESPACE

namespace av {

class OpenGLView;

/// 媒体播放调度器：按播放列表依次播放视频 / 图片 / PDF。
/// - 视频：走原有 IPlayer（CPU/GPU 解码渲染路径不变）。
/// - 图片：解码为 RGBA 后直接上屏，停留 3 秒。
/// - PDF：每页渲染为图片上屏，每页停留 3 秒。
class MediaController : public QObject {
    Q_OBJECT
public:
    MediaController(std::shared_ptr<IPlayer> player,
                    std::shared_ptr<IVideoDisplayView> display,
                    OpenGLView* glView,
                    QObject* parent = nullptr);
    ~MediaController() override;

    void setPlaylist(std::shared_ptr<MediaPlaylist> playlist);

    void play(int index = 0);
    void playNext();
    void pause();
    void resume();
    void stop();

    bool isPlaying() const { return m_playing; }
    bool isBusy() const { return m_busy; }
    int  currentIndex() const { return m_currentIndex; }

signals:
    void currentItemChanged(int index);
    void progressChanged(float current, float total);
    void playlistFinished();

private slots:
    void onSlideTick();

private:
    void playItem(int index);
    void playVideo(const MediaItem& item);
    void playImage(const MediaItem& item);
    void playPdf(const MediaItem& item);
    void showRgbaImage(const QImage& image);
    QImage renderPdfPage(int page);

    void beginSlide(float durationSec);
    void endSlide();

    std::shared_ptr<IPlayer>           m_player;
    std::shared_ptr<IVideoDisplayView> m_display;
    OpenGLView*                        m_glView{nullptr};
    std::shared_ptr<MediaPlaylist>     m_playlist;

    QTimer* m_slideTimer{nullptr};
    float   m_slideDuration{0.0f};
    float   m_slideElapsed{0.0f};

    int  m_currentIndex{-1};
    bool m_playing{false};
    bool m_paused{false};
    bool m_busy{false};  ///< 防重入：playVideo 执行期间为 true

    // PDF 播放状态
    std::unique_ptr<QPdfDocument> m_pdfDoc;
    int                           m_pdfPage{-1};

    class EofListener;
    std::shared_ptr<EofListener> m_eofListener;
};

}  // namespace av

#endif  // LEARNAV_MEDIACONTROLLER_H
