# 开发与测试方式

## 定位

| 组件 | 角色 | 是否交付 |
|---|---|---|
| `src/` | **产品**，用户交付物为 Windows 的 `lslviewer.exe` 或 macOS 的 `LSL Viewer.app` | 是 |
| `qml/Main.qml` | 唯一的 UI，由 C++ 侧拥有并在构建时嵌入可执行文件 | 编译进 exe |
| `tools/mock/` | LSL 仿真数据源，仅供测试使用 | 否 |
| `tests/fixtures/` | XML 输入与期望输出，C++ 单测使用 | 否 |

## 目录

```
.
├─ CMakeLists.txt / vcpkg.json
├─ src/                    # metadata / models / viewercontroller（core 静态库）
│  └─ app/                 # main.cpp、probe_main.cpp（入口，不属于 core）
├─ tests/                  # tst_metadata、tst_qmlcontract、fixtures/
├─ qml/Main.qml            # 唯一 UI（qt_add_resources 嵌入 exe）
├─ tools/mock/             # Python / pylsl 仿真发布器，仅供测试
├─ build/                  # 构建产物（git 忽略）
└─ docs/
```

## 五条规则

1. **产品代码只进 `src/`。** XML 解析、行展开、数据模型与状态机由 C++ 产品实现；测试工装只负责提供输入和验证输出。
2. **mock 只发布，不解析。** 它可以构造 `StreamInfo`、拼 XML、推样本，但不得判断"这个 XML 应该展开成几行"——那是产品的职责，也是被测对象。
3. **mock 必须确定性。** name / type / source_id / XML 全部写死在 `scenarios.py`，禁止随机值和时间戳；UID 由 liblsl 生成，测试按 name 匹配。
4. **任何 C++ 行为改动都要配用例。** 解析类改动配 `tests/fixtures/`；发现/交互类改动配 `tools/mock` 场景或端到端断言。
5. **协议栈同源。** `tools/mock` 的 pylsl 自带一份 liblsl，与产品用的 vcpkg liblsl 应保持 major.minor 一致。

## 测试分层

| 层 | 载体 | 覆盖 |
|---|---|---|
| 单元 | `ctest` → `tst_metadata` | 6 组 fixture：嵌套、属性与 text/tail、Unicode、空 desc、无 desc、纯空白；外加畸形 XML 抛异常 |
| 契约 | `ctest` → `tst_qmlcontract` | offscreen 加载 `qml/Main.qml`，断言无 QML 警告、5 个 objectName、`viewer` 的 8 属性 4 方法、两个模型的角色名 |
| 端到端 | `pytest` → `tools/mock` + `lslviewer_probe` | 6 个场景：真实 liblsl 发现 → 选择 → 解析 → JSON 断言 |
| 手工 | `lslviewer-mock` + GUI | 视觉、交互手感、滚动性能、流消失 |

`lslviewer_probe` 是产品核心代码的命令行入口，不是测试替身：它链接 `lslviewer_core` 并驱动同一个 `ViewerController`。

## 常用命令

```powershell
# 1. 先按 README 的平台说明配置；以下以 Windows build/release 为例
cmake --build build/release

# 2. 单元 + 契约测试
ctest --test-dir build/release --output-on-failure

# 3. 起一条仿真流（场景名见下）
cd tools/mock
uv sync
uv run lslviewer-mock --scenario nested --duration 120

# 4. 用产品取一条流的 JSON（在仓库根执行）
build\release\lslviewer_probe.exe --stream name:LSLViewer --timeout 10 --out dump.json

# 5. 端到端测试（会自己起 mock、跑 probe）
cd tools/mock
uv run pytest
```

`lslviewer_probe` 退出码：`0` 成功、`2` 超时或发现失败、`3` 没有匹配的流、`4` 参数错误。

## macOS 打包（DMG）

以下命令在项目根目录执行，使用本机 CLion 自带的 CMake、CPack 和 Ninja。
先设置当前终端的环境（新开终端后需重新执行，路径按实际安装位置调整）：

```sh
export DEVELOPER_DIR=/Library/Developer/CommandLineTools
export PATH="$HOME/Applications/CLion.app/Contents/bin/cmake/mac/aarch64/bin:$PATH"
```

首次打包或更换 Qt / vcpkg 路径时，配置 Release：

