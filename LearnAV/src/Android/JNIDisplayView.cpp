//
// Copyright (c) 2024 Yellow. All rights reserved.
//

#include <jni.h>

#include "AndroidDisplayView.h"
#include "Interface/IVideoDisplayView.h"
#include "Log.h"

static const char* kJNIDisplayViewTag = "LearnAV-DisplayView";

namespace av {

static AndroidDisplayView* AsAndroidDisplayView(jlong handle) {
    return dynamic_cast<AndroidDisplayView*>(reinterpret_cast<IVideoDisplayView*>(handle));
}

}  // namespace av

extern "C" {

JNIEXPORT jlong JNICALL Java_com_xingin_avplayer_NativeDisplayView_nativeCreate(JNIEnv* env, jobject obj) {
    av::IVideoDisplayView* displayView = av::IVideoDisplayView::Create();
    LOGI(kJNIDisplayViewTag, "DisplayView created: %p", displayView);
    return reinterpret_cast<jlong>(displayView);
}

JNIEXPORT void JNICALL Java_com_xingin_avplayer_NativeDisplayView_nativeRelease(JNIEnv* env, jobject obj,
                                                                                jlong handle) {
    auto* displayView = reinterpret_cast<av::IVideoDisplayView*>(handle);
    if (displayView) {
        LOGI(kJNIDisplayViewTag, "DisplayView released: %p", displayView);
        delete displayView;
    }
}

JNIEXPORT void JNICALL Java_com_xingin_avplayer_NativeDisplayView_nativeInitializeGL(JNIEnv* env, jobject obj,
                                                                                     jlong handle) {
    if (auto* view = av::AsAndroidDisplayView(handle)) {
        LOGI(kJNIDisplayViewTag, "DisplayView InitializeGL");
        view->InitializeGL();
    }
}

JNIEXPORT void JNICALL Java_com_xingin_avplayer_NativeDisplayView_nativeOnSurfaceChanged(JNIEnv* env, jobject obj,
                                                                                         jlong handle, jint width,
                                                                                         jint height) {
    if (auto* view = av::AsAndroidDisplayView(handle)) {
        LOGI(kJNIDisplayViewTag, "DisplayView OnSurfaceChanged: %d x %d", width, height);
        view->SetDisplaySize(width, height);
        view->SetSurfaceSize(width, height);
    }
}

JNIEXPORT void JNICALL Java_com_xingin_avplayer_NativeDisplayView_nativeOnDrawFrame(JNIEnv* env, jobject obj,
                                                                                    jlong handle) {
    if (auto* view = av::AsAndroidDisplayView(handle)) {
        view->OnDrawFrame();
    }
}

}  // extern "C"
