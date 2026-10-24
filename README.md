# LSL Viewer

基于 C++17 / Qt Quick 的 [Lab Streaming Layer](https://labstreaminglayer.org/) 流查看器：
发现网络上的 LSL 流，查看流属性、完整 XML 描述以及结构化的 `desc` metadata。程序不读取或绘制实时信号。

QML 界面位于 `qml/Main.qml`，构建时嵌入可执行文件。`liblsl` 与 `pugixml` 由 vcpkg 提供。

## 仓库结构

```
.
├─ CMakeLists.txt / vcpkg.json
├─ src/                 # metadata 解析、列表模型、控制器（编成 lslviewer_core 静态库）
│  └─ app/              # GUI 与 probe 的入口，不属于 core
├─ tests/               # C++ 单测与 QML 契约测试（fixtures/ 为数据用例）
├─ qml/Main.qml         # 唯一 UI
├─ tools/mock/          # Python 仿真数据源，仅供测试
├─ docs/                # 开发方式与决策记录
└─ build/               # 构建产物（git 忽略）
```

## 环境要求

- Windows x64 + Visual Studio C++ 生成工具（MSVC），或 macOS + Xcode Command Line Tools
- CMake 3.24 及以上，Ninja
- Qt 6.9 及以上，使用与编译器和目标架构匹配的套件
- vcpkg；本地仓库和可执行程序需支持 `vcpkg.json` 中锁定的基线

仓库不提供机器相关的 CMake presets。通过命令行、CLion 的 CMake options，或本地
`CMakeUserPresets.json`（git 忽略）设置 Qt 与 vcpkg 路径。

## 构建与运行

在 Visual Studio Developer PowerShell 中执行（保证 `cl.exe` 可用）：

```powershell
cmake -S . -B build/release -G Ninja -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_PREFIX_PATH=E:/Qt/6.9.1/msvc2022_64 `
  -DCMAKE_TOOLCHAIN_FILE=E:/vcpkg/scripts/buildsystems/vcpkg.cmake `
  -DVCPKG_TARGET_TRIPLET=x64-windows
cmake --build build/release
```

首次配置会由 vcpkg 安装 `liblsl` 和 `pugixml`。产物在 `build/release/`，其中 `windeployqt`
的 post-build 步骤会把 Qt 运行时部署到可执行文件旁边。

运行 `build/release/lslviewer.exe`，确保网络上有可发现的 LSL 发布端；程序启动后自动发现。
若在构建环境之外启动，需保留随附的 Qt 与 LSL 运行时 DLL。

调试构建使用 `-DCMAKE_BUILD_TYPE=Debug` 和独立目录 `build/debug`。

### macOS / CLion

以 Apple Silicon、Qt 6.9.3 和 CLion 管理的 vcpkg 为例，在项目根目录执行：

```sh
cmake -S . -B cmake-build-debug -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_PREFIX_PATH=/Applications/Qt/6.9.3/macos \
  -DCMAKE_TOOLCHAIN_FILE="$HOME/.vcpkg-clion/vcpkg/scripts/buildsystems/vcpkg.cmake"
cmake --build cmake-build-debug
ctest --test-dir cmake-build-debug --output-on-failure
open "cmake-build-debug/LSL Viewer.app"
```

路径按实际安装位置调整。CLion 中选择 Debug、Ninja 和 AppleClang 工具链，在 CMake options
中设置上述 `CMAKE_PREFIX_PATH` 与 `CMAKE_TOOLCHAIN_FILE`，然后 Reload CMake Project。
使用 CLion 自带的 CMake/Ninja 时，无需另外把它们加入终端的 `PATH`。

CLion 的 Debug 与 Release profile 配置独立。新增 Release 时，选择 Release 构建类型和
`cmake-build-release` 构建目录，并在该 profile 的 **CMake options** 中同样填写（替换为本机路径）：

```text
-DCMAKE_PREFIX_PATH=/Applications/Qt/6.9.3/macos
-DCMAKE_TOOLCHAIN_FILE=/Users/songle/.vcpkg-clion/vcpkg/scripts/buildsystems/vcpkg.cmake
```

若配置时报找不到 `LSLConfig.cmake`，先检查该 profile 的命令中是否包含
`CMAKE_TOOLCHAIN_FILE`；vcpkg 工具链负责安装和查找 LSL。`CMAKE_PREFIX_PATH` 用于选择
预期的 Qt 安装，缺少时可能自动找到 Homebrew 的另一套 Qt。补齐后执行
**Reset Cache and Reload Project**，清除之前缓存的包路径。
终端构建 Release 时，将上例的 `Debug` 改为 `Release`，所有 `cmake-build-debug` 改为
`cmake-build-release`，保留 Qt 和 vcpkg 两个路径参数。

若同时安装了 Xcode 与 Command Line Tools，出现 `ld: tapi error`、`unknown architecture`
等 SDK 链接错误，应查看 vcpkg 的 `buildtrees/detect_compiler/config-<triplet>-rel-err.log`。
例如旧版 AppleClang 17 的链接器无法读取 macOS 27 SDK 中的 `arm64e.x1`，会导致编译器
检测失败，并连带报 `CMAKE_CXX_COMPILER not set`。

应确保编译器和 SDK 来自同一套兼容的工具。本机 Command Line Tools 已更新时，在 CLion
的 **Settings → Build, Execution, Deployment → CMake → Debug → Environment** 中添加：

```text
DEVELOPER_DIR=/Library/Developer/CommandLineTools
```

这是环境变量，不是 `-D` CMake option；它会传给 vcpkg 的子进程。设置后执行
**Reset Cache and Reload Project**。其他构建 profile 也需设置同一环境变量。
终端中先执行以下命令，再运行上面的配置和构建命令（仅影响当前 shell）：

```sh
export DEVELOPER_DIR=/Library/Developer/CommandLineTools
xcrun clang --version
xcrun --show-sdk-path
```

### macOS 安装包（DMG）

先按上面的说明配置 Release（包括 Qt、vcpkg 路径与 `DEVELOPER_DIR`），然后在项目根目录执行：

```sh
export DEVELOPER_DIR=/Library/Developer/CommandLineTools
cmake --build cmake-build-release --parallel
cpack --config cmake-build-release/CPackConfig.cmake -B build/packages
```

若终端未配置 CMake 的 `PATH`，使用 CLion 自带的完整路径：

```sh
"/Users/songle/Applications/CLion.app/Contents/bin/cmake/mac/aarch64/bin/cmake" --build cmake-build-release --parallel
"/Users/songle/Applications/CLion.app/Contents/bin/cmake/mac/aarch64/bin/cpack" --config cmake-build-release/CPackConfig.cmake -B build/packages
```

当前 Apple Silicon 构建生成 `build/packages/LSLViewer-0.1.0-macOS26.0-arm64.dmg`。
打开 DMG，将 **LSL Viewer.app** 拖入 **Applications**，然后从“应用程序”启动。
首次访问本地网络时允许访问，以便发现 LSL 流。

打包会在临时安装目录中使用对应 Qt 的 `macdeployqt` 收集 Qt frameworks、插件和 QML
模块；接收方无需安装 Qt 或 vcpkg。普通构建目录中的 `.app` 用于开发，分发使用 DMG 内的版本。
此安装包只包含 GUI，probe 与测试程序不随包安装。

当前产物以 **arm64、最低 macOS 26.0** 为目标，覆盖 26.6.2，仍可在 macOS 27 上运行。
项目在启用编译器前默认设置 `CMAKE_OSX_DEPLOYMENT_TARGET=26.0`，并自动加载
`cmake/triplets/arm64-osx.cmake`，让 vcpkg 依赖也以 26.0 为目标。首次切换会重建依赖。
旧构建目录建议执行 Reset Cache and Reload Project（终端配置可加 `--fresh`）。
打包会检查所有 Mach-O 的最低系统版本，发现高于目标的库时终止。产物不是 Intel / Universal 安装包；
发布前仍需在目标系统上验证实际运行。
默认使用 ad-hoc 本地签名，未做 Developer ID 签名和 Apple 公证；下载到其他 Mac 后，
Gatekeeper 可能要求在“系统设置 → 隐私与安全性”中确认“仍要打开”。公开分发前应完成正式签名和公证。

### vcpkg 基线错误

如果报 `failed to git show ...:versions/baseline.json`，先更新 CMake 实际使用的 vcpkg
仓库。以下命令针对当前 `vcpkg.json` 的基线，要求 vcpkg 工作区干净且可以快进：

```sh
git -C "$HOME/.vcpkg-clion/vcpkg" fetch origin b42e8ed051d17a7f95b9d2a3140c1f9296423712
git -C "$HOME/.vcpkg-clion/vcpkg" merge --ff-only b42e8ed051d17a7f95b9d2a3140c1f9296423712
"$HOME/.vcpkg-clion/vcpkg/bootstrap-vcpkg.sh" -disableMetrics
```

仅 `fetch` 会补齐提交对象，但旧 checkout 的版本数据库仍可能报 `no version database entry`；
快进后需运行 bootstrap，使 vcpkg 可执行程序支持新脚本的数据格式。Windows 上使用实际的
vcpkg 路径，并改为运行 `bootstrap-vcpkg.bat -disableMetrics`。完成后重新配置 CMake。
若 `CMAKE_CXX_COMPILER not set` 紧随 vcpkg 安装失败出现，应先解决前面的 vcpkg 错误。

## 构建目标

| 目标 | 类型 | 用途 |
|---|---|---|
| `lslviewer` | GUI 可执行文件 | 产品本体，QML 由 `qml/Main.qml` 嵌入 |
| `lslviewer_core` | 静态库 | `metadata` / `models` / `viewercontroller`，不含窗口 |
| `lslviewer_probe` | 控制台可执行文件 | 驱动同一 core，把一条流导出为 JSON，供脚本与测试使用 |
| `tst_metadata`、`tst_qmlcontract` | 测试 | 通过 `ctest` 运行 |

## 测试

```powershell
ctest --test-dir build/release --output-on-failure
```

- `tst_metadata` —— 逐行展开 `tests/fixtures/*.xml`，与同名 `*.expected.json` 对比；
  新增用例只加数据文件，不改代码。覆盖嵌套、属性与 text/tail、Unicode、空 desc、
  无 desc、纯空白，以及畸形 XML 抛异常。
- `tst_qmlcontract` —— offscreen 加载 `qml/Main.qml` 并注入真实 `ViewerController`，
  断言无 QML 警告、UI 依赖的 objectName、8 个属性、4 个方法以及两个模型的角色名。

## Probe

`lslviewer_probe` 复用产品同一份 core，因此测的是真实发现与解析路径：

```powershell
build\release\lslviewer_probe.exe --stream name:LSLViewer --timeout 10 --out dump.json
```

输出包含 `streams`、`details`、`rows`、`xml`、`status`、`selectedUid`。退出码：
`0` 成功、`2` 超时或发现失败、`3` 无匹配流、`4` 参数错误。

## 端到端测试

`tools/mock` 提供确定性的 LSL 仿真数据源，配合 probe 覆盖"发布 → 发现 → 解析 → 断言"整条链路：

```powershell
cd tools/mock
uv sync
uv run pytest
```

详见 [docs/development.md](docs/development.md)。

## 许可证

本项目采用 [MIT 许可证](LICENSE)。仓库内的 `tools/mock` 测试工装一并适用。
