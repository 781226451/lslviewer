# ADR-0001：C++ 产品与 Python 测试工装的职责划分

- 状态：已采纳
- 日期：2026-10-24

## 背景

LSL Viewer 使用 C++ / Qt Quick 实现流发现与 metadata 展示。测试需要可控的 LSL 数据源，
以验证流发现、XML 解析和界面契约。

## 决策

1. **产品使用 C++ 实现**，交付物是 Windows 的 `lslviewer.exe` 或 macOS 的 `LSL Viewer.app`。
2. **Python 用于测试工装**：`tools/mock/` 构造可控的 LSL 流与 metadata，供产品测试使用。
3. QML 由 C++ 侧拥有；契约以 C++ 的 `ViewerController` 与 `qml/Main.qml` 为准。

## 理由

- 产品逻辑集中在 `src/`，GUI 与 probe 复用同一个 core 库。
- 界面集中在 `qml/Main.qml`，通过 QML 契约测试验证其与控制器的接口。
- 确定性的仿真数据源使流发现与 metadata 展示行为能够重复验证。

## 后果

- 测试输入与期望行为由 `tests/fixtures/` 和 `tools/mock/src/lslviewer_mock/scenarios.py` 显式表达。
- 产品行为改动需要维护 C++ 单元测试、offscreen QML 契约测试及相关端到端用例。
- mock 与产品使用的 liblsl 版本应保持 major.minor 一致。
