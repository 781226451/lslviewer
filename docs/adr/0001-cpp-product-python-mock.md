# ADR-0001：C++ 作为唯一产品，Python 降级为测试工装

- 状态：已采纳
- 日期：2026-10-24

## 背景

仓库最初有两条平行分支：`dev-python`（PySide6 实现）与 `dev-cpp`（Qt6/C++ 实现），共享根目录的 `qml/Main.qml`。两端各自实现了 controller、模型、metadata 展开，逻辑重复约 1000 行；C++ 侧的 `metadata.cpp` 注释直接引用 Python `ElementTree` 的 `text`/`tail` 语义，说明规格事实上寄生在 Python 代码里。

## 决策

1. **C++ 为唯一产品**，唯一交付物是 `lslviewer.exe`。
2. **Python 不再是第二实现**：其 viewer 代码（controller / models / metadata / app / qt_properties / stream_types / tests）冻结在 tag `prototype/py-viewer`，从主干移除，不再维护。
3. **Python 转为测试工装**：只保留 LSL 发布能力，迁入 `tools/mock/`，用于构造可控的流与 metadata，以测试 C++ 产品。
4. QML 由 C++ 侧拥有；契约以 C++ 的 `ViewerController` 与 `qml/Main.qml` 为准。

## 理由

- 双实现的维护成本约 ×1.8–2.2，而此程序的规模（C++ 约 610 行）不足以摊薄它。
- 唯一真正需要"快速验证"的部分是 UI 与 metadata 的展示形态，而 UI 已在共享 QML 中，Python 实现的边际价值已经消失。
- 测试 C++ 产品需要的是**可控的数据源**，而不是第二份展示逻辑；后者无法验证前者。

## 后果

正面：

- 规格不再寄生在 Python 代码里，改为 `tests/fixtures/` 与 `tools/mock/src/lslviewer_mock/scenarios.py` 显式表达。
- 维护面减少一半；C++ 侧必须补测试，产品可测性被迫提升（拆 core 库、加 probe 命令行）。

负面 / 代价：

- 失去"用 Python 秒改 UI"的便利，QML 迭代要走 C++ 编译或本地 stub。
- 需要新增 C++ 测试基建（ctest、offscreen QML 测试）与端到端脚本，这部分工作量从 Python 侧转移过来。
- mock 与产品必须共用同一 liblsl 版本，多出一条对齐要求。

## 被否决的方案

- **保持双实现 + 契约测试（M3）**：管理成本与收益不匹配，且用户已明确只交付 C++。
- **C++ core + Shiboken 绑定（M2）**：Python 只做工具，不需要产品级的绑定层，属过度工程。
- **保留 Python viewer 作为 QML 预览器**：会持续漂移，且容易被误当作第二实现，风险高于便利。

## 后续修订

同日执行：C++ 工程从 `cplusplus/` 提到仓库根（根 `CMakeLists.txt`、`src/`、`tests/`），`python/`
目录整体删除，仅保留 `tools/mock` 作为测试工装。因此本文中的"主干"指仓库根这一套工程。
