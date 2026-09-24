// camera.cpp
// Native QR camera pipeline: Camera2 NDK -> AImageReader (YUV_420_888) ->
// quirc decoder. No Java camera classes (CameraX, Camera1/2 Java APIs) are
// used anywhere in this file.
#include "camera.h"
#include <android/log.h>
#include <camera/NdkCameraManager.h>
#include <camera/NdkCameraDevice.h>
#include <camera/NdkCaptureRequest.h>
#include <media/NdkImageReader.h>
#include <mutex>
#include <cstring>
#include <cstdlib>

extern "C" {
#include "qr/quirc.h"
}

#define TAG "QRKeyboardNative"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

namespace {

constexpr int32_t kScanW = 640;
constexpr int32_t kScanH = 480;

struct CameraCtx {
    std::mutex mtx;
    ACameraManager *manager = nullptr;
    ACameraDevice *device = nullptr;
    ACaptureSessionOutputContainer *outputContainer = nullptr;
    ACaptureSessionOutput *previewOutput = nullptr;
    ACaptureSessionOutput *readerOutput = nullptr;
    ACameraOutputTarget *previewTarget = nullptr;
    ACameraOutputTarget *readerTarget = nullptr;
    ACaptureRequest *request = nullptr;
    ACameraCaptureSession *session = nullptr;
    AImageReader *reader = nullptr;
    ANativeWindow *previewWindow = nullptr;
    ANativeWindow *readerWindow = nullptr;
    struct quirc *qr = nullptr;

    qr_result_cb onResult = nullptr;
    qr_error_cb onError = nullptr;
    void *userData = nullptr;
    bool scanning = false;
} g;

void fail(const char *reason) {
    LOGE("camera error: %s", reason);
    if (g.onError) g.onError(reason, g.userData);
}

// --- AImageReader frame callback: decode QR from the Y (luma) plane -------
void onImageAvailable(void * /*ctx*/, AImageReader *reader) {
    std::lock_guard<std::mutex> lock(g.mtx);
    if (!g.scanning) return;

    AImage *image = nullptr;
    if (AImageReader_acquireLatestImage(reader, &image) != AMEDIA_OK || !image) return;

    int32_t width = 0, height = 0;
    AImage_getWidth(image, &width);
    AImage_getHeight(image, &height);

    uint8_t *yData = nullptr;
    int32_t yLen = 0;
    int32_t rowStride = 0;
    AImage_getPlaneData(image, 0, &yData, &yLen);
    AImage_getPlaneRowStride(image, 0, &rowStride);

    if (yData && width > 0 && height > 0 && g.qr) {
        if (quirc_resize(g.qr, width, height) == 0) {
            int qw, qh;
            uint8_t *qbuf = quirc_begin(g.qr, &qw, &qh);
            // Copy the luma plane into quirc's grayscale buffer row by row,
            // since the camera's row stride may be wider than the image.
            for (int y = 0; y < qh && y < height; y++) {
                memcpy(qbuf + y * qw, yData + y * rowStride, qw < width ? qw : width);
            }
            quirc_end(g.qr);

            int count = quirc_count(g.qr);
            for (int i = 0; i < count && g.scanning; i++) {
                struct quirc_code code;
                struct quirc_data data;
                quirc_extract(g.qr, i, &code);
                if (quirc_decode(&code, &data) == QUIRC_SUCCESS) {
                    char *text = (char *)malloc(data.payload_len + 1);
                    memcpy(text, data.payload, data.payload_len);
                    text[data.payload_len] = 0;
                    g.scanning = false; // stop after first successful decode
                    if (g.onResult) g.onResult(text, g.userData);
                    free(text);
                }
            }
        }
    }

    AImage_delete(image);
}

// --- Camera device callbacks ---------------------------------------------
void onDeviceDisconnected(void *, ACameraDevice *) { fail("camera_disconnected"); }
void onDeviceError(void *, ACameraDevice *, int error) {
    char buf[64];
    snprintf(buf, sizeof(buf), "camera_device_error_%d", error);
    fail(buf);
}

void onSessionReady(void *, ACameraCaptureSession *) { LOGI("capture session ready"); }
void onSessionActive(void *, ACameraCaptureSession *) { LOGI("capture session active"); }
void onSessionClosed(void *, ACameraCaptureSession *) { LOGI("capture session closed"); }

} // namespace

