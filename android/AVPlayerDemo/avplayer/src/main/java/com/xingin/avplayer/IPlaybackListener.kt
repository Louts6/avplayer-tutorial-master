package com.xingin.avplayer

/**
 * Playback listener interface
 */
interface IPlaybackListener {
    fun onPlaybackStarted()
    fun onPlaybackTimeChanged(timeStamp: Float, duration: Float)
    fun onPlaybackPaused()
    fun onPlaybackEOF()
}
