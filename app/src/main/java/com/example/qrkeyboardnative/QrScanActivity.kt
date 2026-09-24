package com.example.qrkeyboardnative

import android.Manifest
import android.app.Activity
import android.content.pm.PackageManager
import android.os.Bundle
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.widget.Toast
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat

/**
 * IRREDUCIBLE ANDROID GLUE.
 *
 * Two things here exist ONLY because Android's security model requires
 * them to originate from a real Activity, with no NDK/native alternative:
 *   1. Requesting the runtime CAMERA permission (ActivityCompat.requestPermissions).
 *   2. Owning a Surface that can be shown full-screen above the keyboard.
 *
 * Camera capture itself (Camera2 NDK), the video pipeline, and QR decoding
 * (quirc) are 100% implemented in native C/C++ - see camera.cpp. This class
 * does not touch a single camera frame or decode a single byte.
 */
class QrScanActivity : Activity() {

    private external fun nativeStartCamera(surface: android.view.Surface)
    private external fun nativeStopCamera()

    companion object {
        private const val REQUEST_CAMERA = 1001
        init {
            System.loadLibrary("qrkeyboard_native")
        }
    }

    private var surfaceReady = false
    private var permissionGranted = false
    private var savedHolder: SurfaceHolder? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val surfaceView = SurfaceView(this)
        setContentView(surfaceView)

        surfaceView.holder.addCallback(object : SurfaceHolder.Callback {
            override fun surfaceCreated(holder: SurfaceHolder) {
                surfaceReady = true
                savedHolder = holder
                maybeStartCamera(holder)
            }
            override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {}
            override fun surfaceDestroyed(holder: SurfaceHolder) {
                surfaceReady = false
                savedHolder = null
                nativeStopCamera()
            }
        })

        if (ContextCompat.checkSelfPermission(this, Manifest.permission.CAMERA)
            == PackageManager.PERMISSION_GRANTED
        ) {
            permissionGranted = true
        } else {
            ActivityCompat.requestPermissions(this, arrayOf(Manifest.permission.CAMERA), REQUEST_CAMERA)
        }
    }

    override fun onRequestPermissionsResult(
        requestCode: Int, permissions: Array<out String>, grantResults: IntArray
    ) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (requestCode == REQUEST_CAMERA) {
            permissionGranted = grantResults.isNotEmpty() && grantResults[0] == PackageManager.PERMISSION_GRANTED
            if (!permissionGranted) {
                Toast.makeText(this, "Cần quyền Camera để quét QR", Toast.LENGTH_SHORT).show()
                finish()
                return
            }
            maybeStartCamera(null)
        }
    }

    private fun maybeStartCamera(holder: SurfaceHolder?) {
        if (!permissionGranted || !surfaceReady) return
        val surface = (holder ?: savedHolder)?.surface ?: return
        nativeStartCamera(surface)
    }

    override fun onDestroy() {
        nativeStopCamera()
        super.onDestroy()
    }

    // ---- Called FROM native code (cpp/jni_bridge.cpp) via JNI ----------

    /** A QR code was successfully decoded; hand it to the keyboard and close. */
    fun onQrDecoded(text: String) {
        runOnUiThread {
            QrResultHolder.pendingText = text
            finish()
        }
    }

    /** Fatal camera error reported by native code. */
    fun onQrError(reason: String) {
        runOnUiThread {
            Toast.makeText(this, "Lỗi camera: $reason", Toast.LENGTH_SHORT).show()
            finish()
        }
    }
}
