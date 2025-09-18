# Ultra 700 Servo Phone – Bare‑Metal Peripheral Demo

This repository is a test / exploration project for the Ultra 700 (MT6261 class) “servo” feature phone platform. It showcases a minimal, clean-room style bring‑up of core on‑board peripherals and subsystems under a fully bare‑metal (no RTOS) environment using a bundled GCC ARM toolchain and CMake/Ninja build flow.

## Implemented Features
| Area | Status | Notes |
|------|--------|-------|
| Clock / PLL init | Basic | Minimal enable sequence (TCM, core clocks) |
| GPIO | Working | Key→GPIO toggle mapping (12 lines) + safe GPIO helpers |
| Keypad | Working | Matrix scan + event reporting, demo overlay (optional) |
| LCD (ILI9341) | Working | Basic rectangles, text output (font lib) |
| USB CDC | Working | Console output (`USB_Print*`) + key logs |
| SD / MSDC | Working | Dual controller probe, SDHC/SDSC detect, block read |
| FAT32 (read‑only) | Minimal | Mount, root + directory listing, 8.3 names |
| Audio (tones / PCM) | Working | Tone generator + PCM sample playback |
| Power / PMU (partial) | Partial | Battery ADC sampling skeleton |
| Bootloader + Payload split | Working | Separate link scripts & signing |

## Repository Layout (Key Paths)
```
gcc/                     Bundled arm-none-eabi toolchain (9.3.1)
cmake/toolchain-*.cmake  Cross compile toolchain file
src/Bootloader           Minimal first stage (boot + SHA1 + handoff)
src/Application          Main application logic & demos
src/Application/Drivers  Peripheral drivers (keypad, LCD, FAT32, SD, USB CDC ...)
src/Lib/MT6261           Vendor/SoC register & low-level drivers
bin/                     Build artifacts (.elf/.bin/.hex + signed .bin)
tools/                   Signing tool, ninja, flashing helpers, monitor script
build/, build-debug/     Generated (ignored) CMake build trees (Release/Debug)
```

## Quick Start
Requires only:
- Windows + PowerShell
- CMake ≥ 3.21 (presets support)
- No external ARM GCC install (uses `./gcc/`)

### 1. Configure & Build (Release)
```powershell
cmake --preset arm-release
cmake --build --preset arm-release
```

### 2. Debug Variant
```powershell
cmake --preset arm-debug
cmake --build --preset arm-debug --target payload.elf
```

### 3. Simplified Script
The root script `build.ps1` wraps presets and adds convenience flags:
```powershell
./build.ps1                 # Release build (payload + bootloader)
./build.ps1 -Config Debug   # Debug build
./build.ps1 -Target payload.elf
./build.ps1 -Clean -Monitor # Clean rebuild then start serial monitor
```

### 4. Serial Monitor
`./build.ps1 -Monitor` launches `monitor.ps1` (auto‑detects COM port, prints key events, filesystem logs, etc.).

## Boot Flow
1. Bootloader initializes minimal clocks / memory, verifies (or just copies) payload.
2. Jumps to payload reset handler located per `MT6261A.ld`.
3. Payload initializes full driver set (GPIO, LCD, keypad, USB, SD) then enters main event loop handling keypad events and demo actions.

## Key Demo Bindings (Example)
| Key | Action |
|-----|--------|
| `1` | Toggle flash LED |
| `W` | Draw text stamp on LCD |
| `E` | Play PCM sample + print CPU frequency |
| `A` | Short melody playback |
| `P` | Mount SD + list FAT32 root directory |
| Mapped set (44,58,32,18,4,57,45,31,17,20,34,48) | Toggle GPIO0..GPIO11 |

## FAT32 Support (Read‑Only Snapshot)
- Single volume mount (first FAT32 partition or VBR at LBA0)
- 512‑byte sector assumption
- Root + directory listing (8.3 names, LFN skipped)
- Simple file open/read (sequential clusters)
- Future expansion: recursive listing, path parsing, LFN assembly, write support

## SD Driver Highlights
- Clean-room register mapping (MSDC0/MSDC2)
- CMD0 / CMD8 / ACMD41 / CMD2 / CMD3 / CMD9 / CMD7 / CMD17
- R2 / R6 response parsing with snapshot status handling
- Capacity extraction (CSD v1 & v2) printed in MB

## Build Artifacts
After a successful build (`bin/`):
- `bootloader.elf|bin|hex|map`
- `payload.elf|bin|hex|map`
- Signed `*.bin` if `tools/mtk_sign.exe` present (POST_BUILD step)

## Customization Tips
- Add new drivers under `src/Application/Drivers/` and append to `PAYLOAD_SRCS` in `CMakeLists.txt`.
- Toggle optimization: pass `-Config Debug` to `build.ps1` or use debug preset.
- Extend key actions inside `appinit.c` switch statement.

## Cleaning
```powershell
./build.ps1 -Clean          # Release tree
./build.ps1 -Config Debug -Clean
```
or manually remove `build*/` directories.

## Toolchain Updates
Replace contents of `gcc/` with a newer arm-none-eabi release (adjust versioned folder names if necessary). Ensure `CMAKE_TOOLCHAIN_FILE` remains valid.

## Roadmap (Potential Next Steps)
- Recursive FAT32 directory traversal
- Long File Name (LFN) assembly
- Basic shell over USB CDC (mount, list, hexdump sectors)
- SPI flash integration & persistence layer
- Power management / sleep states
- More robust error reporting & logging abstraction

## License
This is a test / educational project. Original SoC header content may be subject to its respective vendor terms. All new code in this repository is intended for learning and experimentation.

---
Feel free to open issues or extend the demos with additional peripherals (camera interface, RTC alarms, etc.).