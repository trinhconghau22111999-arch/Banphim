// jni_bridge.cpp
// The ONLY file that speaks JNI. Everything else (keyboard.c, renderer.c,
// camera.cpp, qr/quirc.c) is plain, Android-agnostic C/C++. This file's job
// is narrow: translate Android lifecycle/touch/surface events into calls
// into the native engine, and translate native results back into the two
// Java-only calls Android leaves no way around: InputConnection text
// commits and CAMERA permission / Activity launch.
#include <jni.h>
#include <android/native_window_jni.h>
#include <android/log.h>
#include <mutex>
#include <cstring>

extern "C" {
#include "keyboard.h"
}
#include "renderer.h"
#include "camera.h"

#define TAG "QRKeyboardNative"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)

namespace {
JavaVM *g_vm = nullptr;

// ---- Keyboard (QrKeyboardService) state ----------------------------------
kb_state_t g_kb;
ANativeWindow *g_kbWindow = nullptr;
int g_pressedIdx = -1;
std::mutex g_kbMutex;

void redraw() {
    if (g_kbWindow) render_keyboard(g_kbWindow, &g_kb, g_pressedIdx);
}

// ---- QR scanner (QrScanActivity) state -----------------------------------
std::mutex g_qrMutex;
jobject g_qrActivityRef = nullptr; // global ref, set while the scanner activity is alive

JNIEnv *attachEnv(bool *didAttach) {
    JNIEnv *env = nullptr;
    if (g_vm->GetEnv((void **)&env, JNI_VERSION_1_6) == JNI_EDETACHED) {
        g_vm->AttachCurrentThread(&env, nullptr);
        *didAttach = true;
    } else {
        *didAttach = false;
    }
    return env;
}

void onQrResult(const char *text, void * /*user_data*/) {
    std::lock_guard<std::mutex> lock(g_qrMutex);
    if (!g_qrActivityRef) return;
    bool attached = false;
    JNIEnv *env = attachEnv(&attached);
    jclass cls = env->GetObjectClass(g_qrActivityRef);
    jmethodID mid = env->GetMethodID(cls, "onQrDecoded", "(Ljava/lang/String;)V");
    jstring jtext = env->NewStringUTF(text);
    env->CallVoidMethod(g_qrActivityRef, mid, jtext);
    env->DeleteLocalRef(jtext);
    env->DeleteLocalRef(cls);
    if (attached) g_vm->DetachCurrentThread();
}

void onQrError(const char *reason, void * /*user_data*/) {
    std::lock_guard<std::mutex> lock(g_qrMutex);
    if (!g_qrActivityRef) return;
    bool attached = false;
    JNIEnv *env = attachEnv(&attached);
    jclass cls = env->GetObjectClass(g_qrActivityRef);
    jmethodID mid = env->GetMethodID(cls, "onQrError", "(Ljava/lang/String;)V");
    jstring jreason = env->NewStringUTF(reason);
    env->CallVoidMethod(g_qrActivityRef, mid, jreason);
    env->DeleteLocalRef(jreason);
    env->DeleteLocalRef(cls);
    if (attached) g_vm->DetachCurrentThread();
}

} // namespace

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void * /*reserved*/) {
    g_vm = vm;
    kb_init(&g_kb);
    return JNI_VERSION_1_6;
}

// =====================  QrKeyboardService.kt bridge  ======================

