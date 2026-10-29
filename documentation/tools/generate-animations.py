#!/usr/bin/env python3
"""Generate the animated SVG figures of the theory chapters.

Every figure is written twice to documentation/animations/:

    <name>.svg          SMIL animation, shown on GitHub and on the booklet site
    <name>-frames.svg   the same scene frozen at a few instants, for the PDF booklet
                        (rsvg-convert, which renders the PDF images, ignores SMIL)

Both are generated from one description of the scene, so the print strip can
never drift from the animation. The output is deterministic: commit it.

Usage:
    python documentation/tools/generate-animations.py [--check]

--check regenerates into a temporary directory and compares the result with
the committed files. Standard library only.

Exit codes:
    0  success (or --check found no drift)
    1  --check found a missing, stale or unexpected file
"""

import argparse
import html
import math
import pathlib
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[2]
OUT_DIR = ROOT / "documentation" / "animations"

DURATION = 6.0
SAMPLES = 120
FRAMES_WIDTH = 760
PAD = 20
GAP = 14
TITLE_H = 34
FRAME_LABEL_H = 24

FONT = "DejaVu Sans, Verdana, Arial, sans-serif"
INK = "#1f2937"
MUTED = "#6b7280"
GRID = "#e5e7eb"
CARD = "#ffffff"
EDGE = "#d1d5db"
STEEL = "#e2e8f0"
STEEL_EDGE = "#94a3b8"
PHASE = {"A": "#c2410c", "B": "#15803d", "C": "#1d4ed8"}
FIELD = "#7c3aed"
NORTH = "#b91c1c"
SOUTH = "#64748b"
D_AXIS = "#b91c1c"
Q_AXIS = "#0f766e"
TORQUE = "#0f766e"
ALPHA = "#475569"
BETA = "#a16207"
ARROW_COLORS = (INK, MUTED, FIELD, D_AXIS, Q_AXIS, ALPHA, BETA, *PHASE.values())

SQRT3 = math.sqrt(3.0)
BORE = 0.64


def _num(value):
    text = f"{value:.2f}"
    if "." in text:
        text = text.rstrip("0").rstrip(".")
    return "0" if text in ("-0", "") else text


def _value(value):
    if isinstance(value, str):
        return html.escape(value, quote=True)
    return _num(value)


def _attrs(attributes):
    return "".join(
        f' {name.replace("_", "-")}="{_value(value)}"'
        for name, value in attributes.items()
        if value is not None
    )


def tag(name, body=None, **attributes):
    if body is None:
        return f"<{name}{_attrs(attributes)}/>"
    return f"<{name}{_attrs(attributes)}>{body}</{name}>"


def rich(text, size):
    """Render `i_a` / `ψ_{f}` style subscripts as tspans."""
    shift = size * 0.3
    small = size * 0.72
    segments = []
    plain = ""
    i = 0
    while i < len(text):
        if text[i] == "_" and i + 1 < len(text):
            if text[i + 1] == "{":
                end = text.index("}", i)
                sub = text[i + 2:end]
                i = end + 1
            else:
                sub = text[i + 1]
                i += 2
            segments.append((plain, False))
            segments.append((sub, True))
            plain = ""
        else:
            plain += text[i]
            i += 1
    segments.append((plain, False))

    out = []
    lowered = False
    for content, is_sub in segments:
        if not content:
            continue
        escaped = html.escape(content, quote=False)
        if is_sub:
            out.append(f'<tspan dy="{_num(shift)}" font-size="{_num(small)}">{escaped}</tspan>')
            lowered = True
        elif lowered:
            out.append(f'<tspan dy="{_num(-shift)}">{escaped}</tspan>')
            lowered = False
        else:
            out.append(escaped)
    return "".join(out)


def text(x, y, content, size=12, fill=INK, anchor="middle", weight=None, **attributes):
    return tag(
        "text", rich(content, size), x=x, y=y, font_size=size, fill=fill,
        text_anchor=anchor, font_weight=weight, **attributes,
    )


def arrow_ref(color):
    return f"url(#ah-{color[1:]})"


def pol(cx, cy, r, degrees):
    a = math.radians(degrees)
    return cx + r * math.cos(a), cy - r * math.sin(a)


def _sample_times(discrete):
    count = SAMPLES if discrete else SAMPLES + 1
    return [k / SAMPLES for k in range(count)]


class Scene:
    """One rendering of a figure: animated (t is None) or frozen at instant t in [0, 1)."""

    def __init__(self, t=None, text_scale=1.0):
        self.t = t
        self.text_scale = text_scale

    @property
    def animated(self):
        return self.t is None

    def font(self, size):
        return size * self.text_scale

    def el(self, name, attributes=None, dyn=None, rotate=None, discrete=False, body=""):
        attributes = dict(attributes or {})
        dyn = dyn or {}
        now = 0.0 if self.animated else self.t
        for attribute, fn in dyn.items():
            attributes[attribute] = fn(now)
        if rotate is not None:
            angle, cx, cy = rotate
            attributes["transform"] = f"rotate({_num(-angle(now))} {_num(cx)} {_num(cy)})"

        children = ""
        if self.animated:
            times = _sample_times(discrete)
            timing = (f'dur="{_num(DURATION)}s" repeatCount="indefinite" '
                      f'calcMode="{"discrete" if discrete else "linear"}"')
            for attribute, fn in dyn.items():
                values = ";".join(_value(fn(t)) for t in times)
                children += f'<animate attributeName="{attribute}" {timing} values="{values}"/>'
            if rotate is not None:
                angle, cx, cy = rotate
                values = ";".join(f"{_num(-angle(t))} {_num(cx)} {_num(cy)}" for t in times)
                children += (f'<animateTransform attributeName="transform" type="rotate" '
                             f'{timing} values="{values}"/>')
        return f"<{name}{_attrs(attributes)}>{body}{children}</{name}>"


def vector(scene, cx, cy, length, angle, color, width, discrete=False, halo=False):
    def x2(t):
        return cx + length(t) * math.cos(math.radians(angle(t)))

    def y2(t):
        return cy - length(t) * math.sin(math.radians(angle(t)))

    parts = []
    if halo:
        parts.append(scene.el(
            "line", {"x1": cx, "y1": cy, "stroke": CARD, "stroke-width": width + 4,
                     "stroke-linecap": "round", "opacity": 0.85},
            dyn={"x2": x2, "y2": y2}, discrete=discrete,
        ))
    parts.append(scene.el(
        "line", {"x1": cx, "y1": cy, "stroke": color, "stroke-width": width,
                 "stroke-linecap": "round", "marker-end": arrow_ref(color)},
        dyn={"x2": x2, "y2": y2}, discrete=discrete,
    ))
    return "".join(parts)


