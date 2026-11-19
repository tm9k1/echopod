# EchoPod (Enhanced Firmware Fork)

This repository is an enhanced fork of [BasilSkyWalk/echopod](https://github.com/BasilSkyWalk/echopod), the replacement firmware for the Snowsky (FiiO) Echo Mini digital audio player.

For upstream documentation, hardware research credits, and original base specifications, refer to [BasilSkyWalk/echopod](https://github.com/BasilSkyWalk/echopod).

---

## What Changed in This Fork

This fork focuses exclusively on hardware-level firmware refinements, audio engine optimizations, and display enhancements to elevate the listening and visual experience on the physical Echo Mini hardware:

### 1. No SD Card Requirement — Standalone Internal Storage
- Full standalone operation directly on the Echo Mini's internal 8 GB eMMC flash memory, indexing songs, database (`library.db`), cover art, and settings without needing a microSD card.
- *Useful when you just got your Echo Mini and want to load up your favorite albums immediately without having to buy or insert an SD card.*

### 2. Clearer Text (Tuned by Hardware Pixel LED Alignment)
- Font glyph rasterization and baseline offsets are tuned specifically to align with the physical subpixel LED stripe grid of the Echo Mini LCD panel, eliminating font blurriness and chromatic halos.
- *You'll appreciate it when glancing down at track titles, small artist metadata, or bitrate tags while outdoors without having to squint.*

### 3. Clearer Album Art
- Overhauled RGB565 color quantization and dithering pipeline to preserve shadow depth, contrast, and vibrant saturation from embedded album artwork tags without blotchy banding noise.
- *Useful when your favorite album sleeves have subtle gradients or dark artwork that previously looked crushed or washed out on tiny displays.*

### 4. Better CoverFlow for All Display Orientations
- Real-time 3D quad-distortion perspective carousel with ambient lighting falloff and floor reflections, completely optimized for both Landscape ($320 \times 170$) and Portrait ($170 \times 320$) orientations.
- *You'll appreciate it when using the player one-handed in portrait mode and flicking through your album sleeves with the exact same fluid 3D physics as landscape.*

### 5. Super-Gapless Playback
- Audio decoder lookahead pre-buffering and zero-click boundary handoffs eliminate micro-stutters and silence gaps across FLAC, ALAC, MP3, AAC, and Opus.
- *Useful when listening to live concert recordings, continuous DJ mixes, or concept albums where even a split-second pause or click ruins the musical immersion.*

### 6. Crash-Safe Instant Resume
- Atomic session persistence flushes exact track playback timestamps, volume levels, and queue state to internal flash before power-off.
- *You'll appreciate it when your battery runs out or you power down mid-track on your commute, and your song picks up at the exact second where you left off on next boot.*

### 7. On-Device Gapless & Engine Diagnostics
- Integrated real-time audio pipeline logger and diagnostics menu screen tracking track transition handoffs, buffer latency, and decoder health.
- *Useful when testing new audio rips or obscure formats and verifying microsecond-accurate gapless transitions right on the device without tethering to a PC.*

---

## Installing on Your Echo Mini

1. Download or build the firmware binary **`HIFIEC90.IMG`**.
2. Connect your Snowsky Echo Mini to your computer via USB.
3. Copy **`HIFIEC90.IMG`** directly to the root of the player's internal storage (or SD card root).
4. Safely eject the player and disconnect the USB cable.
5. The player will detect the update file, flash the enhanced firmware, and reboot into EchoPod.

---

## Building the Firmware from Source

To build the release firmware binary (`build/release/HIFIEC90.IMG`):

### Prerequisites
- CMake 3.25+
- Ninja build system
- `arm-none-eabi-gcc` toolchain (with `newlib` support)
- Python 3

### Build Command

```sh
cmake --preset release
cmake --build --preset release
```

The resulting firmware image is generated at `build/release/HIFIEC90.IMG`.

---

## License

EchoPod is open-source software licensed under the Apache License 2.0.
Snowsky and FiiO are trademarks of their respective owners. This project is independently developed and is not affiliated with or endorsed by FiiO or Snowsky.
