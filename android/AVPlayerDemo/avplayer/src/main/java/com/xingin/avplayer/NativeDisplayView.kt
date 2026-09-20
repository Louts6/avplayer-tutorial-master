package com.xingin.avplayer

/**
 * Native display view wrapper
 */
class NativeDisplayView {
    private var nativeHandle: Long = 0

    init {
        nativeHandle = nativeCreate()
    }

    fun release() {
        if (nativeHandle != 0L) {
            nativeRelease(nativeHandle)
            nativeHandle = 0
        }
    }

    fun initializeGL() {
        nativeInitializeGL(nativeHandle)
    }

    fun onSurfaceChanged(width: Int, height: Int) {
        nativeOnSurfaceChanged(nativeHandle, width, height)
    }

    fun onDrawFrame() {
        nativeOnDrawFrame(nativeHandle)
    }

    fun getHandle(): Long = nativeHandle

    private external fun nativeCreate(): Long
    private external fun nativeRelease(handle: Long)
    private external fun nativeInitializeGL(handle: Long)
    private external fun nativeOnSurfaceChanged(handle: Long, width: Int, height: Int)
    private external fun nativeOnDrawFrame(handle: Long)

    companion object {
        init {
            System.loadLibrary("learnav")
        }
    }
}
