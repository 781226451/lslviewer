# LSL Viewer 仿真数据源

用于测试 C++ 版 `lslviewer` 的确定性 LSL 发布端。**它只发布，不解析**：任何"XML 应该展开成几行"之类的逻辑都属于产品，出现在这里就等于让测试验证自己。

## 用法

```powershell
cd tools/mock
uv sync --locked          # 首次
uv run lslviewer-mock --list-scenarios
uv run lslviewer-mock --scenario nested --duration 120
```

发布端在 stdout 打印一行就绪事件，便于脚本等待：

```json
{"event": "ready", "scenario": "simple", "streams": [{"name": "LSLViewer Mock EEG", "source_id": "lslviewer-mock-simple"}]}
```

`--duration 0` 表示一直运行；`Ctrl+C` 退出。`lifecycle` 场景默认 10 秒后让除第一条以外的流消失，可用 `--drop-after` 覆盖。

## 场景

| 场景 | 用途 |
|---|---|
| `simple` | 冒烟：一条 EEG 流，两层 metadata |
| `nested` | 深嵌套、重复 `channel` 节点、中英混排 |
| `empty-desc` | 完全没有 metadata，验证"没有 desc"分支 |
| `huge` | 64 通道、长文本、约 30 KB XML，看滚动与性能 |
| `unicode` | 中文、emoji、RTL 文本 |
| `many` | 20 条并发流，验证列表与刷新 |
| `lifecycle` | 中途消失的流，验证"已不可用"处理 |

## 规则

1. **确定性**：name / type / source_id / metadata 全部写死在 `scenarios.py`，不得使用随机数、时间戳或主机名。UID 由 liblsl 生成，测试按 name 匹配。
2. **只发布**：不读 metadata、不判断行数、不模拟状态机。
3. **协议同源**：本包的 `pylsl` 自带一份 `liblsl`，与被测产品（vcpkg `liblsl`）的 major.minor 必须一致，否则测的不是产品的行为。
4. 新增场景 = 在 `scenarios.py` 加一个构造函数并注册进 `SCENARIOS`，不需要改 CLI。
