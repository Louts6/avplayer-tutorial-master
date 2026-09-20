//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#include <EGL/egl.h>
#include <jni.h>

#include "IGLContext.h"
#include "IPlaybackListener.h"
#include "IPlayer.h"
#include "Interface/IVideoDisplayView.h"
#include "Log.h"

static const char* kJNIPlayerTag = "LearnAV-JNI";

namespace av {
struct AndroidPlayerHolder {
    std::shared_ptr<GLContext> glContext{nullptr};
    IPlayer* player{nullptr};
};
static inline IPlayer* GetPlayerFromHandle(jlong handle) {
    auto* holder = reinterpret_cast<AndroidPlayerHolder*>(handle);
    return holder ? holder->player : nullptr;
}
}  // namespace av
#define GET_PLAYER(h) av::GetPlayerFromHandle(h)

namespace av {

class JNIPlaybackListener : public IPlaybackListener {
public:
    explicit JNIPlaybackListener(JNIEnv* env, jobject listener) {
        env->GetJavaVM(&m_javaVM);
        m_listener = env->NewGlobalRef(listener);

        jclass listenerClass = env->GetObjectClass(listener);
        m_onPlaybackStarted = env->GetMethodID(listenerClass, "onPlaybackStarted", "()V");
        m_onPlaybackTimeChanged = env->GetMethodID(listenerClass, "onPlaybackTimeChanged", "(FF)V");
        m_onPlaybackPaused = env->GetMethodID(listenerClass, "onPlaybackPaused", "()V");
        m_onPlaybackEOF = env->GetMethodID(listenerClass, "onPlaybackEOF", "()V");
        env->DeleteLocalRef(listenerClass);
    }

    ~JNIPlaybackListener() override {
        if (m_javaVM && m_listener) {
            JNIEnv* env = nullptr;
            m_javaVM->GetEnv((void**)&env, JNI_VERSION_1_6);
            if (env) env->DeleteGlobalRef(m_listener);
        }
    }

    void NotifyPlaybackStarted() override { callVoid(m_onPlaybackStarted); }
    void NotifyPlaybackTimeChanged(float timeStamp, float duration) override {
        callVoidFloatFloat(m_onPlaybackTimeChanged, timeStamp, duration);
    }
    void NotifyPlaybackPaused() override { callVoid(m_onPlaybackPaused); }
    void NotifyPlaybackEOF() override { callVoid(m_onPlaybackEOF); }

private:
    void callVoid(jmethodID methodId) {
        if (!m_javaVM || !m_listener || !methodId) return;
        JNIEnv* env = nullptr;
        bool needsDetach = (m_javaVM->GetEnv((void**)&env, JNI_VERSION_1_6) == JNI_EDETACHED);
        if (needsDetach) m_javaVM->AttachCurrentThread(&env, nullptr);
        if (env) {
            env->CallVoidMethod(m_listener, methodId);
            if (needsDetach) m_javaVM->DetachCurrentThread();
        }
    }
    void callVoidFloatFloat(jmethodID methodId, float a, float b) {
        if (!m_javaVM || !m_listener || !methodId) return;
        JNIEnv* env = nullptr;
        bool needsDetach = (m_javaVM->GetEnv((void**)&env, JNI_VERSION_1_6) == JNI_EDETACHED);
        if (needsDetach) m_javaVM->AttachCurrentThread(&env, nullptr);
        if (env) {
            env->CallVoidMethod(m_listener, methodId, a, b);
            if (needsDetach) m_javaVM->DetachCurrentThread();
        }
    }

    JavaVM* m_javaVM{nullptr};
    jobject m_listener{nullptr};
    jmethodID m_onPlaybackStarted{nullptr};
    jmethodID m_onPlaybackTimeChanged{nullptr};
    jmethodID m_onPlaybackPaused{nullptr};
    jmethodID m_onPlaybackEOF{nullptr};
};

}  // namespace av

