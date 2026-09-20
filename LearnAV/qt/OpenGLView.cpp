//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#include "OpenGLView.h"

#include "Interface/IVideoDisplayView.h"

namespace av {

OpenGLView::OpenGLView(QWidget *parent) : QOpenGLWidget(parent) {
    m_videoDisplayView = std::shared_ptr<IVideoDisplayView>(IVideoDisplayView::Create());
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void OpenGLView::initializeGL() {
    if (m_videoDisplayView) m_videoDisplayView->InitializeGL();
}

void OpenGLView::resizeGL(int w, int h) {
    m_glWidth = w * devicePixelRatio();
    m_glHeight = h * devicePixelRatio();
}

void OpenGLView::paintGL() {
    if (m_videoDisplayView) m_videoDisplayView->Render(m_glWidth, m_glHeight, 0.0, 0.0, 0.0);
    update();
}

}  // namespace av