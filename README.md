# QR Keyboard Native (C/C++ rewrite)

Bản viết lại của [trinhconghau22111999-arch/QR-CODE](https://github.com/trinhconghau22111999-arch/QR-CODE)
với **toàn bộ logic ứng dụng bằng C/C++**, thay cho Kotlin thuần trong bản gốc.

## ĐỌC TRƯỚC: vì sao không thể "100% C/C++"

Bản gốc là một **bàn phím hệ thống Android** (Input Method Service). Android chỉ cho
phép đăng ký IME, mở màn hình xin quyền camera, và gõ chữ vào ô nhập liệu của app khác
thông qua các **class Java/Kotlin bắt buộc** của chính hệ điều hành
(`InputMethodService`, `InputConnection`, `Activity.requestPermissions`) — không có API
NDK/native nào thay thế được, vì đây là ranh giới bảo mật & vòng đời do chính Android
Framework kiểm soát, không phải giới hạn kỹ thuật của C/C++.

Vì vậy dự án này làm đúng mức tối đa có thể:

| Thành phần | Ngôn ngữ | Vì sao |
|---|---|---|
| Vẽ bàn phím (QWERTY, phím, layout, hiệu ứng khi bấm) | **C** (`keyboard.c`, `renderer.c`) | Rasterize trực tiếp bằng con trỏ pixel lên `ANativeWindow`, không dùng `Canvas`/`View` của Android |
| Bắt sự kiện chạm, tính phím nào được bấm | **C** (`keyboard.c`) | Hit-test thuần toán học |
| Camera, giải mã QR | **C/C++** (`camera.cpp` + thư viện `quirc`) | Camera2 NDK (`libcamera2ndk`) + `AImageReader`, decode bằng `quirc` (thư viện QR thuần C) — chạy 100% offline, không CameraX, không ML Kit |
| Cầu nối JNI | **C++** (`jni_bridge.cpp`) | Bắt buộc phải có một lớp phiên dịch giữa native và JVM |
| `InputMethodService`, `InputConnection`, xin quyền Camera, mở Activity | **Kotlin** (3 file nhỏ) | Đây là các class/API của chính Android OS, không tồn tại ở tầng NDK |

3 file Kotlin còn lại (`MainActivity.kt`, `QrKeyboardService.kt`, `QrScanActivity.kt`)
**không chứa bất kỳ logic bàn phím, vẽ, hay QR nào** — mỗi hàm chỉ dài 1-3 dòng, gọi
thẳng vào native qua JNI hoặc gọi API hệ thống mà Android bắt buộc phải xuất phát từ
Activity/Service. Toàn bộ phần "não" của app nằm trong `app/src/main/cpp/`.

## Tính năng — giống 1:1 với bản gốc

- [x] Bàn phím hệ thống thật (`InputMethodService`), hoạt động trong mọi app (Zalo,
      Messenger, trình duyệt, ghi chú...) — không phải overlay giả.
- [x] QWERTY cơ bản, phím Shift, chuyển trang 123 ⇄ ABC, dấu cách, Enter, xoá (`<-`).
- [x] Nút **[QR]** trên bàn phím → mở lớp quét (Activity) phía trên bàn phím.
- [x] Quét & giải mã QR **hoàn toàn offline trên máy**, không gửi ảnh lên server nào
      (quirc chạy cục bộ, giống tinh thần "on-device" của ML Kit ở bản gốc).
- [x] Tự động chèn nội dung mã QR vào đúng ô nhập liệu đang gõ dở.
- [x] Xin quyền Camera lần đầu dùng nút QR.

## Cấu trúc dự án

```
QRKeyboardNative/
├── .github/workflows/build-apk.yml   # tự build (+ ký) APK trên GitHub Actions
├── gradlew / gradlew.bat / gradle/wrapper/   # Gradle Wrapper (đã kèm sẵn jar)
├── keystore/key.qr.banphim.jks       # keystore release (git-ignored, không commit)
├── keystore.properties.example       # mẫu cấu hình ký local (an toàn để commit)
├── .gitignore
├── app/
│   ├── build.gradle
│   └── src/main/
│       ├── AndroidManifest.xml
│       ├── java/com/example/qrkeyboardnative/   # Kotlin glue tối thiểu (không có logic)
│       │   ├── MainActivity.kt
│       │   ├── QrKeyboardService.kt
│       │   ├── QrScanActivity.kt
│       │   └── QrResultHolder.kt
│       ├── res/...
│       └── cpp/                                  # TOÀN BỘ LOGIC Ở ĐÂY
│           ├── CMakeLists.txt
│           ├── jni_bridge.cpp      # duy nhất file "biết" JNI
│           ├── keyboard.h / .c     # layout, state machine, hit-test
│           ├── renderer.h / .c     # rasterizer + bitmap font 5x7
│           ├── font5x7.h           # bảng font (Adafruit-GFX, BSD license)
│           ├── camera.h / .cpp     # Camera2 NDK + tích hợp quirc
│           └── qr/                 # thư viện quirc (giải mã QR thuần C, ISC license)
├── build.gradle
├── settings.gradle
└── gradle.properties
```

## Build tự động trên GitHub (CI/CD) — không cần cài gì cả

Dự án đã kèm sẵn `gradlew`/`gradlew.bat`/`gradle-wrapper.jar` (khác với bản gốc chưa
có) và file `.github/workflows/build-apk.yml`. Chỉ cần đẩy code lên GitHub:

1. Tạo repo mới trên GitHub, đẩy (push) toàn bộ thư mục `QRKeyboardNative` lên.
2. Vào tab **Actions** của repo — workflow **"Build APK"** sẽ tự chạy (mỗi lần push,
   mở Pull Request, hoặc bấm **Run workflow** thủ công).
3. Workflow tự cài JDK 17, Android SDK, NDK 26 + CMake, rồi build cả
   `assembleDebug` và `assembleRelease`.
4. **Version tự tăng mỗi lần build**: `versionCode` = số thứ tự lần chạy workflow
   (`github.run_number` — chỉ tăng, không bao giờ lặp lại dù build lại commit cũ),
   `versionName` = `1.0.<số đó>` (vd `1.0.7`). Không cần commit gì thêm, không cần
   token ghi ngược vào repo — Android tự nhận app mới luôn "cao phiên bản hơn" app
   cũ khi cài đè.
5. Sau khi chạy xong (mục **Summary** của lần chạy), tải APK ở phần **Artifacts**:
   - `QRKeyboardNative-debug-apk`
   - `QRKeyboardNative-release-unsigned-apk` *(thực ra đã ký bằng debug-key sẵn để
     cài thử được ngay — xem ghi chú ký ứng dụng bên dưới)*
6. **Cách nhanh nhất để có bản Release đính kèm trực tiếp trong mục Releases**: tạo
   Git tag dạng `v1.0.0` rồi push tag đó lên —
   ```
   git tag v1.0.0
   git push origin v1.0.0
   ```
   Workflow sẽ tự tạo GitHub Release và đính kèm cả 2 APK vào đó.

## Ký ứng dụng (signing) — `key.qr.banphim`

Dự án đã có sẵn **keystore release thật**, alias `key.qr.banphim`, tại
`keystore/key.qr.banphim.jks` (RSA 2048-bit, PKCS12, hiệu lực 10.000 ngày).
File này **KHÔNG được commit lên Git** (đã liệt trong `.gitignore`) — bạn cần tự
thêm nó vào máy/CI theo 1 trong 2 cách sau.

**Mật khẩu keystore** (store password = key password, do định dạng PKCS12 bắt
buộc trùng nhau): xem trong file bạn tải kèm `key.qr.banphim-passwords.txt`.
⚠️ Vì mật khẩu này được sinh trong phiên làm việc với Claude, để an toàn tuyệt
đối cho bản phát hành thật, bạn nên **tự tạo lại keystore mới** bằng lệnh dưới
đây và tự giữ mật khẩu, không dùng lại mật khẩu đã thấy trong chat:
```
keytool -genkeypair -v -keystore keystore/key.qr.banphim.jks \
  -alias key.qr.banphim -keyalg RSA -keysize 2048 -validity 10000
```

### Cách 1 — Build local bằng Android Studio
1. Copy `keystore.properties.example` → `keystore.properties` (cùng cấp thư mục gốc).
2. Điền `storePassword` và `keyPassword` thật vào đó.
3. Đặt file `.jks` vào đúng đường dẫn `keystore/key.qr.banphim.jks`.
4. Build `assembleRelease` bình thường — Gradle tự đọc `keystore.properties`.

### Cách 2 — Build tự động trên GitHub Actions
Vào **Settings → Secrets and variables → Actions** của repo, thêm 4 Secret:

| Tên Secret | Giá trị |
|---|---|
| `KEYSTORE_BASE64` | Nội dung base64 của file `.jks` (xem file `key.qr.banphim-base64.txt` đính kèm) |
| `KEYSTORE_PASSWORD` | Mật khẩu keystore |
| `KEY_ALIAS` | `key.qr.banphim` |
| `KEY_PASSWORD` | Mật khẩu key (= mật khẩu keystore) |

Workflow sẽ tự giải mã base64 thành file `.jks`, build và ký APK release bằng
đúng key `key.qr.banphim`. Nếu chưa thêm Secret, build vẫn chạy bình thường
nhưng tự động rơi về ký bằng debug-key (không lỗi, chỉ không phải chữ ký chính
thức).

## Build thủ công bằng Android Studio

1. Cài **Android Studio** (Koala trở lên) kèm **NDK** và **CMake** (Settings → SDK
   Manager → SDK Tools → tick "NDK (Side by side)" và "CMake").
2. **Open** thư mục `QRKeyboardNative`. Android Studio sẽ tự cấu hình Gradle +
   Native (CMake) build — cần máy có internet ở bước tải dependencies.
3. Cắm điện thoại thật (có camera, bật USB debugging) — khuyến khích dùng máy thật vì
   camera thật cần thiết để test quét QR. Bấm **Run ▶**.

## Cách dùng trên điện thoại

Giống hệt bản gốc:

1. Mở app, bấm **"Mở cài đặt bàn phím"**.
2. Bật **QR Keyboard Native** trong Cài đặt hệ thống → Bàn phím.
3. Vào ô nhập văn bản bất kỳ, chuyển sang **QR Keyboard Native**.
4. Bấm **[QR]** → cấp quyền Camera lần đầu → đưa mã QR vào khung hình → nội dung tự
   động được gõ vào ô nhập liệu.

## Giấy phép thư viện bên thứ ba đã nhúng

- `app/src/main/cpp/qr/` — **quirc** (Daniel Beer, ISC License) — xem
  `qr/LICENSE_QUIRC.txt`.
- `app/src/main/cpp/font5x7.h` — bảng bitmap font cổ điển đi kèm **Adafruit-GFX-Library**
  (BSD License) — xem `LICENSE_ADAFRUIT_FONT.txt`.

## Có thể mở rộng thêm (như gợi ý ở bản gốc)

- Gõ tiếng Việt có dấu (Telex/VNI) — viết bộ xử lý dấu thuần C, gắn vào `keyboard.c`.
- Rung/haptic khi bấm phím — gọi `Vibrator` qua JNI (một API Java-only khác của Android).
- Quét QR từ ảnh trong thư viện — decode ảnh (ví dụ bằng `stb_image`, thuần C) rồi đưa
  buffer trực tiếp vào `quirc`, không cần Camera2.
- Lưu lịch sử mã đã quét — ghi vào SQLite qua NDK (`libsqlite3` nếu tự build) hoặc file
  nhị phân đơn giản bằng `fopen`/`fwrite` thuần C.
