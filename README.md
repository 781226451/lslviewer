# LSL Viewer

基于 C++17 / Qt Quick 的 [Lab Streaming Layer](https://labstreaminglayer.org/) 流查看器：
发现网络上的 LSL 流，查看流属性、完整 XML 描述以及结构化的 `desc` metadata。程序不读取或绘制实时信号。

QML 界面位于 `qml/Main.qml`，构建时嵌入可执行文件。`liblsl` 与 `pugixml` 由 vcpkg 提供。

## 仓库结构

```
.
├─ CMakeLists.txt / CMakePresets.json / vcpkg.json
├─ src/                 # metadata 解析、列表模型、控制器（编成 lslviewer_core 静态库）
│  └─ app/              # GUI 与 probe 的入口，不属于 core
├─ tests/               # C++ 单测与 QML 契约测试（fixtures/ 为数据用例）
├─ qml/Main.qml         # 唯一 UI
├─ tools/mock/          # Python 仿真数据源，仅供测试
├─ docs/                # 开发方式与决策记录
└─ build/               # 构建产物（git 忽略）
```

## 环境要求

- Windows x64 + Visual Studio C++ 生成工具（MSVC）
- CMake 3.24 及以上，Ninja
- Qt 6.9.1 for MSVC 2022，位于 `E:\Qt\6.9.1\msvc2022_64`
- vcpkg 位于 `E:\vcpkg`

以上路径写在 `CMakePresets.json` 中，安装位置不同就改那里。

## 构建与运行

在 Visual Studio Developer PowerShell 中执行（保证 `cl.exe` 可用）：

```powershell
cmake --preset windows-release
cmake --build --preset windows-release
```

首次配置会由 vcpkg 安装 `liblsl` 和 `pugixml`。产物在 `build/release/`，其中 `windeployqt`
的 post-build 步骤会把 Qt 运行时部署到可执行文件旁边。

运行 `build/release/lslviewer.exe`，确保网络上有可发现的 LSL 发布端；程序启动后自动发现。
若在构建环境之外启动，需保留随附的 Qt 与 LSL 运行时 DLL。

调试构建把两处 `windows-release` 换成 `windows-debug`。

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
