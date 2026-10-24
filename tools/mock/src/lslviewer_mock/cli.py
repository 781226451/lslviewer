"""Command line entry point: publish one scenario and keep it online."""

from __future__ import annotations

import argparse
import json
import sys
import threading
import time
from dataclasses import dataclass

from pylsl import StreamOutlet

from .publish import create_outlet, push_samples
from .scenarios import SCENARIOS, MockStream, get_scenario


@dataclass
class Published:
    stream: MockStream
    outlet: StreamOutlet
    stop: threading.Event
    thread: threading.Thread


def _start(streams: tuple[MockStream, ...]) -> list[Published]:
    published: list[Published] = []
    for stream in streams:
        outlet = create_outlet(stream)
        stop = threading.Event()
        thread = threading.Thread(target=push_samples, args=(outlet, stream, stop), daemon=True)
        thread.start()
        published.append(Published(stream=stream, outlet=outlet, stop=stop, thread=thread))
    return published


def _stop(items: list[Published]) -> None:
    for item in items:
        item.stop.set()
    for item in items:
        item.thread.join(timeout=2)


def _emit(event: str, **fields: object) -> None:
    print(json.dumps({"event": event, **fields}, ensure_ascii=False), flush=True)


def _force_utf8_output() -> None:
    """Windows consoles default to a legacy code page; the ready line carries
    stream names that may contain emoji, and an encode error would kill the
    publisher before it announces itself."""
    for stream in (sys.stdout, sys.stderr):
        reconfigure = getattr(stream, "reconfigure", None)
        if reconfigure is None:
            continue
        try:
            reconfigure(encoding="utf-8")
        except (OSError, ValueError):  # pragma: no cover - non-reconfigurable stream
            pass


def main(argv: list[str] | None = None) -> int:
    _force_utf8_output()
    parser = argparse.ArgumentParser(
        prog="lslviewer-mock",
        description="Publish deterministic LSL streams for testing the C++ viewer.",
    )
    parser.add_argument("--scenario", default="simple", help="scenario name (see --list-scenarios)")
    parser.add_argument("--duration", type=float, default=60.0, help="seconds to stay online, 0 = forever")
    parser.add_argument(
        "--drop-after",
        type=float,
        default=None,
        help="seconds after which all but the first stream disappear (lifecycle scenario)",
    )
    parser.add_argument("--list-scenarios", action="store_true", help="print scenario names and exit")
    args = parser.parse_args(argv)

    if args.list_scenarios:
        for name in SCENARIOS:
            print(name)
        return 0

    try:
        scenario = get_scenario(args.scenario)
    except KeyError:
        print(f"unknown scenario: {args.scenario}", file=sys.stderr)
        return 2

    published = _start(scenario.streams)
    _emit(
        "ready",
        scenario=scenario.name,
        streams=[{"name": stream.name, "source_id": stream.source_id} for stream in scenario.streams],
    )

    drop_after = args.drop_after if args.drop_after is not None else scenario.drop_after
    started = time.monotonic()
    deadline = started + args.duration if args.duration > 0 else None
    dropped = False

    try:
        while True:
            now = time.monotonic()
            if deadline is not None and now >= deadline:
                break
            if drop_after is not None and not dropped and now - started >= drop_after:
                dropped = True
                vanished = published[1:]
                _stop(vanished)
                del published[1:]
                _emit("dropped", streams=[item.stream.name for item in vanished])
            time.sleep(0.05)
    except KeyboardInterrupt:
        pass
    finally:
        _stop(published)

    return 0


if __name__ == "__main__":  # pragma: no cover
    raise SystemExit(main())
