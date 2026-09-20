package com.xingin.avplayer

import android.content.Context
import android.opengl.GLSurfaceView
import android.util.AttributeSet
import android.util.Log
import javax.microedition.khronos.egl.EGLConfig
import javax.microedition.khronos.opengles.GL10

/**
 * GLSurfaceView for video playback
 */
class PlayerGLSurfaceView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null
) : GLSurfaceView(context, attrs) {

    companion object {
        private const val TAG = "PlayerGLSurfaceView"
    }

    private val displayView: NativeDisplayView = NativeDisplayView()
    private var surfaceWidth: Int = 0
    private var surfaceHeight: Int = 0
    private val onSurfaceReadyCallbacks = mutableListOf<() -> Unit>()
    private var isSurfaceReady = false

    init {
        Log.i(TAG, "PlayerGLSurfaceView created")
        setEGLContextClientVersion(3) // OpenGL ES 3.0
        setRenderer(PlayerRenderer())
        renderMode = RENDERMODE_CONTINUOUSLY
    }

    fun getDisplayView(): NativeDisplayView = displayView
    
    /**
     * Add a callback to be invoked when the surface is ready
     * If surface is already ready, callback will be invoked immediately
     */
    fun addOnSurfaceReadyCallback(callback: () -> Unit) {
        if (isSurfaceReady) {
            // Surface already ready, invoke immediately
            Log.i(TAG, "Surface already ready, invoking callback immediately")
            callback.invoke()
        } else {
            // Add to pending callbacks
            Log.i(TAG, "Surface not ready yet, adding callback to queue")
            onSurfaceReadyCallbacks.add(callback)
        }
    }

    fun cleanup() {
        displayView.release()
    }

    private inner class PlayerRenderer : Renderer {
        override fun onSurfaceCreated(gl: GL10?, config: EGLConfig?) {
            Log.i(TAG, "onSurfaceCreated called - GL context is ready!")
            displayView.initializeGL()
            // Mark surface as ready and invoke all pending callbacks
            isSurfaceReady = true
            Log.i(TAG, "Invoking ${onSurfaceReadyCallbacks.size} pending callbacks")
            onSurfaceReadyCallbacks.forEach { it.invoke() }
            onSurfaceReadyCallbacks.clear()
        }

        override fun onSurfaceChanged(gl: GL10?, width: Int, height: Int) {
            surfaceWidth = width
            surfaceHeight = height
            displayView.onSurfaceChanged(width, height)
        }

        override fun onDrawFrame(gl: GL10?) {
            displayView.onDrawFrame()
        }
    }
}
