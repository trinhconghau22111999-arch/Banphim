cmake_minimum_required(VERSION 3.22.1)
project(qrkeyboard_native)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)

add_library(qrkeyboard_native SHARED
    jni_bridge.cpp
    camera.cpp
    keyboard.c
    renderer.c
    qr/quirc.c
    qr/decode.c
    qr/identify.c
    qr/version_db.c
)

target_include_directories(qrkeyboard_native PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/qr
)

find_library(log-lib log)
find_library(android-lib android)
find_library(jnigraphics-lib jnigraphics)
find_library(camera2ndk-lib camera2ndk)
find_library(mediandk-lib mediandk)

target_link_libraries(qrkeyboard_native
    ${log-lib}
    ${android-lib}
    ${jnigraphics-lib}
    ${camera2ndk-lib}
    ${mediandk-lib}
)

target_compile_options(qrkeyboard_native PRIVATE -Wall -Wextra -Wno-unused-parameter)
