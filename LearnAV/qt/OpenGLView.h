//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#ifndef LEARNAV_OPENGLVIEW_H
#define LEARNAV_OPENGLVIEW_H

#ifdef _WIN32
#include <glad/glad.h>
#endif
#include <QOpenGLContext>
#include <QOpenGLWidget>
#include <memory>

namespace av {

struct IVideoDisplayView;

class OpenGLView : public QOpenGLWidget {
    Q_OBJECT

public:
    explicit OpenGLView(QWidget *parent = nullptr);
    ~OpenGLView() override = default;

    std::shared_ptr<IVideoDisplayView> GetVideoDisplayView() { return m_videoDisplayView; }

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

private:
    int m_glWidth{0};
    int m_glHeight{0};
    std::shared_ptr<IVideoDisplayView> m_videoDisplayView;
};

}  // namespace av

#endif  // LEARNAV_OPENGLVIEW_H