```sh
cmake -S . -B cmake-build-release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_MAKE_PROGRAM="$HOME/Applications/CLion.app/Contents/bin/ninja/mac/aarch64/ninja" \
  -DCMAKE_PREFIX_PATH=/Applications/Qt/6.9.3/macos \
  -DCMAKE_TOOLCHAIN_FILE="$HOME/.vcpkg-clion/vcpkg/scripts/buildsystems/vcpkg.cmake"
```

若旧缓存曾选中其他 Qt 或工具链，在上述命令的 `cmake` 后添加 `--fresh` 重新配置。
CLion 中配置时，Release profile 也需单独设置这两个依赖路径和 `DEVELOPER_DIR`，
详见 [README 的 macOS / CLion 说明](../README.md#macos--clion)。

每次代码更新后，依次构建、测试、打包；前一步成功后再执行下一步：

```sh
cmake --build cmake-build-release --parallel
ctest --test-dir cmake-build-release --output-on-failure
cpack --config cmake-build-release/CPackConfig.cmake -B build/packages
```

CPack 会安装 GUI 到暂存目录，通过 `macdeployqt` 收集 Qt frameworks、QML 模块与所需
窗口插件，执行 ad-hoc 签名和签名校验，再创建包含 Applications 快捷方式的 DMG。
`cpack` 本身不编译代码，所以必须先执行构建。普通构建目录中的 `.app` 仍用于开发，
分发时使用 DMG 内部署完整的版本。

当前产物为 `build/packages/LSLViewer-0.1.0-macOS26.0-arm64.dmg`；文件名中的版本来自
`CMakeLists.txt` 的 `project(... VERSION ...)`，`macOS26.0` 来自部署目标。打开 DMG，将 **LSL Viewer.app** 拖入
**Applications** 即可安装，接收方无需另装 Qt 或 vcpkg。

可检查磁盘映像完整性：

```sh
hdiutil verify build/packages/LSLViewer-0.1.0-macOS26.0-arm64.dmg
```

当前安装包以 **Apple Silicon、macOS 26.0 或更新版本** 为目标。项目在 `project()` 前默认
设置 `CMAKE_OSX_DEPLOYMENT_TARGET=26.0`，并通过 `cmake/triplets/arm64-osx.cmake` 将
vcpkg 依赖的 `VCPKG_OSX_DEPLOYMENT_TARGET` 固定为 26.0。更换部署目标需要同步修改
应用和依赖配置、重新构建；只修改 `Info.plist` 不会改变二进制兼容性。
从旧的 27.0 构建切换时，用上面的配置命令加 `--fresh`；vcpkg 会根据新 triplet 的哈希重建依赖。
打包时会检查所有 Mach-O（含 Qt 库的各架构切片）的最低系统版本，高于目标则报错。
此检查不能代替目标系统实测。默认包未做 Developer ID 签名和 Apple 公证。
首次在目标 Mac 上启动时，应验证界面加载、允许本地网络访问，并确认可以发现测试流。

若在受限自动化环境中出现 `hdiutil: create failed - 设备未配置`，可能是磁盘映像服务
被沙箱限制；可在 macOS 普通终端中重新执行上述 `cpack` 命令。

## mock 场景

| 场景 | 用途 |
|---|---|
| `simple` | 冒烟：一条 EEG 流，两层 metadata |
| `nested` | 深嵌套、重复 `channel`、中英混排 |
| `empty-desc` | 无 metadata |
| `huge` | 64 通道、长文本、>10 KB XML |
| `unicode` | 中文、emoji、RTL |
| `many` | 20 条并发流 |
| `lifecycle` | 默认 10 秒后除首条外全部消失（手工验证用，未纳入自动测试） |

## 完成的定义（DoD）

- [ ] 按 README 的平台说明配置成功，`cmake --build <构建目录>` 通过
- [ ] `ctest --test-dir build/release` 通过
- [ ] 受影响的 fixture 或 mock 场景已更新，`uv run pytest` 通过
- [ ] `qml/Main.qml` 的改动已由 `tst_qmlcontract` 验证
- [ ] 两端 liblsl 版本仍一致
- [ ] 用户可见变更已记录在 `README.md` 或提交信息中

## UI 迭代方式

先执行 `uv run lslviewer-mock --scenario nested` 启动仿真流，修改 QML 后重新构建并运行应用。
也可用 Qt 的 `qml` 工具单独加载 `Main.qml`，临时注入 stub `viewer` 对象用于界面预览；
stub 只放本地、不进仓库，正式界面通过 `tst_qmlcontract` 验证。
