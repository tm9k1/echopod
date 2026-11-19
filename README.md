# EchoPod (Fork)

This repository is a fork of the original [BasilSkyWalk/echopod](https://github.com/BasilSkyWalk/echopod) firmware project for the Snowsky (FiiO) Echo Mini digital audio player.

For the upstream documentation, hardware references, and original user guides, please visit the upstream repository at [BasilSkyWalk/echopod](https://github.com/BasilSkyWalk/echopod).

---

## What is New in This Fork

This fork enhances the native C desktop simulator (`platform/sim`) and firmware display pipelines with photorealistic hardware simulation, rotating edge controls, artwork rendering improvements, and desktop ergonomics:

### 1. Photorealistic Borderless Chassis Simulator
- **Borderless Hardware Window**: Replaced standard OS window chrome with a borderless window (`SDL_WINDOW_BORDERLESS`) mapped to the physical proportions of the Snowsky Echo Mini.
- **Native Drag Anywhere**: Integrated `SDL_SetWindowHitTest` allowing the user to click and grab the casing anywhere to drag it across the desktop.
- **High-Resolution Casing Texture**: Loads high-res official chassis assets (`echo_mini_casing.bmp`).

### 2. 4-Way Rotating Physical Edge Buttons
- Clicking `[ROTATE ↷]` or pressing <kbd>R</kbd> rotates the casing $90^\circ$ clockwise ($0^\circ \rightarrow 90^\circ \rightarrow 180^\circ \rightarrow 270^\circ$).
- Physical buttons travel attached to the rotating chassis edge in physically consistent order:
  - **$0^\circ$ Landscape**: Top shoulder buttons (`POWER`, `VOL-`, `VOL+`, `PLAY`, `MENU`, `BACK`).
  - **$90^\circ$ Portrait**: Right flank buttons (`POWER`, `VOL-`, `VOL+`, `PLAY`, `MENU`, `BACK` top-to-bottom).
  - **$180^\circ$ Inverted Landscape**: Bottom shoulder buttons (`BACK`, `MENU`, `PLAY`, `VOL+`, `VOL-`, `POWER` left-to-right).
  - **$270^\circ$ Inverted Portrait**: Left flank buttons (`BACK`, `MENU`, `PLAY`, `VOL+`, `VOL-`, `POWER` top-to-bottom).
- Tactile feedback includes button depression, beveled drop shadows, and an active Power LED indicator.

### 3. Standby & Soft Power-Off / Power-On Cycle
- Holding the `POWER` button or <kbd>P</kbd> no longer closes the simulator window.
- The firmware saves its state, gracefully shuts down, turns off the LCD display, and dims the Power LED to dark green.
- Holding `POWER` or <kbd>P</kbd> again while in standby boots the Echo Mini back up with animated splash and resumes normal operation.
- Closing is handled cleanly via the `[✕]` control in the top bar.

### 4. Full Cover Flow 3D & Now Playing Artwork Rendering
- Fixed and verified the Cover Flow 3D quad-distortion perspective pipeline in the simulator, including lighting gradient falloff and floor reflections.
- Restored full Now Playing album cover box rendering ($112 \times 112$).

### 5. Host Storage Integration
- Added a `[FILES 📁]` button and <kbd>F</kbd> keyboard shortcut to immediately open the host file manager (Windows File Explorer) on the internal storage directory.

### 6. Zero-Comments Codebase Rule
- All modifications strictly comply with zero-comments formatting enforced by `tools/check_no_comments.py` (0 comments across all 776 code files).

---

## Building and Running the Simulator

### Windows (MSYS2 / MinGW / Clang)

```sh
cmake -B build/sim -G Ninja -DCMAKE_BUILD_TYPE=Debug
ninja -C build/sim echo_sim
./build/sim/platform/sim/echo_sim --scale 2 --internal ./internal_storage --system-dir ./internal_storage/.echo
```

### Controls

| Action | Mouse | Keyboard |
| :--- | :--- | :--- |
| Move Window | Click & drag casing body | — |
| Power Off / Standby | Hold `[POWER ⏻]` (~2s) | Hold <kbd>P</kbd> |
| Power On | Hold `[POWER ⏻]` in standby (~0.8s) | Hold <kbd>P</kbd> |
| Backlight Sleep | Tap `[POWER ⏻]` | Tap <kbd>P</kbd> |
| Rotate 90° CW | Click `[ROTATE ↷]` | <kbd>R</kbd> |
| Open Music Storage | Click `[FILES 📁]` | <kbd>F</kbd> |
| Volume / Up | Click `[VOL -]` | <kbd>Up Arrow</kbd> |
| Volume / Down | Click `[VOL +]` | <kbd>Down Arrow</kbd> |
| Play / Pause / Select | Click `[PLAY ▶‖]` | <kbd>Enter</kbd> / <kbd>Space</kbd> |
| Menu / Options | Click `[MENU ≡]` | <kbd>M</kbd> / <kbd>Tab</kbd> |
| Back / Return | Click `[BACK ↩]` | <kbd>Backspace</kbd> / <kbd>Esc</kbd> |
| Close Simulator | Click `[✕]` | — |
