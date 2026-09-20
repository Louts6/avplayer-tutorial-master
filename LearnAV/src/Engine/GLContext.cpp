//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#include "IGLContext.h"
#include "Log.h"

#include <QCoreApplication>
#include <QMetaObject>

namespace av {

GLContext::GLContext(void *sharedGLContext) { m_sharedGLContext = static_cast<QOpenGLContext *>(sharedGLContext); }

GLContext::~GLContext() {
    DoneCurrent();
    Destroy();
}

bool GLContext::Initialize() {
    if (!m_sharedGLContext) {
        LOGE("GLContext", "Shared GL context is null!");
        return false;
    }

    // Qt 6 中 QOffscreenSurface 是 QWindow 派生类，必须在 GUI 线程创建，
    // 否则会出现 "Attempting to create QWindow-based QOffscreenSurface
    // outside the gui thread" 警告并失败。
    // 但 QOpenGLContext 必须在使用它的线程中 create()，否则
    // Qt 会拒绝在另一线程 makeCurrent ("Cannot make QOpenGLContext current
    // in a different thread")，触发 fatal error。
    if (QThread::currentThread() == qApp->thread()) {
        m_surface = new QOffscreenSurface();
        m_surface->create();
    } else {
        QMetaObject::invokeMethod(qApp, [this]() {
            m_surface = new QOffscreenSurface();
            m_surface->create();
        }, Qt::BlockingQueuedConnection);
    }

    m_context = new QOpenGLContext();
    m_context->setShareContext(m_sharedGLContext);
    if (!m_context->create()) {
        LOGE("GLContext", "Failed to create GL context!");
        return false;
    }

    m_context->makeCurrent(m_surface);
    return true;
}

void GLContext::CreateOnGUIThread(bool *created) {
    // 保留以兼容头文件声明，已不再使用。
    (void)created;
}

void GLContext::Destroy() {
    // QOffscreenSurface 必须在创建它的线程（GUI 线程）中销毁；
    // QOpenGLContext 也必须在创建它的线程中销毁（即调用 Destroy 的线程，
    // 通常是与 Initialize 同一线程）。
    if (QThread::currentThread() != qApp->thread() && m_surface) {
        auto surface = m_surface;
        m_surface = nullptr;
        QMetaObject::invokeMethod(qApp, [surface]() { delete surface; },
                                  Qt::BlockingQueuedConnection);
    } else if (m_surface) {
        delete m_surface;
        m_surface = nullptr;
    }

    if (m_context) {
        delete m_context;
        m_context = nullptr;
    }
}

void GLContext::MakeCurrent() {
    if (m_context && m_surface) m_context->makeCurrent(m_surface);
}

void GLContext::DoneCurrent() {
    if (m_context) m_context->doneCurrent();
}

}  // namespace av