extern "C" {

JNIEXPORT jlong JNICALL Java_com_xingin_avplayer_NativePlayer_nativeCreate(JNIEnv* env, jobject obj) {
    auto* holder = new av::AndroidPlayerHolder();
    holder->glContext = std::make_shared<av::GLContext>(nullptr);
    holder->player = av::IPlayer::Create(holder->glContext);
    return reinterpret_cast<jlong>(holder);
}

JNIEXPORT void JNICALL Java_com_xingin_avplayer_NativePlayer_nativeRelease(JNIEnv* env, jobject obj, jlong handle) {
    auto* holder = reinterpret_cast<av::AndroidPlayerHolder*>(handle);
    if (holder) {
        if (holder->player) delete holder->player;
        holder->glContext.reset();
        delete holder;
    }
}

JNIEXPORT void JNICALL Java_com_xingin_avplayer_NativePlayer_nativeSetSharedGLContext(JNIEnv* env, jobject obj,
                                                                                      jlong handle) {
    auto* holder = reinterpret_cast<av::AndroidPlayerHolder*>(handle);
    if (!holder || !holder->glContext) return;
    EGLDisplay display = eglGetCurrentDisplay();
    EGLContext context = eglGetCurrentContext();
    if (display != EGL_NO_DISPLAY && context != EGL_NO_CONTEXT)
        holder->glContext->SetSharedEGLContext(display, context);
}

JNIEXPORT void JNICALL Java_com_xingin_avplayer_NativePlayer_nativeAttachDisplayView(JNIEnv* env, jobject obj,
                                                                                     jlong handle,
                                                                                     jlong displayViewHandle) {
    auto* holder = reinterpret_cast<av::AndroidPlayerHolder*>(handle);
    if (!holder || !holder->player) return;
    EGLDisplay display = eglGetCurrentDisplay();
    EGLContext context = eglGetCurrentContext();
    if (display != EGL_NO_DISPLAY && context != EGL_NO_CONTEXT && holder->glContext)
        holder->glContext->SetSharedEGLContext(display, context);
    auto* displayView = reinterpret_cast<av::IVideoDisplayView*>(displayViewHandle);
    if (displayView) {
        holder->player->AttachDisplayView(
            std::shared_ptr<av::IVideoDisplayView>(displayView, [](av::IVideoDisplayView*) {}));
    }
}

JNIEXPORT void JNICALL Java_com_xingin_avplayer_NativePlayer_nativeDetachDisplayView(JNIEnv* env, jobject obj,
                                                                                     jlong handle,
                                                                                     jlong displayViewHandle) {
    av::IPlayer* player = GET_PLAYER(handle);
    auto* displayView = reinterpret_cast<av::IVideoDisplayView*>(displayViewHandle);
    if (player && displayView) {
        player->DetachDisplayView(std::shared_ptr<av::IVideoDisplayView>(displayView, [](av::IVideoDisplayView*) {}));
    }
}

JNIEXPORT void JNICALL Java_com_xingin_avplayer_NativePlayer_nativeSetPlaybackListener(JNIEnv* env, jobject obj,
                                                                                       jlong handle, jobject listener) {
    av::IPlayer* player = GET_PLAYER(handle);
    if (player && listener) {
        auto jniListener = std::make_shared<av::JNIPlaybackListener>(env, listener);
        player->SetPlaybackListener(jniListener);
    }
}

JNIEXPORT jboolean JNICALL Java_com_xingin_avplayer_NativePlayer_nativeOpen(JNIEnv* env, jobject obj, jlong handle,
                                                                            jstring filePath) {
    av::IPlayer* player = GET_PLAYER(handle);
    if (!player) return JNI_FALSE;
    const char* path = env->GetStringUTFChars(filePath, nullptr);
    std::string pathStr(path);
    env->ReleaseStringUTFChars(filePath, path);
    return player->Open(pathStr) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL Java_com_xingin_avplayer_NativePlayer_nativePlay(JNIEnv* env, jobject obj, jlong handle) {
    av::IPlayer* player = GET_PLAYER(handle);
    if (player) player->Play();
}

JNIEXPORT void JNICALL Java_com_xingin_avplayer_NativePlayer_nativePause(JNIEnv* env, jobject obj, jlong handle) {
    av::IPlayer* player = GET_PLAYER(handle);
    if (player) player->Pause();
}

JNIEXPORT void JNICALL Java_com_xingin_avplayer_NativePlayer_nativeSeekTo(JNIEnv* env, jobject obj, jlong handle,
                                                                          jfloat progress) {
    av::IPlayer* player = GET_PLAYER(handle);
    if (player) player->SeekTo(progress);
}

JNIEXPORT jboolean JNICALL Java_com_xingin_avplayer_NativePlayer_nativeIsPlaying(JNIEnv* env, jobject obj,
                                                                                 jlong handle) {
    av::IPlayer* player = GET_PLAYER(handle);
    return (player && player->IsPlaying()) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jlong JNICALL Java_com_xingin_avplayer_NativePlayer_nativeAddVideoFilter(JNIEnv* env, jobject obj,
                                                                                   jlong handle, jint type) {
    av::IPlayer* player = GET_PLAYER(handle);
    if (!player) return 0;
    auto filter = player->AddVideoFilter(static_cast<av::VideoFilterType>(type));
    return reinterpret_cast<jlong>(filter.get());
}

JNIEXPORT void JNICALL Java_com_xingin_avplayer_NativePlayer_nativeRemoveVideoFilter(JNIEnv* env, jobject obj,
                                                                                     jlong handle, jint type) {
    av::IPlayer* player = GET_PLAYER(handle);
    if (player) player->RemoveVideoFilter(static_cast<av::VideoFilterType>(type));
}

JNIEXPORT jboolean JNICALL Java_com_xingin_avplayer_NativePlayer_nativeStartRecording(JNIEnv* env, jobject obj,
                                                                                      jlong handle,
                                                                                      jstring outputFilePath,
                                                                                      jint flags) {
    av::IPlayer* player = GET_PLAYER(handle);
    if (!player) return JNI_FALSE;
    const char* path = env->GetStringUTFChars(outputFilePath, nullptr);
    std::string pathStr(path);
    env->ReleaseStringUTFChars(outputFilePath, path);
    return player->StartRecording(pathStr, flags) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL Java_com_xingin_avplayer_NativePlayer_nativeStopRecording(JNIEnv* env, jobject obj,
                                                                                 jlong handle) {
    av::IPlayer* player = GET_PLAYER(handle);
    if (player) player->StopRecording();
}

JNIEXPORT jboolean JNICALL Java_com_xingin_avplayer_NativePlayer_nativeIsRecording(JNIEnv* env, jobject obj,
                                                                                   jlong handle) {
    av::IPlayer* player = GET_PLAYER(handle);
    return (player && player->IsRecording()) ? JNI_TRUE : JNI_FALSE;
}

}  // extern "C"
