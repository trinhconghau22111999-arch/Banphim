package com.example.qrkeyboardnative

import android.content.Intent
import android.inputmethodservice.InputMethodService
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.View
import android.view.inputmethod.EditorInfo

/**
 * IRREDUCIBLE ANDROID GLUE.
 *
 * Android will only bind a system keyboard by instantiating a subclass of
 * android.inputmethodservice.InputMethodService declared in the manifest -
 * there is no NDK entry point the OS can call directly for this. Likewise,
 * android.view.inputmethod.InputConnection (used to actually type into the
 * focused app) is a Java-only interface with no native equivalent.
 *
 * Every method below does nothing but forward to native C/C++
 * (see cpp/jni_bridge.cpp, keyboard.c, renderer.c) or perform the one or
 * two framework calls (InputConnection, Intent) that only exist on the
 * Java/Kotlin side of Android. NO keyboard layout, drawing, touch-hit
 * testing, or QR logic is implemented here.
 */
class QrKeyboardService : InputMethodService() {

    private external fun nativeInit()
    private external fun nativeSurfaceChanged(surface: android.view.Surface, width: Int, height: Int)
    private external fun nativeSurfaceDestroyed()
    private external fun nativeTouchEvent(x: Float, y: Float, action: Int)

    companion object {
        init {
            System.loadLibrary("qrkeyboard_native")
        }
    }

    override fun onCreateInputView(): View {
        nativeInit()
        val surfaceView = SurfaceView(this)
        surfaceView.holder.addCallback(object : SurfaceHolder.Callback {
            override fun surfaceCreated(holder: SurfaceHolder) {}
            override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
                nativeSurfaceChanged(holder.surface, width, height)
            }
            override fun surfaceDestroyed(holder: SurfaceHolder) {
                nativeSurfaceDestroyed()
            }
        })
        surfaceView.setOnTouchListener { _, event ->
            // DOWN and MOVE both mean "finger is down, follow it" (code 0) -
            // MOVE must NOT be treated as a cancel, since real touch input
            // always emits MOVE events even for a perfectly still tap.
            // Only a genuine ACTION_CANCEL (code 2) aborts the key press.
            val action = when (event.actionMasked) {
                MotionEvent.ACTION_DOWN, MotionEvent.ACTION_MOVE -> 0
                MotionEvent.ACTION_UP -> 1
                else -> 2 // ACTION_CANCEL and anything unexpected
            }
            nativeTouchEvent(event.x, event.y, action)
            true
        }
        return surfaceView
    }

    override fun onStartInputView(info: EditorInfo?, restarting: Boolean) {
        super.onStartInputView(info, restarting)
        // Pick up a QR result produced while this app's QrScanActivity was
        // in the foreground (see QrResultHolder.kt / QrScanActivity.kt).
        QrResultHolder.pendingText?.let { text ->
            QrResultHolder.pendingText = null
            commitText(text)
        }
    }

    // ---- Called FROM native code (cpp/jni_bridge.cpp) via JNI ----------

    /** Commits literal text into the currently focused input field. */
    fun commitText(text: String) {
        currentInputConnection?.commitText(text, 1)
    }

    /** Deletes one character before the cursor. */
    fun sendBackspace() {
        currentInputConnection?.deleteSurroundingText(1, 0)
    }

    /** Sends an Enter/Done action, matching the focused field's IME action. */
    fun sendEnter() {
        val ic = currentInputConnection ?: return
        val action = currentInputEditorInfo?.imeOptions?.and(EditorInfo.IME_MASK_ACTION)
        if (action != null && action != EditorInfo.IME_ACTION_NONE &&
            action != EditorInfo.IME_ACTION_UNSPECIFIED
        ) {
            ic.performEditorAction(action)
        } else {
            ic.sendKeyEvent(KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ENTER))
            ic.sendKeyEvent(KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_ENTER))
        }
    }

    /**
     * Launches the QR scanner. Starting an Activity from a Service requires
     * FLAG_ACTIVITY_NEW_TASK - Android security policy, not app logic.
     */
    fun launchQrScanner() {
        val intent = Intent(this, QrScanActivity::class.java)
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
        startActivity(intent)
    }
}
