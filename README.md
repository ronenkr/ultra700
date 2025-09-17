# MT6261 bare-metal build (CMake + GCC)

This workspace builds two binaries with the bundled GCC ARM toolchain:
- bootloader (linked with `MT6261A_boot.ld`)
- payload/system (linked with `MT6261A.ld`)

Outputs are placed under `build/bin` and include `.elf`, `.bin`, `.hex`, and optional signed `.bin` if `tools/mtk_sign.exe` exists.

## Prerequisites
- Windows with PowerShell
- No global installs required; uses local `gcc/` toolchain
- CMake 3.18+

## Configure and build
```powershell
# From the repository root
cmake -S . -B build -G "Ninja" -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-arm-none-eabi.cmake
cmake --build build --target bootloader.elf --config Release
cmake --build build --target payload.elf   --config Release

clean and make
cmake --build build --clean-first -j
```
If Ninja is not installed, replace the generator with "MinGW Makefiles" and use `cmake --build build -j`.

## Notes
- `TARGET_BOOTLOADER` and `TARGET_SYSTEM` macros select configuration via `systemconfig.h`.
- Post-build steps convert ELF to BIN/HEX and call `tools/mtk_sign.exe <file>.bin` when present.
- Adjust optimization by selecting the CMake build type (Debug/Release).

## Customization
- Override toolchain location:
```powershell
cmake -S . -B build -G "Ninja" -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-arm-none-eabi.cmake -DGNU_ARM_TOOLCHAIN_DIR="C:/path/to/gcc-arm-none-eabi"
```
- Edit include paths or source lists in `CMakeLists.txt` if you add/remove modules.