# Android Audio Shim

Enables [Simple Voice Chat](https://modrinth.com/plugin/simple-voice-chat) to work on Android devices running MojoLauncher or PojavLauncher.

## How it works

Android does not implement the Java Sound API (`javax.sound.sampled`) used by Simple Voice Chat. This mod provides a Java SPI implementation that routes audio through native Android APIs:

- **Microphone** - AAudio (Android 8.0+)
- **Speaker** - OpenSL ES (stereo, positional audio)

The native library (`libaudioshim.so`) is bundled inside the JAR and extracted automatically on first launch.

## Requirements

- Android 8.0+
- arm64-v8a architecture
- MojoLauncher or PojavLauncher
- Minecraft 1.12.2+
- Forge or Fabric

## Installation

1. Add `audio-shim.jar` to your mods folder
2. Launch Minecraft
3. Open Simple Voice Chat settings and select the new microphone source

> The mod will not appear in the mod list - this is expected behavior.

To remove the native library error messages in chat, open `voicechat-client.properties` and set `use_natives=false`.

## Building from source

### Requirements

- [Termux](https://termux.dev) on Android (or any Linux aarch64 environment with clang)
- OpenJDK 17
- clang
- OpenSL ES headers (`pkg install openjdk-17 clang` in Termux)

### Build

```bash
bash build.sh
```

Output files:
- `/sdcard/audio-shim.jar` - the mod JAR
- `/sdcard/libaudioshim.so` - the native library (also bundled inside the JAR)

### Deploy

```bash
# Copy JAR to mods folder
cp /sdcard/audio-shim.jar /path/to/mods/

# Copy .so to JVM library path (MojoLauncher)
su -c "cp /sdcard/libaudioshim.so /data/data/git.artdeell.mojo/files/libaudioshim.so"
```

## Author

GRED?
