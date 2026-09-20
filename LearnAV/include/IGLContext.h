//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#ifndef LEARNAV_IGLCONTEXT_H
#define LEARNAV_IGLCONTEXT_H

#include <memory>

// 根据平台选择不同的 OpenGL 上下文实现，对外 API 一致（Initialize/Destroy/MakeCurrent/DoneCurrent/WaitUntilReady）。
#ifdef ANDROID
// Android：App 在渲染线程设置共享 EGL；管线线程使用与其共享的 EGL context 做离屏渲染（实现见 AndroidGLContext.cpp）。
#include <condition_variable>
#include <mutex>

namespace av {

class GLContext {
public:
    explicit GLContext(void* context);
    ~GLContext();

    /** 由已在渲染线程绑定 EGL 的调用方调用（或 JNI SetSharedGLContext），传入
     * eglGetCurrentDisplay/Context；管线将创建与其共享的 context。 */
    void SetSharedEGLContext(void* eglDisplay, void* eglContext);
    void* GetSharedGLContext() { return m_nativeContext; }

    bool Initialize();
    void Destroy();
    void MakeCurrent();
    void DoneCurrent();

    /** 阻塞直到 SetSharedEGLContext 已在渲染线程调用成功，或超时。timeoutMs <= 0 表示默认 10s。返回 true 表示 context
     * 已就绪。 */
    bool WaitUntilReady(int timeoutMs = 0);

private:
    void* m_nativeContext{nullptr};
    void* m_eglDisplay{nullptr};
    void* m_eglContextShared{nullptr};
    void* m_eglContextOur{nullptr};
    void* m_eglSurfacePbuffer{nullptr};
    void* m_eglConfig{nullptr};
    bool m_initialized{false};
    std::mutex m_readyMutex;
    std::condition_variable m_readyCv;
    bool m_ready{false};
};

}  // namespace av

#else
// Qt 平台（Mac/Windows/Linux）
#ifdef _WIN32
#include <glad/glad.h>
#endif
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QThread>

namespace av {

class GLContext {
public:
    explicit GLContext(void* sharedGLContext);
    virtual ~GLContext();

    bool Initialize();
    void Destroy();
    void MakeCurrent();
    void DoneCurrent();
    void* GetSharedGLContext() { return m_sharedGLContext; }

    /** Qt 下无需等待，直接返回 true。 */
    bool WaitUntilReady(int timeoutMs = 0) {
        (void)timeoutMs;
        return true;
    }

private:
    void CreateOnGUIThread(bool* created);

    QOpenGLContext* m_sharedGLContext{nullptr};
    QOpenGLContext* m_context{nullptr};
    QOffscreenSurface* m_surface{nullptr};
};

}  // namespace av

#endif  // ANDROID

#endif  // LEARNAV_IGLCONTEXT_H
