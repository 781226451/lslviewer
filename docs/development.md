# 开发与测试方式

## 定位

| 组件 | 角色 | 是否交付 |
|---|---|---|
| `src/` | **产品**，唯一的用户交付物（`lslviewer.exe`） | 是 |
| `qml/Main.qml` | 唯一的 UI，由 C++ 侧拥有并在构建时嵌入可执行文件 | 编译进 exe |
| `tools/mock/` | LSL 仿真数据源，仅供测试使用 | 否 |
| `tests/fixtures/` | XML 输入与期望输出，C++ 单测使用 | 否 |

Python 的第一版 viewer 实现已定型为原型，冻结在 tag `prototype/py-viewer`，`python/` 目录已从
主干删除；Python 现在只剩 `tools/mock` 这一份测试工装。

## 目录

```
.
├─ CMakeLists.txt / CMakePresets.json / vcpkg.json
├─ src/                    # metadata / models / viewercontroller（core 静态库）
│  └─ app/                 # main.cpp、probe_main.cpp（入口，不属于 core）
├─ tests/                  # tst_metadata、tst_qmlcontract、fixtures/
├─ qml/Main.qml            # 唯一 UI（qt_add_resources 嵌入 exe）
├─ tools/mock/             # pylsl 发布器（唯一的 Python 代码）
├─ build/                  # 构建产物（git 忽略）
└─ docs/
```

## 五条规则

1. **产品代码只进 `src/`。** Python 侧不得出现 XML 解析、行展开、数据模型、状态机等产品逻辑；出现即视为第二实现，必须删除。
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
# 1. 配置与构建（需 VS Developer PowerShell，或先 call vcvars64.bat）
cmake --preset windows-release
cmake --build --preset windows-release

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

- [ ] `cmake --preset windows-release && cmake --build --preset windows-release` 通过
- [ ] `ctest --test-dir build/release` 通过
- [ ] 受影响的 fixture 或 mock 场景已更新，`uv run pytest` 通过
- [ ] `qml/Main.qml` 的改动已由 `tst_qmlcontract` 验证
- [ ] 两端 liblsl 版本仍一致
- [ ] 用户可见变更已记录在 `README.md` 或提交信息中

## UI 迭代方式

QML 迭代不经过 Python。做法是：先 `uv run lslviewer-mock --scenario nested`，再运行 C++ 二进制改 QML。若嫌编译慢，可用 Qt 的 `qml` 工具单独加载 `Main.qml` 并临时注入 stub `viewer` 对象——**stub 只放本地、不进仓库**，否则又是一份实现。
