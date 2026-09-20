//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#include "PlayerWidget.h"

#include <QVBoxLayout>

#include "IPlayer.h"
#include "OpenGLView.h"

PlayerWidget::PlayerWidget(QWidget *parent, std::shared_ptr<av::IPlayer> player) : QWidget(parent), m_player(player) {
    m_openGLView = new av::OpenGLView(this);
    if (m_player) m_player->AttachDisplayView(m_openGLView->GetVideoDisplayView());

    auto *vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(0, 0, 0, 0);
    vbox->addWidget(m_openGLView);
}

PlayerWidget::~PlayerWidget() {
    if (m_player && m_openGLView) m_player->DetachDisplayView(m_openGLView->GetVideoDisplayView());
}