def static_vector(x1, y1, x2, y2, color, width, halo=False):
    parts = []
    if halo:
        parts.append(tag("line", x1=x1, y1=y1, x2=x2, y2=y2, stroke=CARD,
                         stroke_width=width + 4, stroke_linecap="round", opacity=0.85))
    parts.append(tag("line", x1=x1, y1=y1, x2=x2, y2=y2, stroke=color, stroke_width=width,
                     stroke_linecap="round", marker_end=arrow_ref(color)))
    return "".join(parts)


def moving_label(scene, cx, cy, r, angle, content, size, fill):
    """A label that travels on a circle but stays upright."""
    return scene.el(
        "text", {"font-size": size, "fill": fill, "text-anchor": "middle", "font-weight": "bold"},
        dyn={"x": lambda t: pol(cx, cy, r, angle(t))[0],
             "y": lambda t: pol(cx, cy, r, angle(t))[1] + size * 0.36},
        body=rich(content, size),
    )


def stator(scene, cx, cy, r, phases="ABC"):
    """Stator iron with one coil per phase: magnetic axis at 0°, 120°, 240°.

    A coil whose axis points along φ has its go conductor (current out of the
    page, dot) at φ + 90° and its return conductor (into the page, cross) at
    φ − 90°; by the right-hand rule its field inside the bore points along φ.
    """
    bore = r * BORE
    slot = (r + bore) / 2
    parts = [
        tag("circle", cx=cx, cy=cy, r=r, fill=STEEL, stroke=STEEL_EDGE, stroke_width=1.5),
        tag("circle", cx=cx, cy=cy, r=bore, fill=CARD, stroke=STEEL_EDGE, stroke_width=1.5),
    ]
    for name in phases:
        axis = 120 * "ABC".index(name)
        color = PHASE[name]
        ax, ay = pol(cx, cy, r + 8, axis)
        parts.append(tag("line", x1=cx, y1=cy, x2=ax, y2=ay, stroke=color, stroke_width=1.2,
                         stroke_dasharray="3 4", opacity=0.7))
        lx, ly = pol(cx, cy, r + 19, axis)
        size = scene.font(13)
        parts.append(text(lx, ly + size * 0.36, name, size=size, fill=color, weight="bold"))
        gx, gy = pol(cx, cy, slot, axis + 90)
        parts.append(tag("circle", cx=gx, cy=gy, r=7, fill=CARD, stroke=color, stroke_width=2))
        parts.append(tag("circle", cx=gx, cy=gy, r=2.4, fill=color))
        rx, ry = pol(cx, cy, slot, axis - 90)
        parts.append(tag("circle", cx=rx, cy=ry, r=7, fill=CARD, stroke=color, stroke_width=2))
        d = 3.6
        parts.append(tag("path", d=f"M{_num(rx - d)},{_num(ry - d)} L{_num(rx + d)},{_num(ry + d)} "
                                   f"M{_num(rx - d)},{_num(ry + d)} L{_num(rx + d)},{_num(ry - d)}",
                         stroke=color, stroke_width=2))
    return "".join(parts)


def rotor_body(scene, cx, cy, r):
    size = scene.font(12)
    return "".join([
        tag("path", d=f"M{_num(cx)},{_num(cy - r)} A{_num(r)},{_num(r)} 0 0 1 {_num(cx)},{_num(cy + r)} Z",
            fill=NORTH),
        tag("path", d=f"M{_num(cx)},{_num(cy + r)} A{_num(r)},{_num(r)} 0 0 1 {_num(cx)},{_num(cy - r)} Z",
            fill=SOUTH),
        text(cx + r * 0.52, cy + size * 0.36, "N", size=size, fill=CARD, weight="bold"),
        text(cx - r * 0.52, cy + size * 0.36, "S", size=size, fill=CARD, weight="bold"),
        tag("circle", cx=cx, cy=cy, r=3.5, fill=CARD, stroke=INK, stroke_width=1),
    ])


def rotor(scene, cx, cy, r, angle, opacity=None):
    return scene.el("g", {"opacity": opacity}, rotate=(angle, cx, cy),
                    body=rotor_body(scene, cx, cy, r))


def gauge(scene, cx, y, value, half=80, label="torque"):
    size = scene.font(11)
    return "".join([
        tag("line", x1=cx - half, y1=y, x2=cx + half, y2=y, stroke=GRID, stroke_width=10),
        tag("line", x1=cx, y1=y - 8, x2=cx, y2=y + 8, stroke=MUTED, stroke_width=1),
        scene.el("line", {"x1": cx, "y1": y, "y2": y, "stroke": TORQUE, "stroke-width": 10},
                 dyn={"x2": lambda t: cx + half * value(t)}),
        text(cx - half - 4, y + size * 0.36, "−", size=size, fill=MUTED, anchor="end"),
        text(cx + half + 4, y + size * 0.36, "+", size=size, fill=MUTED, anchor="start"),
        text(cx, y + 10 + size, label, size=size, fill=MUTED),
    ])


def legend(x, y, items, size=11, vertical=False):
    parts = []
    cx, cy = x, y
    for label, color, style in items:
        if style == "dash":
            parts.append(tag("line", x1=cx, y1=cy, x2=cx + 20, y2=cy, stroke=color,
                             stroke_width=2.2, stroke_dasharray="5 3"))
        elif style == "arrow":
            parts.append(tag("line", x1=cx, y1=cy, x2=cx + 18, y2=cy, stroke=color, stroke_width=3,
                             marker_end=arrow_ref(color)))
        elif style == "dot":
            parts.append(tag("circle", cx=cx + 10, cy=cy, r=3.5, fill=color))
        elif style == "fill":
            parts.append(tag("rect", x=cx, y=cy - 6, width=20, height=12, fill=color,
                             fill_opacity=0.25, stroke=color, stroke_width=1))
        else:
            parts.append(tag("line", x1=cx, y1=cy, x2=cx + 20, y2=cy, stroke=color, stroke_width=2.4))
        parts.append(text(cx + 27, cy + size * 0.36, label, size=size, anchor="start"))
        plain = label.replace("_{", "").replace("_", "").replace("}", "")
        if vertical:
            cy += size + 8
        else:
            cx += 27 + len(plain) * size * 0.62 + 16
    return "".join(parts)