extern "C" JNIEXPORT void JNICALL
Java_com_example_qrkeyboardnative_QrKeyboardService_nativeInit(JNIEnv *, jobject) {
    std::lock_guard<std::mutex> lock(g_kbMutex);
    kb_init(&g_kb);
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_qrkeyboardnative_QrKeyboardService_nativeSurfaceChanged(
        JNIEnv *env, jobject /*thiz*/, jobject surface, jint width, jint height) {
    std::lock_guard<std::mutex> lock(g_kbMutex);
    if (g_kbWindow) { ANativeWindow_release(g_kbWindow); g_kbWindow = nullptr; }
    g_kbWindow = ANativeWindow_fromSurface(env, surface);
    kb_layout(&g_kb, (float)width, (float)height);
    redraw();
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_qrkeyboardnative_QrKeyboardService_nativeSurfaceDestroyed(JNIEnv *, jobject) {
    std::lock_guard<std::mutex> lock(g_kbMutex);
    if (g_kbWindow) { ANativeWindow_release(g_kbWindow); g_kbWindow = nullptr; }
}

// Handles a touch event (action: 0=DOWN/MOVE i.e. "finger is down, update
// highlight", 1=UP i.e. "finger lifted here, fire this key", 2=CANCEL).
//
// IMPORTANT: DOWN and MOVE must share code 0. A real finger touch always
// generates ACTION_MOVE events even for a stationary tap (sensor jitter),
// so MOVE must only update the highlighted key - not cancel the touch -
// or almost no tap would ever register on real hardware.
extern "C" JNIEXPORT void JNICALL
Java_com_example_qrkeyboardnative_QrKeyboardService_nativeTouchEvent(
        JNIEnv *env, jobject thiz, jfloat x, jfloat y, jint action) {
    std::lock_guard<std::mutex> lock(g_kbMutex);
    int idx = kb_hit_test(&g_kb, x, y);

    if (action == 0) { // DOWN or MOVE: just follow the finger, don't fire yet
        g_pressedIdx = idx;
        redraw();
        return;
    }
    if (action == 2) { // CANCEL (e.g. parent intercepted the gesture)
        g_pressedIdx = -1;
        redraw();
        return;
    }

    // ACTION_UP: fire whatever key is under the finger at release time.
    // (Sliding off the keyboard entirely before lifting, idx == -1,
    // correctly cancels the tap - same "slide to cancel" behavior as a
    // normal Android keyboard.)
    int firedIdx = idx;

    const kb_key_t *key = (firedIdx >= 0) ? kb_tap(&g_kb, firedIdx) : nullptr;
    redraw();
    if (!key) return;

    jclass cls = env->GetObjectClass(thiz);
    switch (key->action) {
        case KA_CHAR: {
            // key->label already holds the shift-resolved character (see
            // kb_tap() in keyboard.c).
            jmethodID mid = env->GetMethodID(cls, "commitText", "(Ljava/lang/String;)V");
            jstring js = env->NewStringUTF(key->label);
            env->CallVoidMethod(thiz, mid, js);
            env->DeleteLocalRef(js);
            break;
        }
        case KA_SPACE: {
            jmethodID mid = env->GetMethodID(cls, "commitText", "(Ljava/lang/String;)V");
            jstring js = env->NewStringUTF(" ");
            env->CallVoidMethod(thiz, mid, js);
            env->DeleteLocalRef(js);
            break;
        }
        case KA_BACKSPACE: {
            jmethodID mid = env->GetMethodID(cls, "sendBackspace", "()V");
            env->CallVoidMethod(thiz, mid);
            break;
        }
        case KA_ENTER: {
            jmethodID mid = env->GetMethodID(cls, "sendEnter", "()V");
            env->CallVoidMethod(thiz, mid);
            break;
        }
        case KA_QR_SCAN: {
            jmethodID mid = env->GetMethodID(cls, "launchQrScanner", "()V");
            env->CallVoidMethod(thiz, mid);
            break;
        }
        default:
            break; // KA_SHIFT / mode switches only affect layout, already redrawn
    }
    env->DeleteLocalRef(cls);
}

// ======================  QrScanActivity.kt bridge  ========================

extern "C" JNIEXPORT void JNICALL
Java_com_example_qrkeyboardnative_QrScanActivity_nativeStartCamera(
        JNIEnv *env, jobject thiz, jobject surface) {
    {
        std::lock_guard<std::mutex> lock(g_qrMutex);
        if (g_qrActivityRef) env->DeleteGlobalRef(g_qrActivityRef);
        g_qrActivityRef = env->NewGlobalRef(thiz);
    }
    ANativeWindow *window = ANativeWindow_fromSurface(env, surface);
    camera_start(window, onQrResult, onQrError, nullptr);
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_qrkeyboardnative_QrScanActivity_nativeStopCamera(JNIEnv *env, jobject) {
    camera_stop();
    std::lock_guard<std::mutex> lock(g_qrMutex);
    if (g_qrActivityRef) { env->DeleteGlobalRef(g_qrActivityRef); g_qrActivityRef = nullptr; }
}
