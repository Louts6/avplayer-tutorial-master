//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#include <EGL/egl.h>
#include <GLES3/gl3.h>

#include <chrono>

#include "IGLContext.h"
#include "Log.h"

static const char* kAndroidGLTag = "AndroidGLContext";

namespace av {

GLContext::GLContext(void* context) : m_nativeContext(context) {}

GLContext::~GLContext() { Destroy(); }

void GLContext::SetSharedEGLContext(void* eglDisplay, void* eglContext) {
    if (m_initialized) return;
    EGLDisplay display = static_cast<EGLDisplay>(eglDisplay);
    EGLContext sharedCtx = static_cast<EGLContext>(eglContext);
    if (display == EGL_NO_DISPLAY || sharedCtx == EGL_NO_CONTEXT) {
        LOGE(kAndroidGLTag, "SetSharedEGLContext: invalid display or context");
        return;
    }
    const EGLint configAttribs[] = {EGL_SURFACE_TYPE,
                                    EGL_PBUFFER_BIT,
                                    EGL_RENDERABLE_TYPE,
                                    EGL_OPENGL_ES3_BIT,
                                    EGL_RED_SIZE,
                                    8,
                                    EGL_GREEN_SIZE,
                                    8,
                                    EGL_BLUE_SIZE,
                                    8,
                                    EGL_ALPHA_SIZE,
                                    8,
                                    EGL_NONE};
    EGLConfig config;
    EGLint numConfig;
    if (!eglChooseConfig(display, configAttribs, &config, 1, &numConfig) || numConfig == 0) {
        LOGE(kAndroidGLTag, "eglChooseConfig failed: 0x%x", eglGetError());
        return;
    }
    const EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    EGLContext ourCtx = eglCreateContext(display, config, sharedCtx, contextAttribs);
    if (ourCtx == EGL_NO_CONTEXT) {
        LOGE(kAndroidGLTag, "eglCreateContext failed: 0x%x", eglGetError());
        return;
    }
    const EGLint pbufferAttribs[] = {EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE};
    EGLSurface pbuffer = eglCreatePbufferSurface(display, config, pbufferAttribs);
    if (pbuffer == EGL_NO_SURFACE) {
        LOGE(kAndroidGLTag, "eglCreatePbufferSurface failed: 0x%x", eglGetError());
        eglDestroyContext(display, ourCtx);
        return;
    }
    m_eglDisplay = display;
    m_eglContextShared = sharedCtx;
    m_eglContextOur = ourCtx;
    m_eglSurfacePbuffer = pbuffer;
    m_eglConfig = config;
    m_initialized = true;
    {
        std::lock_guard<std::mutex> lock(m_readyMutex);
        m_ready = true;
    }
    m_readyCv.notify_all();
    LOGI(kAndroidGLTag, "Shared EGL context and PBuffer created for pipeline thread");
}

bool GLContext::Initialize() { return m_initialized; }

bool GLContext::WaitUntilReady(int timeoutMs) {
    const int kDefaultTimeoutMs = 10000;
    if (timeoutMs <= 0) timeoutMs = kDefaultTimeoutMs;
    std::unique_lock<std::mutex> lock(m_readyMutex);
    bool ok = m_readyCv.wait_for(lock, std::chrono::milliseconds(timeoutMs), [this] { return m_ready; });
    if (!ok) {
        LOGE(kAndroidGLTag, "WaitUntilReady timed out after %d ms (attachDisplayView not called on GL thread?)",
             timeoutMs);
    }
    return m_ready;
}

void GLContext::Destroy() {
    DoneCurrent();
    if (m_eglDisplay != nullptr) {
        EGLDisplay d = static_cast<EGLDisplay>(m_eglDisplay);
        if (m_eglSurfacePbuffer != nullptr) {
            eglDestroySurface(d, static_cast<EGLSurface>(m_eglSurfacePbuffer));
            m_eglSurfacePbuffer = nullptr;
        }
        if (m_eglContextOur != nullptr) {
            eglDestroyContext(d, static_cast<EGLContext>(m_eglContextOur));
            m_eglContextOur = nullptr;
        }
        m_eglConfig = nullptr;
        m_eglContextShared = nullptr;
        m_eglDisplay = nullptr;
    }
    m_initialized = false;
    {
        std::lock_guard<std::mutex> lock(m_readyMutex);
        m_ready = false;
    }
    m_readyCv.notify_all();
}

void GLContext::MakeCurrent() {
    if (!m_initialized || m_eglDisplay == nullptr || m_eglContextOur == nullptr || m_eglSurfacePbuffer == nullptr)
        return;
    EGLBoolean ok =
        eglMakeCurrent(static_cast<EGLDisplay>(m_eglDisplay), static_cast<EGLSurface>(m_eglSurfacePbuffer),
                       static_cast<EGLSurface>(m_eglSurfacePbuffer), static_cast<EGLContext>(m_eglContextOur));
    if (!ok) {
        LOGE(kAndroidGLTag, "eglMakeCurrent failed: 0x%x", eglGetError());
    }
}

void GLContext::DoneCurrent() {
    if (m_eglDisplay != nullptr) {
        eglMakeCurrent(static_cast<EGLDisplay>(m_eglDisplay), EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    }
}

}  // namespace av