DEGREE_TICKS = [(0.0, "0°"), (0.25, "90°"), (0.5, "180°"), (0.75, "270°"), (1.0, "360°")]


class Plot:
    """A rectangular plot whose x axis spans one cycle, u in [0, 1]."""

    def __init__(self, x, y, w, h, y_range, y_ticks, x_label, y_label, x_ticks=DEGREE_TICKS):
        self.x, self.y, self.w, self.h = x, y, w, h
        self.y_range = y_range
        self.y_ticks = y_ticks
        self.x_ticks = x_ticks
        self.x_label = x_label
        self.y_label = y_label

    def px(self, u):
        return self.x + u * self.w

    def py(self, v):
        lo, hi = self.y_range
        return self.y + (hi - v) / (hi - lo) * self.h

    def axes(self):
        parts = []
        for value in self.y_ticks:
            y = self.py(value)
            parts.append(tag("line", x1=self.x, y1=y, x2=self.x + self.w, y2=y,
                             stroke=MUTED if value == 0 else GRID, stroke_width=1))
            parts.append(text(self.x - 6, y + 3.5, f"{value:g}".replace("-", "−"),
                              size=10, fill=MUTED, anchor="end"))
        for u, label in self.x_ticks:
            x = self.px(u)
            parts.append(tag("line", x1=x, y1=self.y, x2=x, y2=self.y + self.h, stroke=GRID,
                             stroke_width=1))
            parts.append(text(x, self.y + self.h + 14, label, size=10, fill=MUTED))
        parts.append(tag("rect", x=self.x, y=self.y, width=self.w, height=self.h, fill="none",
                         stroke=EDGE, stroke_width=1))
        if self.x_label:
            parts.append(text(self.x + self.w / 2, self.y + self.h + 30, self.x_label, size=11,
                              fill=MUTED))
        if self.y_label:
            mx, my = self.x - 34, self.y + self.h / 2
            parts.append(text(mx, my, self.y_label, size=11, fill=MUTED,
                              transform=f"rotate(-90 {_num(mx)} {_num(my)})"))
        return "".join(parts)

    def curve(self, fn, color, width=2.2, dash=None, steps=240):
        points = " ".join(f"{_num(self.px(k / steps))},{_num(self.py(fn(k / steps)))}"
                          for k in range(steps + 1))
        return tag("polyline", points=points, fill="none", stroke=color, stroke_width=width,
                   stroke_dasharray=dash, stroke_linejoin="round")

    def tracker(self, scene, curves, marks=None):
        """A moving cursor and dots when animated; numbered markers in a frame strip."""
        parts = []
        if marks is None:
            parts.append(scene.el(
                "line", {"y1": self.y, "y2": self.y + self.h, "stroke": INK, "stroke-width": 1,
                         "stroke-dasharray": "3 3"},
                dyn={"x1": self.px, "x2": self.px},
            ))
            for fn, color in curves:
                parts.append(scene.el(
                    "circle", {"r": 4.5, "fill": color, "stroke": CARD, "stroke-width": 1.5},
                    dyn={"cx": self.px, "cy": lambda t, f=fn: self.py(f(t))},
                ))
            return "".join(parts)
        previous = None
        level = 0
        for number, t in marks:
            x = self.px(t)
            level = level + 1 if previous is not None and x - previous < 20 else 0
            previous = x
            parts.append(tag("line", x1=x, y1=self.y, x2=x, y2=self.y + self.h, stroke=INK,
                             stroke_width=1, stroke_dasharray="3 3"))
            for fn, color in curves:
                parts.append(tag("circle", cx=x, cy=self.py(fn(t)), r=4, fill=color, stroke=CARD,
                                 stroke_width=1.5))
            parts.append(badge(x, self.y + 11 + 20 * level, number))
        return "".join(parts)


def badge(x, y, number, size=11):
    return (tag("circle", cx=x, cy=y, r=size * 0.8, fill=INK)
            + text(x, y + size * 0.36, str(number), size=size, fill=CARD, weight="bold"))


def defs():
    markers = "".join(
        f'<marker id="ah-{color[1:]}" viewBox="0 0 10 10" refX="7" refY="5" markerWidth="3.4" '
        f'markerHeight="3.4" markerUnits="strokeWidth" orient="auto">'
        f'<path d="M0,0 L10,5 L0,10 z" fill="{color}"/></marker>'
        for color in sorted(set(ARROW_COLORS))
    )
    return f"<defs>{markers}</defs>"


class Figure:
    name = ""
    title = ""
    diagram_size = (300, 300)
    plot_size = (380, 300)
    frame_diagram_size = None
    frame_times = ()

    def diagram(self, scene):
        raise NotImplementedError

    def frame_diagram(self, scene):
        return self.diagram(scene)

    def frame_label(self, t):
        return f"θ_e = {round(360 * t)}°"

    def plot(self, scene, marks):
        raise NotImplementedError


def _document(width, height, title, body):
    return (
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{_num(width)}" height="{_num(height)}" '
        f'viewBox="0 0 {_num(width)} {_num(height)}" role="img" aria-labelledby="title" '
        f'font-family="{FONT}">'
        f'<title id="title">{html.escape(title, quote=False)}</title>'
        f"{defs()}"
        f'{tag("rect", x=0.5, y=0.5, width=width - 1, height=height - 1, rx=10, fill=CARD, stroke=EDGE)}'
        f'{text(PAD, PAD + 14, title, size=15, anchor="start", weight="bold")}'
        f"{body}</svg>\n"
    )


def render_animation(figure):
    dw, dh = figure.diagram_size
    pw, ph = figure.plot_size
    width = PAD + dw + 20 + pw + PAD
    height = PAD + TITLE_H + max(dh, ph) + PAD
    scene = Scene()
    top = PAD + TITLE_H
    body = (f'<g transform="translate({PAD} {top})">{figure.diagram(scene)}</g>'
            f'<g transform="translate({PAD + dw + 20} {top})">{figure.plot(scene, None)}</g>')
    return _document(width, height, figure.title, body)


