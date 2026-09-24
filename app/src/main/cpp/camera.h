// camera.h
// Native camera capture (Android Camera2 NDK API, no CameraX/Java) that
// feeds frames into the embedded quirc QR decoder. This is the native
// replacement for the original app's "CameraX + ML Kit" QrScanActivity logic.
#ifndef QRKB_CAMERA_H
#define QRKB_CAMERA_H

#include <android/native_window.h>

#ifdef __cplusplus
extern "C" {
#endif

// Called from an internal camera callback thread when a QR code has been
// decoded. `text` is a UTF-8, NUL-terminated string owned by the caller of
// the callback - copy it if you need it after the callback returns.
typedef void (*qr_result_cb)(const char *text, void *user_data);

// Called on fatal camera errors (no camera, permission denied at the driver
// level, device disconnected, etc) with a short human-readable reason.
typedef void (*qr_error_cb)(const char *reason, void *user_data);

// Opens the back camera, starts a repeating preview onto `preview_window`
// (owned by the caller / the Activity's SurfaceView) and starts decoding
// QR codes from the camera frames in the background. Returns 0 on success.
//
// NOTE: the CAMERA runtime permission must already be granted before this
// is called - that permission dialog is one of the few things Android
// requires to originate from an Activity (see QrScanActivity.kt), and
// this native code assumes it has already been granted.
int camera_start(ANativeWindow *preview_window, qr_result_cb on_result,
                  qr_error_cb on_error, void *user_data);

// Tears down the capture session, closes the camera and releases all
// native camera/image-reader resources. Safe to call even if not started.
void camera_stop(void);

#ifdef __cplusplus
}
#endif

#endif // QRKB_CAMERA_H