int camera_start(ANativeWindow *preview_window, qr_result_cb on_result,
                  qr_error_cb on_error, void *user_data) {
    std::lock_guard<std::mutex> lock(g.mtx);
    g.onResult = on_result;
    g.onError = on_error;
    g.userData = user_data;
    g.previewWindow = preview_window;

    g.qr = quirc_new();
    if (!g.qr || quirc_resize(g.qr, kScanW, kScanH) < 0) {
        fail("quirc_init_failed");
        return -1;
    }

    g.manager = ACameraManager_create();
    if (!g.manager) { fail("no_camera_manager"); return -1; }

    ACameraIdList *idList = nullptr;
    if (ACameraManager_getCameraIdList(g.manager, &idList) != ACAMERA_OK || idList->numCameras == 0) {
        fail("no_camera_found");
        return -1;
    }

    // Prefer a back-facing camera; fall back to the first camera available.
    const char *chosenId = idList->cameraIds[0];
    for (int i = 0; i < idList->numCameras; i++) {
        ACameraMetadata *chars = nullptr;
        if (ACameraManager_getCameraCharacteristics(g.manager, idList->cameraIds[i], &chars) == ACAMERA_OK) {
            ACameraMetadata_const_entry entry;
            if (ACameraMetadata_getConstEntry(chars, ACAMERA_LENS_FACING, &entry) == ACAMERA_OK &&
                entry.data.u8[0] == ACAMERA_LENS_FACING_BACK) {
                chosenId = idList->cameraIds[i];
                ACameraMetadata_free(chars);
                break;
            }
            ACameraMetadata_free(chars);
        }
    }

    ACameraDevice_StateCallbacks deviceCallbacks{};
    deviceCallbacks.onDisconnected = onDeviceDisconnected;
    deviceCallbacks.onError = onDeviceError;

    if (ACameraManager_openCamera(g.manager, chosenId, &deviceCallbacks, &g.device) != ACAMERA_OK) {
        fail("open_camera_failed");
        ACameraManager_deleteCameraIdList(idList);
        return -1;
    }
    ACameraManager_deleteCameraIdList(idList);

    // Image reader for QR decoding (separate stream from the preview).
    if (AImageReader_new(kScanW, kScanH, AIMAGE_FORMAT_YUV_420_888, 2, &g.reader) != AMEDIA_OK) {
        fail("image_reader_failed");
        return -1;
    }
    AImageReader_ImageListener listener{ nullptr, onImageAvailable };
    AImageReader_setImageListener(g.reader, &listener);
    AImageReader_getWindow(g.reader, &g.readerWindow);

    ACaptureSessionOutputContainer_create(&g.outputContainer);
    ACaptureSessionOutput_create(g.previewWindow, &g.previewOutput);
    ACaptureSessionOutputContainer_add(g.outputContainer, g.previewOutput);
    ACaptureSessionOutput_create(g.readerWindow, &g.readerOutput);
    ACaptureSessionOutputContainer_add(g.outputContainer, g.readerOutput);

    ACameraOutputTarget_create(g.previewWindow, &g.previewTarget);
    ACameraOutputTarget_create(g.readerWindow, &g.readerTarget);

    ACameraDevice_createCaptureRequest(g.device, TEMPLATE_PREVIEW, &g.request);
    ACaptureRequest_addTarget(g.request, g.previewTarget);
    ACaptureRequest_addTarget(g.request, g.readerTarget);

    ACameraCaptureSession_stateCallbacks sessionCallbacks{};
    sessionCallbacks.onReady = onSessionReady;
    sessionCallbacks.onActive = onSessionActive;
    sessionCallbacks.onClosed = onSessionClosed;

    if (ACameraDevice_createCaptureSession(g.device, g.outputContainer, &sessionCallbacks,
                                            &g.session) != ACAMERA_OK) {
        fail("create_session_failed");
        return -1;
    }

    if (ACameraCaptureSession_setRepeatingRequest(g.session, nullptr, 1, &g.request, nullptr) != ACAMERA_OK) {
        fail("start_preview_failed");
        return -1;
    }

    g.scanning = true;
    LOGI("camera started, scanning for QR codes");
    return 0;
}

void camera_stop() {
    std::lock_guard<std::mutex> lock(g.mtx);
    g.scanning = false;

    if (g.session) { ACameraCaptureSession_stopRepeating(g.session); ACameraCaptureSession_close(g.session); g.session = nullptr; }
    if (g.request) { ACaptureRequest_removeTarget(g.request, g.previewTarget); ACaptureRequest_removeTarget(g.request, g.readerTarget); ACaptureRequest_free(g.request); g.request = nullptr; }
    if (g.previewTarget) { ACameraOutputTarget_free(g.previewTarget); g.previewTarget = nullptr; }
    if (g.readerTarget) { ACameraOutputTarget_free(g.readerTarget); g.readerTarget = nullptr; }
    if (g.outputContainer) {
        if (g.previewOutput) { ACaptureSessionOutputContainer_remove(g.outputContainer, g.previewOutput); ACaptureSessionOutput_free(g.previewOutput); g.previewOutput = nullptr; }
        if (g.readerOutput) { ACaptureSessionOutputContainer_remove(g.outputContainer, g.readerOutput); ACaptureSessionOutput_free(g.readerOutput); g.readerOutput = nullptr; }
        ACaptureSessionOutputContainer_free(g.outputContainer);
        g.outputContainer = nullptr;
    }
    if (g.device) { ACameraDevice_close(g.device); g.device = nullptr; }
    if (g.reader) { AImageReader_delete(g.reader); g.reader = nullptr; g.readerWindow = nullptr; }
    if (g.manager) { ACameraManager_delete(g.manager); g.manager = nullptr; }
    if (g.qr) { quirc_destroy(g.qr); g.qr = nullptr; }

    LOGI("camera stopped");
}