def render_frames(figure):
    fdw, fdh = figure.frame_diagram_size or figure.diagram_size
    pw, ph = figure.plot_size
    count = len(figure.frame_times)
    scale = min(1.0, (FRAMES_WIDTH - 2 * PAD - (count - 1) * GAP) / (count * fdw))
    text_scale = min(2.0, max(1.0, 0.9 / scale))
    row_width = count * fdw * scale + (count - 1) * GAP
    left = (FRAMES_WIDTH - row_width) / 2
    top = PAD + TITLE_H

    parts = []
    marks = []
    for index, t in enumerate(figure.frame_times):
        number = index + 1
        marks.append((number, t))
        x = left + index * (fdw * scale + GAP)
        centre = x + fdw * scale / 2
        parts.append(badge(centre - 40, top + 9, number))
        parts.append(text(centre - 26, top + 13, figure.frame_label(t), size=12, anchor="start"))
        scene = Scene(t, text_scale)
        parts.append(f'<g transform="translate({_num(x)} {_num(top + FRAME_LABEL_H)}) '
                     f'scale({_num(scale)})">{figure.frame_diagram(scene)}</g>')

    plot_top = top + FRAME_LABEL_H + fdh * scale + 12
    plot_left = (FRAMES_WIDTH - pw) / 2
    parts.append(f'<g transform="translate({_num(plot_left)} {_num(plot_top)})">'
                 f"{figure.plot(Scene(0.0), marks)}</g>")
    height = plot_top + ph + PAD
    return _document(FRAMES_WIDTH, height, figure.title, "".join(parts))


class RotatingField(Figure):
    name = "rotating-field"
    title = "Three fixed coils, one rotating field"
    frame_times = (0.0, 1 / 6, 1 / 3, 1 / 2)
    unit = 44

    def diagram(self, scene):
        cx = cy = 150
        parts = [stator(scene, cx, cy, 120)]
        parts.append(tag("circle", cx=cx, cy=cy, r=1.5 * self.unit, fill="none", stroke=FIELD,
                         stroke_width=1.2, stroke_dasharray="4 4", opacity=0.6))
        for index, name in enumerate("ABC"):
            axis = 120 * index
            parts.append(vector(
                scene, cx, cy,
                lambda t, a=axis: self.unit * math.cos(math.radians(360 * t - a)),
                lambda t, a=axis: a,
                PHASE[name], 3,
            ))
        parts.append(vector(scene, cx, cy, lambda t: 1.5 * self.unit, lambda t: 360 * t, FIELD, 5,
                            halo=True))
        parts.append(tag("circle", cx=cx, cy=cy, r=3.5, fill=INK))
        return "".join(parts)

    def plot(self, scene, marks):
        p = Plot(46, 10, 316, 200, (-1.25, 1.25), (-1, -0.5, 0, 0.5, 1),
                 "electrical angle θ_e", "phase current (pu)")
        curves = [(lambda u, a=120 * k: math.cos(math.radians(360 * u - a)), PHASE[n])
                  for k, n in enumerate("ABC")]
        return "".join([
            p.axes(),
            *(p.curve(fn, color) for fn, color in curves),
            p.tracker(scene, curves, marks),
            legend(46, 268, [("i_a", PHASE["A"], "line"), ("i_b", PHASE["B"], "line"),
                             ("i_c", PHASE["C"], "line"), ("field (sum)", FIELD, "arrow")]),
        ])


class TorqueAngle(Figure):
    name = "rotor-torque-angle"
    title = "Torque pulls the rotor towards the stator field"
    diagram_size = (500, 300)
    plot_size = (300, 300)
    frame_diagram_size = (250, 300)
    frame_times = (0.0, 1 / 8, 1 / 4, 1 / 2)

    def frame_label(self, t):
        return f"δ = {round(360 * t)}°"

    def held(self, scene, ox):
        cx, cy, r = ox + 125, 150, 100
        size = scene.font(12)
        return "".join([
            text(cx, 14, "rotor turned by hand", size=size, fill=MUTED),
            stator(scene, cx, cy, r, phases=""),
            rotor(scene, cx, cy, 54, lambda t: -360 * t),
            static_vector(cx, cy, cx + 0.95 * r, cy, FIELD, 4, halo=True),
            gauge(scene, cx, 268, lambda t: math.sin(math.radians(360 * t))),
        ])

    def oriented(self, scene, ox):
        cx, cy, r = ox + 125, 150, 100
        size = scene.font(12)
        ax, ay = pol(cx, cy, 30, 90)
        turning = "".join([
            rotor_body(scene, cx, cy, 54),
            static_vector(cx, cy, cx, cy - 0.95 * r, FIELD, 4, halo=True),
            tag("path", d=f"M{_num(cx + 30)},{_num(cy)} A30,30 0 0 0 {_num(ax)},{_num(ay)}",
                fill="none", stroke=CARD, stroke_width=2.5),
        ])
        return "".join([
            text(cx, 14, "field-oriented: field 90° ahead", size=size, fill=MUTED),
            stator(scene, cx, cy, r, phases=""),
            scene.el("g", rotate=(lambda t: 360 * t, cx, cy), body=turning),
            moving_label(scene, cx, cy, 40, lambda t: 360 * t + 45, "90°", scene.font(11), CARD),
            gauge(scene, cx, 268, lambda t: 1.0),
        ])

    def diagram(self, scene):
        return self.held(scene, 0) + self.oriented(scene, 250)

    def frame_diagram(self, scene):
        return self.held(scene, 0)

    def plot(self, scene, marks):
        p = Plot(46, 10, 236, 200, (-1.15, 1.15), (-1, 0, 1), "torque angle δ", "torque / maximum")
        torque = (lambda u: math.sin(math.radians(360 * u)), TORQUE)
        notes = [
            text(p.px(0.25), p.py(0.6), "maximum", size=9.5, fill=MUTED),
            text(p.px(0.0) + 6, p.py(-0.3), "aligned: stable", size=9.5, fill=MUTED, anchor="start"),
            text(p.px(0.5) + 6, p.py(0.3), "opposed: unstable", size=9.5, fill=MUTED, anchor="start"),
        ]
        return "".join([
            p.axes(),
            p.curve(*torque),
            *notes,
            p.tracker(scene, [torque], marks),
            legend(46, 268, [("τ ∝ sin δ", TORQUE, "line")]),
        ])


