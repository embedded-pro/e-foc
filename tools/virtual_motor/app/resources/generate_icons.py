#!/usr/bin/env python3

import math
import pathlib

from PIL import Image, ImageDraw

HERE = pathlib.Path(__file__).resolve().parent
SCALE = 4
SIZE = 256
DASHES = 12

BACKGROUND = "#0F172A"
RING = "#A78BFA"
HUB = "#E2E8F0"
PHASES = ("#F87171", "#FBBF24", "#34D399")


def s(value):
    return value * SCALE


def render():
    image = Image.new("RGBA", (s(SIZE), s(SIZE)), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)

    draw.rounded_rectangle((s(16), s(16), s(240), s(240)), radius=s(48), fill=BACKGROUND)
    box = (s(128 - 84), s(128 - 84), s(128 + 84), s(128 + 84))
    span = 360 / DASHES
    for dash in range(DASHES):
        start = dash * span - 90
        draw.arc(box, start, start + span * 0.6, fill=RING, width=s(18))

    for index, colour in enumerate(PHASES):
        angle = math.radians(-90 + 120 * index)
        end = (s(128 + 60 * math.cos(angle)), s(128 + 60 * math.sin(angle)))
        draw.line((s(128), s(128), *end), fill=colour, width=s(18))
        cap = s(9)
        draw.ellipse((end[0] - cap, end[1] - cap, end[0] + cap, end[1] + cap), fill=colour)

    draw.ellipse((s(128 - 16), s(128 - 16), s(128 + 16), s(128 + 16)), fill=HUB)
    return image.resize((SIZE, SIZE), Image.LANCZOS)


def main():
    icon = render()
    icon.save(HERE / "e-foc-virtual-motor.png")
    icon.save(HERE / "e-foc-virtual-motor.ico", sizes=[(n, n) for n in (16, 24, 32, 48, 64, 128, 256)])


if __name__ == "__main__":
    main()
