"""End-to-end tests: publish a scenario, dump it with the real C++ probe, assert.

These exercise the full chain — pylsl outlet -> liblsl discovery -> C++ parsing
and flattening -> JSON — without any GUI automation. The probe is the product's
own core code, so a regression in discovery, selection or metadata flattening
fails here.
"""

from __future__ import annotations

import json
import os
import subprocess
import sys
from contextlib import contextmanager
from pathlib import Path
from typing import Iterator

import pytest

ROOT = Path(__file__).resolve().parents[3]
DEFAULT_PROBE = ROOT / "build" / "release" / "lslviewer_probe.exe"
PROBE = Path(os.environ.get("LSLVIEWER_PROBE", DEFAULT_PROBE))

pytestmark = pytest.mark.skipif(
    not PROBE.exists(), reason=f"probe not built yet: {PROBE} (build the C++ project first)"
)


@contextmanager
def publishing(scenario: str, duration: float = 30.0) -> Iterator[list[dict[str, str]]]:
    """Start a mock scenario and wait for its ready event, then tear it down."""
    process = subprocess.Popen(
        [sys.executable, "-m", "lslviewer_mock", "--scenario", scenario, "--duration", str(duration)],
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        text=True,
        encoding="utf-8",
    )
    try:
        assert process.stdout is not None
        line = process.stdout.readline()
        ready = json.loads(line)
        assert ready["event"] == "ready", ready
        yield ready["streams"]
    finally:
        process.terminate()
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:  # pragma: no cover - defensive
            process.kill()


def dump(stream_query: str, directory: Path, timeout: float = 15.0) -> dict:
    out = directory / "dump.json"
    result = subprocess.run(
        [str(PROBE), "--stream", stream_query, "--timeout", str(timeout), "--out", str(out)],
        capture_output=True,
        text=True,
        encoding="utf-8",
        timeout=timeout + 20,
    )
    assert out.exists(), f"probe wrote no dump (exit {result.returncode}): {result.stderr}"
    data = json.loads(out.read_text(encoding="utf-8"))
    assert data["ok"], data
    return data


def test_simple_stream_is_discovered_and_flattened(tmp_path: Path) -> None:
    with publishing("simple"):
        data = dump("name:LSLViewer Mock EEG", tmp_path)

    assert data["streamCount"] >= 1
    assert data["details"]["name"] == "LSLViewer Mock EEG"
    assert data["details"]["type"] == "EEG"
    assert data["details"]["channel_count"] == 2
    assert "<desc>" in data["xml"]

    rows = {(row["name"], row["value"], row["depth"]) for row in data["rows"]}
    assert ("manufacturer", "LSLViewer mock", 0) in rows
    assert ("channels", "", 0) in rows
    assert ("channel", "", 1) in rows
    assert ("label", "C3", 2) in rows
    assert ("serial", "MOCK-001", 1) in rows


def test_nested_metadata_keeps_structure_and_order(tmp_path: Path) -> None:
    with publishing("nested"):
        data = dump("name:LSLViewer Mock Complex EEG", tmp_path)

    names = [row["name"] for row in data["rows"]]
    assert names[0] == "schema_version"
    assert names.count("channel") == 8
    assert names[-1] == "index"  # last channel's last field
    depths = {row["name"]: row["depth"] for row in data["rows"] if row["name"] == "channel"}
    assert depths["channel"] == 1


def test_stream_without_metadata_yields_no_rows(tmp_path: Path) -> None:
    with publishing("empty-desc"):
        data = dump("name:LSLViewer Mock Without Metadata", tmp_path)

    assert data["rows"] == []
    assert data["xml"] != ""
    assert data["details"]["name"] == "LSLViewer Mock Without Metadata"


def test_unicode_survives_the_round_trip(tmp_path: Path) -> None:
    with publishing("unicode"):
        data = dump("name:LSLViewer 仿真设备 🚀", tmp_path)

    values = [row["value"] for row in data["rows"]]
    assert "示例制造 🚀" in values
    assert "电极 01" in values
    assert data["details"]["name"] == "LSLViewer 仿真设备 🚀"


def test_many_streams_are_listed(tmp_path: Path) -> None:
    with publishing("many"):
        data = dump("Mock Stream 01", tmp_path)

    assert data["streamCount"] == 20
    names = [stream["name"] for stream in data["streams"]]
    assert "LSLViewer Mock Stream 20" in names
    assert data["details"]["name"] == "LSLViewer Mock Stream 01"


def test_large_metadata_is_flattened_completely(tmp_path: Path) -> None:
    with publishing("huge"):
        data = dump("name:LSLViewer Mock Large Metadata", tmp_path)

    names = [row["name"] for row in data["rows"]]
    assert names.count("channel") == 64
    assert "gain" in names
    assert len(data["xml"]) > 10_000
    assert any(row["name"] == "description" and len(row["value"]) > 1_000 for row in data["rows"])
