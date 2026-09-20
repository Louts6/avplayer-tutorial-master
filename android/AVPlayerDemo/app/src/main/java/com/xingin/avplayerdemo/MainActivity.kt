package com.xingin.avplayerdemo

import android.Manifest
import android.content.pm.PackageManager
import android.os.Bundle
import android.util.Log
import android.view.ViewGroup
import android.widget.Button
import android.widget.LinearLayout
import android.widget.SeekBar
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import com.xingin.avplayer.AVPlayer
import com.xingin.avplayer.IPlaybackListener
import com.xingin.avplayer.PlayerGLSurfaceView
import com.xingin.avplayer.VideoFilterType
import java.io.File
import java.io.FileOutputStream

class MainActivity : ComponentActivity() {

    private lateinit var surfaceView: PlayerGLSurfaceView
    private lateinit var player: AVPlayer
    private lateinit var seekBar: SeekBar
    private var playButton: Button? = null
    private var isUserSeeking = false

    private val playbackListener = object : IPlaybackListener {
        override fun onPlaybackStarted() {
            Log.i(TAG, "Playback started")
            runOnUiThread { showToast("Playback started") }
        }
        override fun onPlaybackTimeChanged(timeStamp: Float, duration: Float) {
            if (!isUserSeeking && duration > 0) {
                runOnUiThread { seekBar.progress = ((timeStamp / duration) * 1000).toInt() }
            }
        }
        override fun onPlaybackPaused() {
            Log.i(TAG, "Playback paused")
        }
        override fun onPlaybackEOF() {
            Log.i(TAG, "Playback EOF")
            runOnUiThread { showToast("Playback finished") }
        }
    }

    companion object {
        private const val TAG = "AVPlayerDemo"
        private const val REQUEST_PERMISSIONS = 1
        private val REQUIRED_PERMISSIONS = arrayOf(
            Manifest.permission.READ_EXTERNAL_STORAGE,
            Manifest.permission.WRITE_EXTERNAL_STORAGE
        )
    }
    
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        
        // Check permissions
        if (!hasPermissions()) {
            ActivityCompat.requestPermissions(this, REQUIRED_PERMISSIONS, REQUEST_PERMISSIONS)
        }
        
