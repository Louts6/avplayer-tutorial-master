//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#include "IGLContext.h"
#include "Log.h"

#include <QCoreApplication>
#include <QMetaObject>
#include <QThread>

namespace av {

GLContext::GLContext(void *sharedGLContext) { m_sharedGLContext = static_cast<QOpenGLContext *>(sharedGLContext); }

GLContext::~GLContext() {
    DoneCurrent();
    Destroy();
}

// thread_local：每个线程独立持有自己的 QOpenGLContext/QOffscreenSurface，
// 避免一个 context 被多个线程共享导致 "Cannot make QOpenGLContext current
// in a different thread" 致命错误。所有线程的 context 都与 m_sharedGLContext
// （来自 UI 线程的 QOpenGLWidget）共享，因此可以共享 OpenGL 资源（纹理/FBO 等）。
QOpenGLContext*& GLContext::tlsContext() {
    thread_local QOpenGLContext* ctx = nullptr;
    return ctx;
}

QOffscreenSurface*& GLContext::tlsSurface() {
    thread_local QOffscreenSurface* surf = nullptr;
    return surf;
}

bool GLContext::Initialize() {
    if (!m_sharedGLContext) {
        LOGE("GLContext", "Shared GL context is null!");
        return false;
    }

    // 如果当前线程已经初始化过，直接返回
    if (tlsContext() && tlsSurface()) return true;

    // Qt 6 中 QOffscreenSurface 是 QWindow 派生类，必须在 GUI 线程创建。
    // QOpenGLContext 必须在使用它的线程中 create()，否则 Qt 会拒绝跨线程 makeCurrent。
    if (QThread::currentThread() == qApp->thread()) {
        tlsSurface() = new QOffscreenSurface();
        tlsSurface()->create();
    } else {
        QMetaObject::invokeMethod(qApp, [this]() {
            tlsSurface() = new QOffscreenSurface();
            tlsSurface()->create();
        }, Qt::BlockingQueuedConnection);
    }

    tlsContext() = new QOpenGLContext();
    tlsContext()->setShareContext(m_sharedGLContext);
    if (!tlsContext()->create()) {
        LOGE("GLContext", "Failed to create GL context!");
        return false;
    }

    tlsContext()->makeCurrent(tlsSurface());
    return true;
}

void GLContext::CreateOnGUIThread(bool *created) {
    (void)created;
}

void GLContext::Destroy() {
    // QOffscreenSurface 必须在创建它的线程（GUI 线程）中销毁；
    // QOpenGLContext 也必须在创建它的线程中销毁。
    if (QThread::currentThread() != qApp->thread() && tlsSurface()) {
        auto surface = tlsSurface();
        tlsSurface() = nullptr;
        QMetaObject::invokeMethod(qApp, [surface]() { delete surface; },
                                  Qt::BlockingQueuedConnection);
    } else if (tlsSurface()) {
        delete tlsSurface();
        tlsSurface() = nullptr;
    }

    if (tlsContext()) {
        delete tlsContext();
        tlsContext() = nullptr;
    }
}

void GLContext::MakeCurrent() {
    if (tlsContext() && tlsSurface()) tlsContext()->makeCurrent(tlsSurface());
}

void GLContext::DoneCurrent() {
    if (tlsContext()) tlsContext()->doneCurrent();
}

}  // namespace av