class BackEmf(Figure):
    name = "back-emf"
    title = "A spinning magnet induces a voltage in the coil"
    frame_times = (0.0, 1 / 8, 1 / 4, 1 / 2)

    def diagram(self, scene):
        cx = cy = 150
        return "".join([
            stator(scene, cx, cy, 120, phases="A"),
            rotor(scene, cx, cy, 58, lambda t: 360 * t),
            vector(scene, cx, cy, lambda t: 72 * math.cos(math.radians(360 * t)), lambda t: 0,
                   FIELD, 4, halo=True),
        ])

    def plot(self, scene, marks):
        p = Plot(46, 10, 316, 200, (-1.25, 1.25), (-1, -0.5, 0, 0.5, 1),
                 "rotor angle θ_e", "normalised")
        flux = (lambda u: math.cos(math.radians(360 * u)), FIELD)
        emf = (lambda u: -math.sin(math.radians(360 * u)), PHASE["A"])
        return "".join([
            p.axes(),
            p.curve(flux[0], FIELD, dash="6 4"),
            p.curve(*emf),
            p.tracker(scene, [flux, emf], marks),
            legend(46, 262, [("flux through coil A, ψ_a / ψ_f", FIELD, "dash"),
                             ("back-EMF, e_a / (ω_e ψ_f)", PHASE["A"], "line")], vertical=True),
        ])


def _six_step_field(t):
    angle = 360 * t
    return 60 * math.floor(angle / 60 + 1e-9) + 120


class SixStep(Figure):
    name = "six-step-vs-sinusoidal"
    title = "Six-step versus sinusoidal commutation"
    diagram_size = (500, 300)
    plot_size = (300, 300)
    frame_diagram_size = (250, 300)
    frame_times = (0.0, 30 / 360, 59 / 360, 60 / 360)

    def panel(self, scene, ox, label, field, discrete):
        cx, cy, r = ox + 125, 150, 100
        return "".join([
            text(cx, 14, label, size=scene.font(12), fill=MUTED),
            stator(scene, cx, cy, r, phases=""),
            rotor(scene, cx, cy, 54, lambda t: 360 * t),
            vector(scene, cx, cy, lambda t: 0.95 * r, field, FIELD, 4, discrete=discrete, halo=True),
            gauge(scene, cx, 268, lambda t: math.sin(math.radians(field(t) - 360 * t))),
        ])

    def diagram(self, scene):
        return (self.panel(scene, 0, "six-step: 60° jumps", _six_step_field, True)
                + self.panel(scene, 250, "sinusoidal: always 90°", lambda t: 360 * t + 90, False))

    def frame_diagram(self, scene):
        return self.panel(scene, 0, "six-step: 60° jumps", _six_step_field, True)

    def plot(self, scene, marks):
        p = Plot(46, 10, 236, 200, (0.6, 1.05), (0.6, 0.7, 0.8, 0.9, 1),
                 "electrical angle θ_e", "torque / maximum")
        six = (lambda u: math.sin(math.radians(_six_step_field(u) - 360 * u)), PHASE["A"])
        smooth = (lambda u: 1.0, TORQUE)
        return "".join([
            p.axes(),
            p.curve(*smooth),
            p.curve(*six),
            text(p.px(0.5), p.py(0.8), "ripple 1 − sin 60° ≈ 13 %", size=10, fill=PHASE["A"]),
            p.tracker(scene, [six, smooth], marks),
            legend(46, 262, [("six-step", PHASE["A"], "line"),
                             ("sinusoidal (FOC)", TORQUE, "line")], vertical=True),
        ])


class ParkTransform(Figure):
    name = "park-transform"
    title = "One current, seen from the stator and from the rotor"
    plot_size = (380, 320)
    frame_times = (0.0, 1 / 8, 1 / 4, 3 / 8)
    i_d = 0.5
    i_q = math.sqrt(3) / 2
    scale = 100

    @property
    def gamma(self):
        return math.degrees(math.atan2(self.i_q, self.i_d))

    def diagram(self, scene):
        cx = cy = 150
        s = self.scale
        size = scene.font(13)
        tx, ty = pol(cx, cy, s, self.gamma)
        turning = "".join([
            tag("g", rotor_body(scene, cx, cy, 34), opacity=0.25),
            static_vector(cx - 125, cy, cx + 125, cy, D_AXIS, 1.6),
            static_vector(cx, cy + 125, cx, cy - 125, Q_AXIS, 1.6),
            tag("line", x1=tx, y1=ty, x2=tx, y2=cy, stroke=D_AXIS, stroke_width=1.2,
                stroke_dasharray="3 3"),
            tag("line", x1=tx, y1=ty, x2=cx, y2=ty, stroke=Q_AXIS, stroke_width=1.2,
                stroke_dasharray="3 3"),
            tag("circle", cx=tx, cy=cy, r=4, fill=D_AXIS),
            tag("circle", cx=cx, cy=ty, r=4, fill=Q_AXIS),
            static_vector(cx, cy, tx, ty, FIELD, 4),
        ])

        def tip_x(t):
            return pol(cx, cy, s, 360 * t + self.gamma)[0]

        def tip_y(t):
            return pol(cx, cy, s, 360 * t + self.gamma)[1]

        dashed = {"stroke": MUTED, "stroke-width": 1, "stroke-dasharray": "2 3"}
        return "".join([
            static_vector(cx - 140, cy, cx + 140, cy, ALPHA, 1.2),
            text(cx + 140, cy + 16, "α", size=size, fill=ALPHA, weight="bold"),
            static_vector(cx, cy + 140, cx, cy - 140, BETA, 1.2),
            text(cx + 12, cy - 134, "β", size=size, fill=BETA, weight="bold"),
            scene.el("g", rotate=(lambda t: 360 * t, cx, cy), body=turning),
            moving_label(scene, cx, cy, 138, lambda t: 360 * t, "d", size, D_AXIS),
            moving_label(scene, cx, cy, 138, lambda t: 360 * t + 90, "q", size, Q_AXIS),
            scene.el("line", dict(dashed, y2=cy), dyn={"x1": tip_x, "y1": tip_y, "x2": tip_x}),
            scene.el("line", dict(dashed, x2=cx), dyn={"x1": tip_x, "y1": tip_y, "y2": tip_y}),
            scene.el("circle", {"r": 4, "cy": cy, "fill": ALPHA}, dyn={"cx": tip_x}),
            scene.el("circle", {"r": 4, "cx": cx, "fill": BETA}, dyn={"cy": tip_y}),
        ])

    def plot(self, scene, marks):
        top = Plot(46, 10, 316, 100, (-1.2, 1.2), (-1, 0, 1), None, "stator α, β")
        bottom = Plot(46, 150, 316, 100, (-1.2, 1.2), (-1, 0, 1), "electrical angle θ_e", "rotor d, q")
        g = self.gamma
        alpha = (lambda u: math.cos(math.radians(360 * u + g)), ALPHA)
        beta = (lambda u: math.sin(math.radians(360 * u + g)), BETA)
        d = (lambda u: self.i_d, D_AXIS)
        q = (lambda u: self.i_q, Q_AXIS)
        return "".join([
            top.axes(), top.curve(*alpha), top.curve(*beta),
            top.tracker(scene, [alpha, beta], marks),
            bottom.axes(), bottom.curve(*d), bottom.curve(*q),
            bottom.tracker(scene, [d, q], marks),
            legend(46, 300, [("i_α", ALPHA, "line"), ("i_β", BETA, "line"),
                             ("i_d", D_AXIS, "line"), ("i_q", Q_AXIS, "line")]),
        ])


