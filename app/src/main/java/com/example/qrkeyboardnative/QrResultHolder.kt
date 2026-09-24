package com.example.qrkeyboardnative

/**
 * IRREDUCIBLE ANDROID GLUE.
 *
 * QrScanActivity and QrKeyboardService are two different Android
 * components. Android (not this app) decides how they communicate:
 * either via Intent extras/setResult (Activity results) or, since the IME
 * is a Service and not the caller of startActivityForResult, a simple
 * static in-process holder. There is no native/C++ concept of "Android
 * component" for this data to flow through instead.
 *
 * This class carries ONLY the decoded QR string - no scanning, decoding or
 * keyboard logic lives here.
 */
object QrResultHolder {
    @Volatile
    var pendingText: String? = null
}