        setupUI()
    }
    
    private fun setupUI() {
        val mainLayout = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            layoutParams = ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT
            )
        }
        surfaceView = createSurfaceView()
        mainLayout.addView(surfaceView)
        seekBar = createSeekBar()
        mainLayout.addView(seekBar)
        mainLayout.addView(createControlLayout())
        setContentView(mainLayout)
        Log.i(TAG, "=== UI Setup Complete ===")
        initPlayer()
        scheduleVideoLoad()
    }

    private fun createSurfaceView(): PlayerGLSurfaceView {
        return PlayerGLSurfaceView(this).apply {
            layoutParams = LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                0
            ).apply { weight = 1f }
        }
    }

    private fun createSeekBar(): SeekBar {
        return SeekBar(this).apply {
            max = 1000
            layoutParams = LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT
            )
            setOnSeekBarChangeListener(seekBarChangeListener)
        }
    }

    private val seekBarChangeListener = object : SeekBar.OnSeekBarChangeListener {
        override fun onProgressChanged(seekBar: SeekBar?, progress: Int, fromUser: Boolean) {
            if (fromUser && ::player.isInitialized) player.seekTo(progress / 1000f)
        }
        override fun onStartTrackingTouch(seekBar: SeekBar?) { isUserSeeking = true }
        override fun onStopTrackingTouch(seekBar: SeekBar?) { isUserSeeking = false }
    }

    private fun createControlLayout(): LinearLayout {
        val layout = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            layoutParams = LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT
            )
        }
        val playBtn = createPlayButton()
        playButton = playBtn
        layout.addView(playBtn)
        layout.addView(createFilterToggleButton("Gray", VideoFilterType.GRAY))
        layout.addView(createFilterToggleButton("Flip", VideoFilterType.FLIP_VERTICAL))
        layout.addView(createFilterToggleButton("Invert", VideoFilterType.INVERT))
        return layout
    }

    private fun createPlayButton(): Button {
        return Button(this).apply {
            text = "Play"
            setOnClickListener {
                if (!::player.isInitialized) return@setOnClickListener
                if (player.isPlaying()) {
                    player.pause()
                    text = "Play"
                } else {
                    player.play()
                    text = "Pause"
                }
            }
        }
    }

    private fun createFilterToggleButton(initialText: String, filterType: VideoFilterType): Button {
        return Button(this).apply {
            text = initialText
            var filterEnabled = false
            setOnClickListener {
                if (!::player.isInitialized) return@setOnClickListener
                if (filterEnabled) {
                    player.removeVideoFilter(filterType)
                    text = initialText
                } else {
                    player.addVideoFilter(filterType)
                    text = "Remove $initialText"
                }
                filterEnabled = !filterEnabled
            }
        }
    }

    private fun scheduleVideoLoad() {
        surfaceView.addOnSurfaceReadyCallback {
            Log.i(TAG, "!!! Surface ready callback triggered, loading default video !!!")
            runOnUiThread { loadDefaultVideo() }
        }
        surfaceView.postDelayed({
            if (::player.isInitialized && !player.isPlaying()) {
                Log.w(TAG, "!!! Backup loader triggered - callback may have failed !!!")
                loadDefaultVideo()
            }
        }, 1500)
    }
    
    private fun initPlayer() {
        player = AVPlayer(surfaceView)
        player.setPlaybackListener(playbackListener)
    }
    
    private fun loadDefaultVideo() {
        try {
            val cacheFile = getOrCopyAssetToCache("sample.mp4") ?: return
            val success = player.open(cacheFile.absolutePath)
            if (success) {
                showToast("Default video loaded")
                Log.i(TAG, "Opened default video: ${cacheFile.absolutePath}")
                player.play()
            } else {
                showToast("Failed to open default video")
                Log.e(TAG, "Failed to open video: ${cacheFile.absolutePath}")
            }
        } catch (e: Exception) {
            Log.e(TAG, "Error loading default video", e)
            showToast("Error loading video: ${e.message}", Toast.LENGTH_LONG)
        }
    }

    private fun getOrCopyAssetToCache(assetName: String): File? {
        assets.open(assetName).use { input ->
            val cacheFile = File(cacheDir, assetName)
            val expectedSize = input.available().toLong()
            if (!cacheFile.exists() || cacheFile.length() != expectedSize) {
                Log.i(TAG, "Copying video from assets to cache...")
                FileOutputStream(cacheFile).use { input.copyTo(it) }
                Log.i(TAG, "Video copied to: ${cacheFile.absolutePath}")
            }
            return cacheFile.takeIf { it.exists() }
        }
    }
    
    private fun showToast(message: String, length: Int = Toast.LENGTH_SHORT) {
        Toast.makeText(this, message, length).show()
    }

    private fun hasPermissions(): Boolean {
        return REQUIRED_PERMISSIONS.all {
            ContextCompat.checkSelfPermission(this, it) == PackageManager.PERMISSION_GRANTED
        }
    }
    
    override fun onRequestPermissionsResult(
        requestCode: Int,
        permissions: Array<out String>,
        grantResults: IntArray
    ) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (requestCode == REQUEST_PERMISSIONS) {
            showToast(
                if (grantResults.all { it == PackageManager.PERMISSION_GRANTED }) "Permissions granted"
                else "Permissions denied"
            )
        }
    }
    
    override fun onPause() {
        super.onPause()
        if (::player.isInitialized && player.isPlaying()) {
            player.pause()
            playButton?.text = "Play"
            Log.i(TAG, "Playback paused: app moved to background")
        }
    }
    
    override fun onDestroy() {
        super.onDestroy()
        if (::player.isInitialized) {
            player.release()
        }
    }
}