SWITCH_STATES = ("100", "110", "010", "011", "001", "101")


def svm_duties(theta_degrees, modulation):
    """Common-mode-injection SVM of documentation/theory/foc.md, per-unit base Vdc/√3."""
    v_alpha = modulation * math.cos(math.radians(theta_degrees))
    v_beta = modulation * math.sin(math.radians(theta_degrees))
    phases = (v_alpha,
              -v_alpha / 2 + SQRT3 / 2 * v_beta,
              -v_alpha / 2 - SQRT3 / 2 * v_beta)
    common = -(max(phases) + min(phases)) / 2
    return [(v + common) / SQRT3 + 0.5 for v in phases]


class SvmHexagon(Figure):
    name = "svm-hexagon"
    title = "Space-vector modulation: hexagon, sector and duties"
    frame_times = (20 / 360, 80 / 360, 140 / 360, 200 / 360)
    modulation = 0.9
    radius = 112

    def diagram(self, scene):
        cx = cy = 150
        rh = self.radius
        inscribed = rh * math.cos(math.radians(30))
        size = scene.font(11)
        parts = []
        for k in range(6):
            a = pol(cx, cy, rh, 60 * k)
            b = pol(cx, cy, rh, 60 * (k + 1))
            parts.append(scene.el(
                "polygon",
                {"points": f"{_num(cx)},{_num(cy)} {_num(a[0])},{_num(a[1])} {_num(b[0])},{_num(b[1])}",
                 "fill": FIELD},
                dyn={"fill-opacity": lambda t, k=k: 0.2 if math.floor(6 * t + 1e-9) % 6 == k else 0},
                discrete=True,
            ))
        vertices = " ".join(f"{_num(x)},{_num(y)}" for x, y in (pol(cx, cy, rh, 60 * k) for k in range(6)))
        parts.append(tag("polygon", points=vertices, fill="none", stroke=INK, stroke_width=1.5))
        for k in range(6):
            x, y = pol(cx, cy, rh, 60 * k)
            parts.append(tag("line", x1=cx, y1=cy, x2=x, y2=y, stroke=MUTED, stroke_width=0.8,
                             stroke_dasharray="2 3"))
            parts.append(tag("circle", cx=x, cy=y, r=3, fill=INK))
            lx, ly = pol(cx, cy, rh + 20, 60 * k)
            parts.append(text(lx, ly - 1, f"V_{k + 1}", size=size, weight="bold"))
            parts.append(text(lx, ly + size, SWITCH_STATES[k], size=size * 0.85, fill=MUTED))
        parts.append(tag("circle", cx=cx, cy=cy, r=inscribed, fill="none", stroke=MUTED,
                         stroke_width=1.2, stroke_dasharray="5 4"))
        parts.append(vector(scene, cx, cy, lambda t: self.modulation * inscribed, lambda t: 360 * t,
                            FIELD, 4, halo=True))
        parts.append(tag("circle", cx=cx, cy=cy, r=3, fill=INK))
        return "".join(parts)

    def plot(self, scene, marks):
        p = Plot(46, 24, 316, 186, (0, 1), (0, 0.25, 0.5, 0.75, 1), "reference angle θ_e", "duty")
        curves = [(lambda u, k=k: svm_duties(360 * u, self.modulation)[k], PHASE[n])
                  for k, n in enumerate("ABC")]
        sectors = "".join(text(p.px((k + 0.5) / 6), p.y - 6, f"sector {k + 1}", size=9, fill=MUTED)
                          for k in range(6))
        boundaries = "".join(tag("line", x1=p.px(k / 6), y1=p.y, x2=p.px(k / 6), y2=p.y + p.h,
                                 stroke=GRID, stroke_width=1) for k in (1, 3, 5))
        return "".join([
            p.axes(),
            boundaries,
            sectors,
            *(p.curve(fn, color) for fn, color in curves),
            p.tracker(scene, curves, marks),
            legend(46, 262, [("d_A", PHASE["A"], "line"), ("d_B", PHASE["B"], "line"),
                             ("d_C", PHASE["C"], "line"), ("active sector", FIELD, "fill")]),
            legend(46, 282, [("linear limit |v| = V_{dc}/√3", MUTED, "dash")]),
        ])


DEAD_TIME = 0.075
HIGH_ON = 0.25
HIGH_OFF = 0.75


def _high_gate(t):
    return HIGH_ON + DEAD_TIME <= t < HIGH_OFF


def _low_gate(t):
    return t < HIGH_ON or t >= HIGH_OFF + DEAD_TIME


def _half_bridge_state(t):
    if _high_gate(t):
        return "high"
    if _low_gate(t):
        return "low"
    return "dead"


def _mosfet(x, y, label, size):
    """A switch drawn as a box between drain (top) and source (bottom)."""
    return "".join([
        tag("line", x1=x, y1=y - 34, x2=x, y2=y - 18, stroke=INK, stroke_width=2),
        tag("line", x1=x, y1=y + 18, x2=x, y2=y + 34, stroke=INK, stroke_width=2),
        tag("line", x1=x - 46, y1=y, x2=x - 22, y2=y, stroke=MUTED, stroke_width=1.5),
        text(x - 50, y + size * 0.36, label.replace("Q", "G"), size=size * 0.85, fill=MUTED, anchor="end"),
        text(x, y + size * 0.36, label, size=size, weight="bold"),
    ])


def _diode(x, y, label, size):
    """A body diode, cathode up, between the drain and source rails of its switch."""
    return "".join([
        tag("line", x1=x, y1=y - 34, x2=x, y2=y - 9, stroke=INK, stroke_width=1.5),
        tag("line", x1=x, y1=y + 9, x2=x, y2=y + 34, stroke=INK, stroke_width=1.5),
        tag("polygon", points=f"{_num(x - 9)},{_num(y + 9)} {_num(x + 9)},{_num(y + 9)} {_num(x)},{_num(y - 7)}",
            fill=CARD, stroke=INK, stroke_width=1.5),
        tag("line", x1=x - 9, y1=y - 8, x2=x + 9, y2=y - 8, stroke=INK, stroke_width=2),
        text(x + 13, y + size * 0.36, label, size=size * 0.85, fill=MUTED, anchor="start"),
    ])


