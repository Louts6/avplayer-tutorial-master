package com.xingin.avplayer

import android.util.Log

/**
 * High-level AVPlayer interface
 * Provides a simple API for video playback
 */
class AVPlayer(private val surfaceView: PlayerGLSurfaceView) {
    
    private val nativePlayer: NativePlayer = NativePlayer()
    private var playbackListener: IPlaybackListener? = null
    private var isDisplayViewAttached = false

    init {
        Log.i(TAG, "AVPlayer created, waiting for surface ready...")
        // Attach the display view to the player when surface is ready
        surfaceView.addOnSurfaceReadyCallback {
            if (!isDisplayViewAttached) {
                Log.i(TAG, "Surface ready! Setting shared GL context and attaching display view")
                // 由 App 显式设置当前 EGL 为共享上下文，Surface 与管线离屏渲染共享此 context
                nativePlayer.setSharedGLContext()
                nativePlayer.attachDisplayView(surfaceView.getDisplayView())
                isDisplayViewAttached = true
                Log.i(TAG, "Display view attached successfully")
            }
        }
    }
    
    companion object {
        private const val TAG = "AVPlayer"
    }

    /**
     * Set playback listener
     */
    fun setPlaybackListener(listener: IPlaybackListener) {
        playbackListener = listener
        nativePlayer.setPlaybackListener(listener)
    }

    /**
     * Open a video file
     * @param filePath Path to the video file
     * @return true if successful, false otherwise
     */
    fun open(filePath: String): Boolean {
        return nativePlayer.open(filePath)
    }

    /**
     * Start or resume playback
     */
    fun play() {
        nativePlayer.play()
    }

    /**
     * Pause playback
     */
    fun pause() {
        nativePlayer.pause()
    }

    /**
     * Seek to a specific position
     * @param progress Position in range [0.0, 1.0]
     */
    fun seekTo(progress: Float) {
        nativePlayer.seekTo(progress.coerceIn(0f, 1f))
    }

    /**
     * Check if currently playing
     * @return true if playing, false otherwise
     */
    fun isPlaying(): Boolean {
        return nativePlayer.isPlaying()
    }

    /**
     * Add a video filter
     * @param type Filter type
     * @return Filter handle
     */
    fun addVideoFilter(type: VideoFilterType): Long {
        return nativePlayer.addVideoFilter(type)
    }

    /**
     * Remove a video filter
     * @param type Filter type
     */
    fun removeVideoFilter(type: VideoFilterType) {
        nativePlayer.removeVideoFilter(type)
    }

    /**
     * Start recording
     * @param outputFilePath Output file path
     * @param flags Recording flags
     * @return true if successful, false otherwise
     */
    fun startRecording(outputFilePath: String, flags: Int = 0): Boolean {
        return nativePlayer.startRecording(outputFilePath, flags)
    }

    /**
     * Stop recording
     */
    fun stopRecording() {
        nativePlayer.stopRecording()
    }

    /**
     * Check if currently recording
     * @return true if recording, false otherwise
     */
    fun isRecording(): Boolean {
        return nativePlayer.isRecording()
    }

    /**
     * Release resources
     * Call this when done with the player
     */
    fun release() {
        nativePlayer.detachDisplayView(surfaceView.getDisplayView())
        nativePlayer.release()
        surfaceView.cleanup()
    }
}
