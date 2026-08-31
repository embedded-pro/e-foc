#!/usr/bin/env python3
"""Pre-render every TikZ figure in documentation/tikz/ to SVG.

Generated SVGs land in documentation/tikz/images/ and are committed to
the repository so that VS Code, GitHub, and the HTML booklet can display
them without a LaTeX toolchain at render time.

Run this script whenever a .tex file in documentation/tikz/ is modified:

    python scripts/render-tikz-svgs.py

Requirements:
    xelatex (or pdflatex / lualatex) — part of any standard TeX Live install
    pdf2svg — apt: pdf2svg / brew: pdf2svg

Exit codes:
    0  all figures rendered successfully
    1  one or more figures failed (details printed to stderr)
    2  required tools not found
"""

import hashlib
import pathlib
import shutil
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
TIKZ_DIR = ROOT / "documentation" / "tikz"
OUT_DIR = TIKZ_DIR / "images"

SKIP = {".gitkeep"}


def _has(tool):
    return shutil.which(tool) is not None


def _extract_tikzpicture(content):
    begin = r"\begin{tikzpicture}"
    end = r"\end{tikzpicture}"
    s = content.find(begin)
    e = content.rfind(end)
    if s == -1 or e == -1:
        return None
    return content[s: e + len(end)]


def render(tex_path, engine, out_dir):
    content = tex_path.read_text(encoding="utf-8")
    tikz = _extract_tikzpicture(content)
    if tikz is None:
        print(f"  SKIP {tex_path.name} — no tikzpicture environment", file=sys.stderr)
        return True  # not a failure

    svg_out = out_dir / tex_path.with_suffix(".svg").name

    # Only re-render when source has changed
    digest = hashlib.sha1(content.encode()).hexdigest()[:12]
    stamp = svg_out.with_suffix(".stamp")
    if svg_out.exists() and stamp.exists() and stamp.read_text() == digest:
        print(f"  up-to-date  {svg_out.name}")
        return True

    wrapper = "\n".join([
        r"\documentclass[border=8pt,tikz]{standalone}",
        r"\usepackage[dvipsnames]{xcolor}",
        r"\usepackage{pgfplots}",
        r"\pgfplotsset{compat=1.18}",
        r"\usetikzlibrary{arrows.meta,calc,positioning}",
        r"\tikzset{arr/.style={-{Stealth[length=5pt]},thick}}",
        r"\begin{document}",
        tikz,
        r"\end{document}",
    ])

    with tempfile.TemporaryDirectory() as tmp:
        tmp = pathlib.Path(tmp)
        src = tmp / "fig.tex"
        src.write_text(wrapper, encoding="utf-8")
        pdf = tmp / "fig.pdf"

        result = subprocess.run(
            [engine, "-interaction=nonstopmode", "-halt-on-error",
             f"-output-directory={tmp}", str(src)],
            capture_output=True, text=True, cwd=tmp,
        )
        if result.returncode != 0 or not pdf.exists():
            errors = [l for l in result.stdout.splitlines() if l.startswith("!")]
            print(
                f"  FAIL {tex_path.name}: {errors[0] if errors else 'compile error'}",
                file=sys.stderr,
            )
            return False

        conv = subprocess.run(
            ["pdf2svg", str(pdf), str(svg_out)],
            capture_output=True, text=True,
        )
        if conv.returncode != 0 or not svg_out.exists():
            print(f"  FAIL {tex_path.name}: pdf2svg error", file=sys.stderr)
            return False

    stamp.write_text(digest)
    print(f"  OK  {svg_out.name}")
    return True


def main():
    engine = next((e for e in ("xelatex", "pdflatex", "lualatex") if _has(e)), None)
    if engine is None:
        print("ERROR: no LaTeX engine found (xelatex / pdflatex / lualatex)", file=sys.stderr)
        return 2
    if not _has("pdf2svg"):
        print("ERROR: pdf2svg not found — install with: apt install pdf2svg / brew install pdf2svg",
              file=sys.stderr)
        return 2

    OUT_DIR.mkdir(parents=True, exist_ok=True)

    failures = []
    for tex in sorted(TIKZ_DIR.glob("*.tex")):
        if tex.name in SKIP:
            continue
        if not render(tex, engine, OUT_DIR):
            failures.append(tex.name)

    if failures:
        print(f"\n{len(failures)} figure(s) failed: {', '.join(failures)}", file=sys.stderr)
        return 1

    print(f"\nDone. SVGs written to {OUT_DIR.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
