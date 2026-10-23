# LSL Viewer (C++)

This directory contains a C++17/Qt Quick implementation of the Python LSL Viewer. It uses vcpkg to provide `liblsl` and `pugixml`, and the QML UI is embedded in the executable.

The UI is shared with the Python project: both load `../qml/Main.qml` at the repository root. Edit only that file; the C++ build compiles it into the executable.

## Requirements

- Windows x64 with Visual Studio C++ build tools (MSVC)
- CMake 3.24 or later and Ninja
- Qt 6.9.1 for MSVC 2022 at `E:\Qt\6.9.1\msvc2022_64`
- vcpkg at `E:\vcpkg`

The paths above are set in `CMakePresets.json`; adjust them there if your installation differs.

## Configure and build

Run these commands from this directory in a Visual Studio developer PowerShell so that `cl.exe` is available:

```powershell
cmake --preset windows-release
cmake --build --preset windows-release
```

The vcpkg manifest installs `liblsl` and `pugixml` into the configured triplet on the first configure. Build output is written to `build/release/lslviewer.exe`. The post-build step deploys Qt runtime files when the Qt installation provides `windeployqt`; vcpkg's CMake integration deploys required runtime DLLs alongside the executable.

For a debug build, use `windows-debug` instead of `windows-release` in both commands.

## Run

Run `build/release/lslviewer.exe` while an LSL publisher is available on the network. The application starts stream discovery automatically. If launching outside the build environment, keep the deployed Qt and LSL runtime DLLs alongside the executable.