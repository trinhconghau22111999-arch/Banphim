package com.example.qrkeyboardnative

import android.app.Activity
import android.os.Bundle
import android.provider.Settings
import android.view.Gravity
import android.view.inputmethod.InputMethodManager
import android.widget.Button
import android.widget.LinearLayout
import android.widget.TextView

/**
 * IRREDUCIBLE ANDROID GLUE.
 *
 * Android requires a real Activity class (subclass of android.app.Activity,
 * compiled to a Dex/Java bytecode entry point) to appear as a launcher app
 * and to open system Settings screens. There is no NDK/native substitute for
 * this - it is not "keyboard logic", it is OS plumbing, identical in spirit
 * to the original project's MainActivity.kt.
 *
 * This class contains zero application logic: no keyboard code, no QR code,
 * no camera code. It just builds a tiny UI with a button that opens the
 * system "Manage keyboards" screen, then hands off to InputMethodManager.
 */
class MainActivity : Activity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val layout = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            gravity = Gravity.CENTER
            setPadding(48, 48, 48, 48)
        }

        val title = TextView(this).apply {
            text = getString(R.string.app_name)
            textSize = 20f
        }

        val instructions = TextView(this).apply {
            text = getString(R.string.instructions)
            textSize = 15f
            setPadding(0, 32, 0, 32)
        }

        val openSettingsBtn = Button(this).apply {
            text = getString(R.string.open_settings)
            setOnClickListener {
                startActivity(android.content.Intent(Settings.ACTION_INPUT_METHOD_SETTINGS))
            }
        }

        val switchKeyboardBtn = Button(this).apply {
            text = "Chuyển bàn phím ngay"
            setOnClickListener {
                val imm = getSystemService(INPUT_METHOD_SERVICE) as InputMethodManager
                imm.showInputMethodPicker()
            }
        }

        layout.addView(title)
        layout.addView(instructions)
        layout.addView(openSettingsBtn)
        layout.addView(switchKeyboardBtn)
        setContentView(layout)
    }
}
