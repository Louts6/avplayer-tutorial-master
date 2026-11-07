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
    // 注意：lambda 若内部直接写 tlsSurface()，实际写的是 GUI 线程的 thread_local，
    // 当前线程拿到的仍是 nullptr，导致 makeCurrent 失败（FBO/纹理全部创建失败）。
    // 因此用局部变量回传指针，再由当前线程写入自己的 tlsSurface()。
    QOffscreenSurface* surface = nullptr;
    if (QThread::currentThread() == qApp->thread()) {
        surface = new QOffscreenSurface();
        surface->create();
    } else {
        QMetaObject::invokeMethod(qApp, [&surface, this]() {
            surface = new QOffscreenSurface();
            surface->setFormat(m_sharedGLContext->format());
            surface->create();
        }, Qt::BlockingQueuedConnection);
    }
    if (!surface || !surface->isValid()) {
        LOGE("GLContext", "Failed to create offscreen surface!");
        return false;
    }
    tlsSurface() = surface;

    tlsContext() = new QOpenGLContext();
    tlsContext()->setShareContext(m_sharedGLContext);
    if (!tlsContext()->create()) {
        LOGE("GLContext", "Failed to create GL context!");
        return false;
    }

    if (!tlsContext()->makeCurrent(tlsSurface())) {
        LOGE("GLContext", "Failed to make GL context current in this thread!");
        return false;
    }
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
    if (!tlsContext() || !tlsSurface()) {
        LOGE("GLContext", "MakeCurrent called before Initialize() in this thread!");
        return;
    }
    if (!tlsContext()->makeCurrent(tlsSurface())) {
        LOGE("GLContext", "Failed to make GL context current in this thread!");
    }
}

void GLContext::DoneCurrent() {
    if (tlsContext()) tlsContext()->doneCurrent();
}

}  // namespace av
