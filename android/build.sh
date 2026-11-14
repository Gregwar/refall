#!/bin/bash
# Builds Refall's APK (arm64) without Gradle: the game with CMake and the NDK (libmain.so, a NativeActivity), then
# aapt2 (manifest, icon), zipalign and apksigner (a local debug key). The toolchain lives in .deps/android
# (JDK 17, the SDK's build-tools 34, platform 34, NDK r27). Output: build-android/refall.apk
#   adb install -r build-android/refall.apk
#   adb push ~/.cache/zentris /sdcard/Android/data/org.gregwar.refall/files/.cache/   (the songs and caches)
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
A=$ROOT/.deps/android
SDK=$A/sdk
NDK=$SDK/ndk/27.2.12479018
BT=$SDK/build-tools/34.0.0
PLATFORM=$SDK/platforms/android-34/android.jar
export JAVA_HOME=$A/jdk PATH=$A/jdk/bin:$PATH
OUT=$ROOT/build-android
mkdir -p $OUT

cmake -S $ROOT -B $OUT/cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake \
      -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-29 -DANDROID_STL=c++_static >/dev/null
cmake --build $OUT/cmake -j"$(nproc)" --target refall

# resources: the launcher icon (the logo, data/icon.png)
RES=$OUT/res
mkdir -p $RES/mipmap-xxxhdpi
cp $ROOT/data/icon.png $RES/mipmap-xxxhdpi/icon.png
cp -r $ROOT/android/res/. $RES/
$BT/aapt2 compile --dir $RES -o $OUT/res.zip
$BT/aapt2 link -o $OUT/unsigned.apk -I $PLATFORM --manifest $ROOT/android/AndroidManifest.xml $OUT/res.zip

# the native library
rm -rf $OUT/apk && mkdir -p $OUT/apk/lib/arm64-v8a
cp $OUT/cmake/libmain.so $OUT/apk/lib/arm64-v8a/
$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-strip $OUT/apk/lib/arm64-v8a/libmain.so
(cd $OUT/apk && zip -q -r $OUT/unsigned.apk lib)
$BT/zipalign -f -p 4 $OUT/unsigned.apk $OUT/aligned.apk

KEY=$A/debug.keystore
[ -f $KEY ] || keytool -genkeypair -keystore $KEY -storepass android -keypass android -alias refall \
                       -keyalg RSA -keysize 2048 -validity 10000 -dname "CN=Refall debug" >/dev/null 2>&1
$BT/apksigner sign --ks $KEY --ks-pass pass:android --key-pass pass:android --out $OUT/refall.apk $OUT/aligned.apk
rm -f $OUT/unsigned.apk $OUT/aligned.apk
ls -la $OUT/refall.apk
