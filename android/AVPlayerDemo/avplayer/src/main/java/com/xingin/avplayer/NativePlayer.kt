package com.xingin.avplayer

/**
 * Native player wrapper
 */
class NativePlayer {
    private var nativeHandle: Long = 0
    private var playbackListener: IPlaybackListener? = null

    init {
        nativeHandle = nativeCreate()
    }

    fun release() {
        if (nativeHandle != 0L) {
            nativeRelease(nativeHandle)
            nativeHandle = 0
        }
    }

    /**
     * 设置与 Surface 共用的 GL 上下文为管线共享上下文。
     * 必须在已绑定 EGL 的线程上调用（如 GLSurfaceView 的 onSurfaceCreated / 渲染线程）。
     * 调用后，管线离屏渲染会使用与此上下文共享的 EGL 上下文，纹理等资源可共享。
     */
    fun setSharedGLContext() {
        nativeSetSharedGLContext(nativeHandle)
    }

    fun attachDisplayView(displayView: NativeDisplayView) {
        nativeAttachDisplayView(nativeHandle, displayView.getHandle())
    }

    fun detachDisplayView(displayView: NativeDisplayView) {
        nativeDetachDisplayView(nativeHandle, displayView.getHandle())
    }

    fun setPlaybackListener(listener: IPlaybackListener) {
        playbackListener = listener
        nativeSetPlaybackListener(nativeHandle, listener)
    }

    fun open(filePath: String): Boolean {
        return nativeOpen(nativeHandle, filePath)
    }

    fun play() {
        nativePlay(nativeHandle)
    }

    fun pause() {
        nativePause(nativeHandle)
    }

    fun seekTo(progress: Float) {
        nativeSeekTo(nativeHandle, progress)
    }

    fun isPlaying(): Boolean {
        return nativeIsPlaying(nativeHandle)
    }

    fun addVideoFilter(type: VideoFilterType): Long {
        return nativeAddVideoFilter(nativeHandle, type.value)
    }

    fun removeVideoFilter(type: VideoFilterType) {
        nativeRemoveVideoFilter(nativeHandle, type.value)
    }

    fun startRecording(outputFilePath: String, flags: Int): Boolean {
        return nativeStartRecording(nativeHandle, outputFilePath, flags)
    }

    fun stopRecording() {
        nativeStopRecording(nativeHandle)
    }

    fun isRecording(): Boolean {
        return nativeIsRecording(nativeHandle)
    }

    private external fun nativeCreate(): Long
    private external fun nativeRelease(handle: Long)
    private external fun nativeSetSharedGLContext(handle: Long)
    private external fun nativeAttachDisplayView(handle: Long, displayViewHandle: Long)
    private external fun nativeDetachDisplayView(handle: Long, displayViewHandle: Long)
    private external fun nativeSetPlaybackListener(handle: Long, listener: IPlaybackListener)
    private external fun nativeOpen(handle: Long, filePath: String): Boolean
    private external fun nativePlay(handle: Long)
    private external fun nativePause(handle: Long)
    private external fun nativeSeekTo(handle: Long, progress: Float)
    private external fun nativeIsPlaying(handle: Long): Boolean
    private external fun nativeAddVideoFilter(handle: Long, type: Int): Long
    private external fun nativeRemoveVideoFilter(handle: Long, type: Int)
    private external fun nativeStartRecording(handle: Long, outputFilePath: String, flags: Int): Boolean
    private external fun nativeStopRecording(handle: Long)
    private external fun nativeIsRecording(handle: Long): Boolean

    companion object {
        init {
            System.loadLibrary("learnav")
        }
    }
}
