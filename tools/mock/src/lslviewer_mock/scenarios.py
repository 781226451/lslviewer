"""Mock stream definitions.

A scenario is a fixed set of streams. Everything that ends up in the UI is
derived from these values, so tests can assert on exact strings; nothing here
may use randomness, timestamps or the hostname.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any

MetadataTree = dict[str, Any]


@dataclass(frozen=True)
class MockStream:
    name: str
    source_id: str
    type: str = "EEG"
    channels: int = 2
    srate: float = 10.0
    metadata: MetadataTree = field(default_factory=dict)


@dataclass(frozen=True)
class Scenario:
    name: str
    streams: tuple[MockStream, ...]
    drop_after: float | None = None


def _simple() -> Scenario:
    return Scenario(
        name="simple",
        streams=(
            MockStream(
                name="LSLViewer Mock EEG",
                source_id="lslviewer-mock-simple",
                channels=2,
                srate=10.0,
                metadata={
                    "manufacturer": "LSLViewer mock",
                    "description": "简单的仿真流，用于冒烟测试。",
                    "channels": {
                        "channel": [
                            {"label": "C3", "unit": "microvolts"},
                            {"label": "C4", "unit": "microvolts"},
                        ]
                    },
                    "device": {"serial": "MOCK-001"},
                },
            ),
        ),
    )


def _nested() -> Scenario:
    channels = [
        {"label": label, "unit": "microvolts", "index": index + 1}
        for index, label in enumerate(("Fp1", "Fp2", "C3", "C4", "P3", "P4", "O1", "O2"))
    ]
    return Scenario(
        name="nested",
        streams=(
            MockStream(
                name="LSLViewer Mock Complex EEG",
                source_id="lslviewer-mock-nested",
                channels=8,
                srate=100.0,
                metadata={
                    "schema_version": "1.0",
                    "description": "复杂嵌套 metadata 示例；设备、实验和信号均为模拟数据。",
                    "manufacturer": "LSLViewer mock",
                    "device": {
                        "model": "Simulated EEG-8",
                        "serial_number": "MOCK-NESTED-001",
                        "firmware": {
                            "version": "2.4.1",
                            "build": {"date": "2026-10-23", "revision": "mock"},
                        },
                        "hardware": {
                            "amplifier": {
                                "model": "VirtualAmp",
                                "adc": {
                                    "resolution_bits": 24,
                                    "input_range": {"min": -200, "max": 200, "unit": "microvolts"},
                                },
                            }
                        },
                    },
                    "experiment": {
                        "name": "模拟实验",
                        "steps": {"step": [{"index": 1}, {"index": 2}, {"index": 3}]},
                    },
                    "channels": {"channel": channels},
                },
            ),
        ),
    )


def _empty_desc() -> Scenario:
    return Scenario(
        name="empty-desc",
        streams=(
            MockStream(
                name="LSLViewer Mock Without Metadata",
                source_id="lslviewer-mock-empty",
                channels=1,
                srate=0.0,
            ),
        ),
    )


def _huge() -> Scenario:
    long_text = "仿真说明：" + ("这是用于测试长文本显示的重复内容。" * 60)
    channels = [
        {
            "label": f"Ch{index:03d}",
            "unit": "microvolts",
            "gain": index,
            "impedance": {"value": index * 11, "unit": "kOhm"},
        }
        for index in range(1, 65)
    ]
    return Scenario(
        name="huge",
        streams=(
            MockStream(
                name="LSLViewer Mock Large Metadata",
                source_id="lslviewer-mock-huge",
                channels=64,
                srate=50.0,
                metadata={
                    "description": long_text,
                    "manufacturer": "LSLViewer mock",
                    "channels": {"channel": channels},
                    "notes": {"note": [{"text": long_text} for _ in range(5)]},
                },
            ),
        ),
    )


def _unicode() -> Scenario:
    return Scenario(
        name="unicode",
        streams=(
            MockStream(
                name="LSLViewer 仿真设备 🚀",
                source_id="lslviewer-mock-unicode",
                channels=2,
                srate=5.0,
                metadata={
                    "manufacturer": "示例制造 🚀",
                    "description": "中文字符、emoji 与方向文本：עברית العربية",
                    "channel": [{"label": "电极 01"}, {"label": "电极 02"}],
                },
            ),
        ),
    )


def _many(count: int = 20) -> Scenario:
    types = ("EEG", "EMG", "Markers")
    streams = tuple(
        MockStream(
            name=f"LSLViewer Mock Stream {index:02d}",
            source_id=f"lslviewer-mock-many-{index:02d}",
            type=types[index % len(types)],
            channels=1 + index % 4,
            srate=10.0 + index,
            metadata={"manufacturer": "LSLViewer mock", "index": index},
        )
        for index in range(1, count + 1)
    )
    return Scenario(name="many", streams=streams)


def _lifecycle() -> Scenario:
    streams = (
        MockStream(
            name="LSLViewer Mock Stable",
            source_id="lslviewer-mock-lifecycle-stable",
            metadata={"role": "stays online"},
        ),
        MockStream(
            name="LSLViewer Mock Vanishing A",
            source_id="lslviewer-mock-lifecycle-a",
            metadata={"role": "disappears"},
        ),
        MockStream(
            name="LSLViewer Mock Vanishing B",
            source_id="lslviewer-mock-lifecycle-b",
            type="Markers",
            channels=1,
            srate=0.0,
            metadata={"role": "disappears"},
        ),
    )
    return Scenario(name="lifecycle", streams=streams, drop_after=10.0)


SCENARIOS: dict[str, Scenario] = {
    scenario.name: scenario
    for scenario in (_simple(), _nested(), _empty_desc(), _huge(), _unicode(), _many(), _lifecycle())
}


def get_scenario(name: str) -> Scenario:
    return SCENARIOS[name]
