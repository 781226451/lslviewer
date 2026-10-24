"""Build LSL outlets from mock stream definitions and keep them alive."""

from __future__ import annotations

import threading
from typing import Any

from pylsl import StreamInfo, StreamOutlet

from .scenarios import MockStream

# Metadata trees use the same conventions the viewer displays:
#   scalar -> an element with text, dict -> nested element, list -> repeated siblings.
def apply_metadata(parent: Any, fields: dict[str, Any]) -> None:
    for name, value in fields.items():
        items = value if isinstance(value, list) else [value]
        for item in items:
            if isinstance(item, dict):
                apply_metadata(parent.append_child(name), item)
            else:
                parent.append_child_value(name, str(item))


def create_outlet(stream: MockStream) -> StreamOutlet:
    info = StreamInfo(
        name=stream.name,
        type=stream.type,
        channel_count=max(1, stream.channels),
        nominal_srate=float(stream.srate),
        channel_format="float32",
        source_id=stream.source_id,
    )
    if stream.metadata:
        apply_metadata(info.desc(), stream.metadata)
    return StreamOutlet(info)


def push_samples(outlet: StreamOutlet, stream: MockStream, stop: threading.Event) -> None:
    """Push a slow ramp so the stream stays discoverable for the whole run."""
    interval = 1.0 / stream.srate if stream.srate > 0 else 0.1
    value = 0.0
    while not stop.wait(interval):
        try:
            outlet.push_sample([value] * max(1, stream.channels))
        except Exception:  # outlet closed while shutting down
            return
        value = 0.0 if value >= 100.0 else value + 1.0
