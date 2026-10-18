"""Passive AD3 monitor for the e-foc power stage: PWM gate signals on DIO, ADC channels on the scope.

The wiring is configurable (--pwm-pins, --scope, AD3_PWM_PINS, AD3_SCOPE).

Only the logic analyzer and scope are used; no output (wavegen, pattern, supplies, static DIO) is ever driven.
Needs the `ad3-waveforms-bench` package (pinned in infra/hal/ti/validation/host/pyproject.toml) and an
`ad3-bench-server` reachable at AD3_REMOTE (default host.docker.internal:5020).
"""

import argparse
import os
from dataclasses import dataclass

from ad3_waveforms_bench import AnalogDiscovery3, analysis
from ad3_waveforms_bench.instruments.ad3 import LogicCapture

DEFAULT_REMOTE = "host.docker.internal:5020"

DEFAULT_PWM_PINS = "PWM_A=0,PWM_B=1"
DEFAULT_SCOPE_CHANNELS = "ADC_Channel2=1,ADC_Channel3=2"



def parse_mapping(text: str) -> dict[str, int]:
    return {name: int(number) for name, number in (item.split("=") for item in text.split(","))}


@dataclass(frozen=True)
class PwmChannelReport:
    name: str
    dio: int
    idle_level: int | None
    frequency_hz: float
    duty: float
    edges: int


def connect(remote: str | None = None) -> AnalogDiscovery3:
    return AnalogDiscovery3(remote=remote or os.environ.get("AD3_REMOTE", DEFAULT_REMOTE))


def record_pwm(ad3: AnalogDiscovery3, rate: float = 5e6, samples: int = 16384) -> LogicCapture:
    return ad3.logic.record(rate=rate, samples=samples)


def report_pwm(capture: LogicCapture, pins: dict[str, int]) -> list[PwmChannelReport]:
    reports = []
    for name, dio in pins.items():
        bits = capture.channel(dio)
        switching = analysis.edge_count(bits, "both") > 0
        reports.append(
            PwmChannelReport(
                name=name,
                dio=dio,
                idle_level=None if switching else bits[0],
                frequency_hz=capture.frequency(dio) if switching else 0.0,
                duty=capture.duty(dio),
                edges=capture.edge_count(dio, "both"),
            )
        )
    return reports


def scope_stats(ad3: AnalogDiscovery3, channels: dict[str, int], rate: float = 1e5, samples: int = 4000) -> dict[str, analysis.Stats]:
    return {name: ad3.scope.stats(channel, rate=rate, samples=samples) for name, channel in channels.items()}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--remote", help=f"ad3-bench-server host:port (default AD3_REMOTE or {DEFAULT_REMOTE})")
    parser.add_argument("--rate", type=float, default=5e6, help="logic sample rate in Hz")
    parser.add_argument("--samples", type=int, default=16384, help="logic sample count")
    parser.add_argument("--pwm-pins", default=os.environ.get("AD3_PWM_PINS", DEFAULT_PWM_PINS), help="name=DIO list, e.g. PWM1_H=0,PWM1_L=1")
    parser.add_argument("--scope", default=os.environ.get("AD3_SCOPE", DEFAULT_SCOPE_CHANNELS), help="name=scope channel (1|2) list")
    args = parser.parse_args()
    pins = parse_mapping(args.pwm_pins)
    channels = parse_mapping(args.scope)

    with connect(args.remote) as ad3:
        capture = record_pwm(ad3, args.rate, args.samples)
        print(f"logic: {len(capture.samples)} samples @ {capture.rate / 1e6:g} MS/s = {capture.duration * 1e3:.2f} ms")
        for r in report_pwm(capture, pins):
            state = f"idle={r.idle_level}" if r.idle_level is not None else f"f={r.frequency_hz:.1f} Hz duty={r.duty:.3f}"
            print(f"  DIO{r.dio} {r.name:7s} edges={r.edges:4d} {state}")
        for name, s in scope_stats(ad3, channels).items():
            print(f"scope {name} (CH{channels[name]}): mean={s.mean:.4f} V min={s.minimum:.4f} max={s.maximum:.4f} stdev={s.stdev:.4f} n={s.count}")


if __name__ == "__main__":
    main()