class HalfBridgeDeadTime(Figure):
    name = "half-bridge-dead-time"
    title = "One half-bridge leg through a PWM period, dead time exaggerated"
    frame_times = (0.125, 0.29, 0.5, 0.79)

    def frame_label(self, t):
        return {"high": "high side on", "low": "low side on", "dead": "dead time"}[_half_bridge_state(t)]

    def diagram(self, scene):
        size = scene.font(12)
        qx, dx, node_y = 120, 178, 150
        high_y, low_y = 92, 208

        def lit(state):
            return lambda t: 1.0 if _half_bridge_state(t) == state else 0.0

        def boxes(x, y, state):
            return scene.el("rect", {"x": x - 22, "y": y - 18, "width": 44, "height": 36, "rx": 5,
                                     "fill": FIELD, "stroke": INK, "stroke-width": 1.5},
                            dyn={"fill-opacity": lambda t: 0.35 if _half_bridge_state(t) == state else 0.0},
                            discrete=True)

        path = {"stroke": FIELD, "stroke-width": 4, "fill": "none", "stroke-linejoin": "round",
                "stroke-dasharray": "7 4", "stroke-linecap": "round"}
        current_paths = [
            ("high", f"M{qx},40 L{qx},{node_y} L262,{node_y}"),
            ("low", f"M{qx},262 L{qx},{node_y} L262,{node_y}"),
            ("dead", f"M{dx},262 L{dx},{node_y} L262,{node_y}"),
        ]
        node_labels = [
            ("high", "v_{sw} = V_{dc}"),
            ("low", "v_{sw} ≈ 0"),
            ("dead", "v_{sw} = −V_f"),
        ]
        coil = " ".join(f"A6,6 0 0 1 {_num(206 + 12 * (k + 1))},{node_y}" for k in range(4))
        paths = [scene.el("path", dict(path, d=d), dyn={"opacity": lit(state)}, discrete=True)
                 for state, d in current_paths]
        parts = [
            tag("line", x1=40, y1=40, x2=dx + 30, y2=40, stroke=INK, stroke_width=2.5),
            text(36, 44, "V_{dc}", size=size, anchor="end", weight="bold"),
            tag("line", x1=40, y1=262, x2=dx + 30, y2=262, stroke=INK, stroke_width=2.5),
            text(36, 266, "0 V", size=size, anchor="end", weight="bold"),
            tag("line", x1=qx, y1=40, x2=qx, y2=high_y - 34, stroke=INK, stroke_width=2),
            tag("line", x1=qx, y1=high_y + 34, x2=qx, y2=low_y - 34, stroke=INK, stroke_width=2),
            tag("line", x1=qx, y1=low_y + 34, x2=qx, y2=262, stroke=INK, stroke_width=2),
            tag("line", x1=dx, y1=40, x2=dx, y2=high_y - 34, stroke=INK, stroke_width=1.5),
            tag("line", x1=dx, y1=high_y + 34, x2=dx, y2=low_y - 34, stroke=INK, stroke_width=1.5),
            tag("line", x1=dx, y1=low_y + 34, x2=dx, y2=262, stroke=INK, stroke_width=1.5),
            tag("line", x1=qx, y1=node_y, x2=206, y2=node_y, stroke=INK, stroke_width=2),
            tag("path", d=f"M206,{node_y} {coil}", fill="none", stroke=INK, stroke_width=2),
            tag("line", x1=254, y1=node_y, x2=272, y2=node_y, stroke=INK, stroke_width=2),
            text(240, node_y + 24, "phase L", size=size * 0.85, fill=MUTED),
            static_vector(232, node_y - 18, 268, node_y - 18, PHASE["A"], 2),
            text(250, node_y - 26, "i > 0", size=size * 0.85, fill=PHASE["A"]),
            *paths,
            tag("circle", cx=qx, cy=node_y, r=4, fill=INK),
            boxes(qx, high_y, "high"),
            boxes(qx, low_y, "low"),
            _mosfet(qx, high_y, "Q_H", size),
            _mosfet(qx, low_y, "Q_L", size),
            _diode(dx, high_y, "D_H", size),
            _diode(dx, low_y, "D_L", size),
        ]
        for state, label in node_labels:
            parts.append(scene.el("text", {"x": 14, "y": node_y + size * 0.36, "font-size": size * 0.9,
                                           "fill": FIELD, "font-weight": "bold"},
                                  dyn={"opacity": lit(state)}, discrete=True, body=rich(label, size * 0.9)))
        return "".join(parts)

    def plot(self, scene, marks):
        ticks = [(0.0, "0"), (0.25, "T/4"), (0.5, "T/2"), (0.75, "3T/4"), (1.0, "T")]
        gate_h = Plot(46, 10, 316, 44, (-0.2, 1.2), (0, 1), None, "G_H", x_ticks=())
        gate_l = Plot(46, 72, 316, 44, (-0.2, 1.2), (0, 1), None, "G_L", x_ticks=())
        node = Plot(46, 134, 316, 92, (-0.2, 1.2), (0, 1), "time in one PWM period", "v_sw / V_dc",
                    x_ticks=ticks)
        high = (lambda u: 1.0 if _high_gate(u) else 0.0, INK)
        low = (lambda u: 1.0 if _low_gate(u) else 0.0, INK)
        ideal = (lambda u: 1.0 if HIGH_ON <= u < HIGH_OFF else 0.0, MUTED)
        actual = (lambda u: 1.0 if _high_gate(u) else 0.0, PHASE["A"])
        lost = tag("rect", x=node.px(HIGH_ON), y=node.py(1.0), width=node.px(HIGH_ON + DEAD_TIME) - node.px(HIGH_ON),
                   height=node.py(0.0) - node.py(1.0), fill=PHASE["A"], fill_opacity=0.18)
        dead = "".join(tag("rect", x=gate_h.px(a), y=gate_h.y, width=gate_h.px(a + DEAD_TIME) - gate_h.px(a),
                           height=node.y + node.h - gate_h.y, fill=FIELD, fill_opacity=0.08)
                       for a in (HIGH_ON, HIGH_OFF))
        return "".join([
            dead,
            gate_h.axes(), gate_h.curve(*high, steps=400), gate_h.tracker(scene, [high], marks),
            gate_l.axes(), gate_l.curve(*low, steps=400), gate_l.tracker(scene, [low], marks),
            node.axes(), lost,
            node.curve(ideal[0], MUTED, dash="5 3", steps=400),
            node.curve(*actual, steps=400),
            node.tracker(scene, [actual], marks),
            legend(46, 278, [("ideal v_{sw}", MUTED, "dash"), ("actual, i > 0", PHASE["A"], "line")]),
            legend(46, 296, [("dead time", FIELD, "fill"), ("lost volt-seconds", PHASE["A"], "fill")]),
        ])


