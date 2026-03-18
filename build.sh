#!/data/data/com.termux/files/usr/bin/bash
set -e

if ! command -v javac &>/dev/null; then pkg install -y openjdk-17; fi
if ! command -v clang  &>/dev/null; then pkg install -y clang; fi

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SRC="$SCRIPT_DIR/src"
OUT="$SCRIPT_DIR/out"
JAR="$SCRIPT_DIR/audio-shim.jar"
SO="$SCRIPT_DIR/libaudioshim.so"

JNI_H=$(find /data/data/com.termux/files/usr -name "jni.h" 2>/dev/null | head -1)
JNI_INCLUDE=$(dirname "$JNI_H")
JNI_MD_DIR="$JNI_INCLUDE/linux"
SLES_INCLUDE="/data/data/com.termux/files/usr/include"

echo "[*] jni.h: $JNI_H"
echo "[*] Compiling native library..."

clang -shared -fPIC \
    -I"$JNI_INCLUDE" \
    -I"$JNI_MD_DIR" \
    -I"$SLES_INCLUDE" \
    -o "$SO" \
    "$SCRIPT_DIR/native/audio_bridge.c" \
    -ldl -lOpenSLES

echo "[*] .so built"

mkdir -p "$OUT"
echo "[*] Compiling Java..."

javac -source 8 -target 8 -d "$OUT" \
    "$SRC/de/maxhenkel/shim/NativeAudio.java" \
    "$SRC/de/maxhenkel/shim/NativeAudioOutput.java" \
    "$SRC/de/maxhenkel/shim/AndroidTargetDataLine.java" \
    "$SRC/de/maxhenkel/shim/AndroidSourceDataLine.java" \
    "$SRC/de/maxhenkel/shim/AndroidMixer.java" \
    "$SRC/de/maxhenkel/shim/AndroidMixerProvider.java"

echo "[*] Packaging JAR..."

cp -r "$SCRIPT_DIR/META-INF" "$OUT/"
cp "$SCRIPT_DIR/resources/mcmod.info" "$OUT/mcmod.info"
cp "$SCRIPT_DIR/resources/fabric.mod.json" "$OUT/fabric.mod.json"
mkdir -p "$OUT/natives"
cp "$SO" "$OUT/natives/libaudioshim.so"

cd "$OUT" && jar cf "$JAR" . && cd "$SCRIPT_DIR"

cp "$SO"  /sdcard/libaudioshim.so
cp "$JAR" /sdcard/audio-shim.jar

echo ""
echo "Done! Files saved to /sdcard/"
