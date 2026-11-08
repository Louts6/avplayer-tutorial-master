//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#ifndef LEARNAV_CONTROLLERWIDGET_H
#define LEARNAV_CONTROLLERWIDGET_H

#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QWidget>

namespace av {
struct IPlayer;
class MediaController;
}  // namespace av

class ControllerWidget : public QWidget {
    Q_OBJECT
public:
    explicit ControllerWidget(QWidget* parent, std::shared_ptr<av::IPlayer> player);
    ~ControllerWidget() override = default;

    void setMediaController(av::MediaController* controller) { m_controller = controller; }

private slots:
    void onImportButtonClicked();
    void onPlayButtonClicked();
    void onExportButtonClicked();
    void onVideoFilterFlipVerticalButtonClicked();
    void onVideoFilterGrayButtonClicked();
    void onVideoFilterInvertButtonClicked();
    void onVideoFilterStickerButtonClicked();
    void onGpuCheckBoxToggled(bool checked);

private:
    QPushButton* m_buttonImport{nullptr};
    QPushButton* m_buttonPlay{nullptr};
    QPushButton* m_buttonExport{nullptr};
    QPushButton* m_buttonVideoFilterFlipVertical{nullptr};
    QPushButton* m_buttonVideoFilterGray{nullptr};
    QPushButton* m_buttonVideoFilterInvert{nullptr};
    QPushButton* m_buttonVideoFilterSticker{nullptr};
    QCheckBox* m_checkBoxGpu{nullptr};

    std::shared_ptr<av::IPlayer> m_player;
    av::MediaController*         m_controller{nullptr};
};

#endif  // LEARNAV_CONTROLLERWIDGET_H