SLOTS = 12
POLES = 8
COGGING_PERIODS = 24


def _annular_sector(cx, cy, r_in, r_out, a0, a1, steps=8):
    outer = [pol(cx, cy, r_out, a0 + (a1 - a0) * k / steps) for k in range(steps + 1)]
    inner = [pol(cx, cy, r_in, a1 - (a1 - a0) * k / steps) for k in range(steps + 1)]
    return " ".join(f"{_num(x)},{_num(y)}" for x, y in outer + inner)


class CoggingTorque(Figure):
    name = "cogging-torque"
    title = "Cogging: magnets pulled towards the stator teeth with no current"
    frame_times = (0.0, 1 / 24, 1 / 12, 1 / 8)
    sweep = 90.0

    def frame_label(self, t):
        return f"θ_m = {self.sweep * t:g}°"

    def diagram(self, scene):
        cx = cy = 150
        size = scene.font(11)
        parts = [tag("circle", cx=cx, cy=cy, r=128, fill=STEEL, stroke=STEEL_EDGE, stroke_width=1.5),
                 tag("circle", cx=cx, cy=cy, r=104, fill=CARD, stroke=STEEL_EDGE, stroke_width=1)]
        pitch = 360 / SLOTS
        for k in range(SLOTS):
            centre = 90 + k * pitch
            parts.append(tag("polygon", points=_annular_sector(cx, cy, 76, 105, centre - pitch * 0.3,
                                                               centre + pitch * 0.3),
                             fill=STEEL, stroke=STEEL_EDGE, stroke_width=1))
        magnets = []
        pole_pitch = 360 / POLES
        for k in range(POLES):
            centre = 90 + k * pole_pitch
            colour = NORTH if k % 2 == 0 else SOUTH
            magnets.append(tag("polygon", points=_annular_sector(cx, cy, 52, 72, centre - pole_pitch * 0.42,
                                                                 centre + pole_pitch * 0.42),
                               fill=colour, stroke=CARD, stroke_width=1))
            lx, ly = pol(cx, cy, 62, centre)
            magnets.append(text(lx, ly + size * 0.36, "N" if k % 2 == 0 else "S", size=size * 0.9, fill=CARD,
                                weight="bold"))
        magnets.append(tag("circle", cx=cx, cy=cy, r=52, fill=STEEL, stroke=STEEL_EDGE, stroke_width=1))
        magnets.append(tag("line", x1=cx, y1=cy, x2=cx, y2=cy - 50, stroke=INK, stroke_width=2))
        magnets.append(tag("circle", cx=cx, cy=cy, r=4, fill=INK))
        parts.append(scene.el("g", rotate=(lambda t: self.sweep * t, cx, cy), body="".join(magnets)))
        parts.append(tag("line", x1=cx, y1=cy - 132, x2=cx, y2=cy - 108, stroke=INK, stroke_width=1.5,
                         stroke_dasharray="3 3"))
        if scene.animated:
            parts.append(text(cx, cy + 148, f"{SLOTS} slots, {POLES} poles: lcm(12, 8) = {COGGING_PERIODS} per turn",
                              size=size, fill=MUTED))
        return "".join(parts)

    def plot(self, scene, marks):
        ticks = [(k / 6, f"{int(self.sweep * k / 6)}°") for k in range(7)]
        p = Plot(46, 10, 316, 200, (-5.5, 5.5), (-4, -2, 0, 2, 4), "rotor angle θ_m (mechanical)",
                 "cogging torque (mN·m)", x_ticks=ticks)
        periods = COGGING_PERIODS * self.sweep / 360
        torque = (lambda u: -4.0 * math.sin(2 * math.pi * periods * u), TORQUE)
        detents = "".join(tag("circle", cx=p.px(k / periods), cy=p.py(0), r=3.5, fill=INK)
                          for k in range(int(periods) + 1))
        return "".join([
            p.axes(),
            p.curve(*torque),
            detents,
            p.tracker(scene, [torque], marks),
            legend(46, 262, [("τ_{cog}, 4 mN·m deep (bench motor)", TORQUE, "line")]),
            legend(46, 282, [("detent: stable rest position", INK, "dot")]),
        ])


FIGURES = (RotatingField(), TorqueAngle(), BackEmf(), SixStep(), ParkTransform(), SvmHexagon(),
           HalfBridgeDeadTime(), CoggingTorque())


def generate(out_dir):
    out_dir.mkdir(parents=True, exist_ok=True)
    written = []
    for figure in FIGURES:
        for suffix, render in (("", render_animation), ("-frames", render_frames)):
            path = out_dir / f"{figure.name}{suffix}.svg"
            path.write_text(render(figure), encoding="utf-8", newline="\n")
            written.append(path.name)
    return written


def check():
    with tempfile.TemporaryDirectory() as tmp:
        fresh_dir = pathlib.Path(tmp)
        expected = set(generate(fresh_dir))
        committed = {p.name for p in OUT_DIR.glob("*.svg")} if OUT_DIR.is_dir() else set()
        problems = []
        for name in sorted(expected - committed):
            problems.append(f"missing: {name}")
        for name in sorted(committed - expected):
            problems.append(f"not produced by this script: {name}")
        for name in sorted(expected & committed):
            if (fresh_dir / name).read_bytes() != (OUT_DIR / name).read_bytes():
                problems.append(f"stale: {name}")
    if problems:
        for problem in problems:
            print(f"  {problem}", file=sys.stderr)
        print("Run: python documentation/tools/generate-animations.py", file=sys.stderr)
        return 1
    print(f"{len(expected)} animation files up to date")
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--check", action="store_true",
                        help="compare with the committed files instead of writing them")
    args = parser.parse_args()
    if args.check:
        return check()
    for name in generate(OUT_DIR):
        print(f"  {name}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
