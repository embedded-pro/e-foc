#!/usr/bin/env python3
"""Build the e-foc FOC Theory booklet as PDF and/or GitHub Pages HTML.

Chapter ordering is derived from documentation/theory/README.md:
  every [Title](file.md) link in that file's table is included, in order.

Diagram strategy
----------------
Markdown files contain two complementary representations of each diagram:

  <!-- tikz:diagrams/name.tex -->
  ```mermaid ... ```
  <!-- /tikz -->

The build script replaces the full block (tikz comment + mermaid fence +
closing comment) with a compiled SVG image reference:

  1. The referenced .tex file is compiled with pdflatex (standalone class).
  2. The resulting PDF is converted to SVG using pdf2svg.
  3. The SVG is copied to build/booklet/diagrams/ and referenced in the
     assembled Markdown as a standard image.

When pdf2svg or pdflatex is unavailable the Mermaid fallback inside the block
is used instead, ensuring the booklet still builds.

Rendering backend: Pandoc with XeLaTeX (PDF) and standalone HTML + MathJax.

Usage:
    python scripts/build-foc-booklet.py [--format pdf|html|all] [--assemble-only]
                                         [--skip-tikz]

Exit codes:
    0  success
    1  missing source files or Pandoc render failure
    2  required tool (pandoc) not found
"""

import argparse
import hashlib
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile
from datetime import date

ROOT          = pathlib.Path(__file__).resolve().parent.parent
DOC_ROOT      = ROOT / "documentation" / "theory"
INDEX         = DOC_ROOT / "README.md"
BOOKLET_ASSETS = ROOT / "documentation" / "booklet"
BUILD_DIR     = ROOT / "build" / "booklet"
DIAGRAMS_DIR  = BUILD_DIR / "diagrams"

OUTPUT_STEM   = "FocTheory"
MATHJAX_CDN   = "https://cdn.jsdelivr.net/npm/mathjax@3/es5/tex-mml-chtml.js"

LINK_RE      = re.compile(r"\[([^\]]+)\]\(([^)]+\.md)\)")
H1_RE        = re.compile(r"^#\s+(.+?)\s*$", re.MULTILINE)
HEADING_RE   = re.compile(r"^(#{1,6})\s")
IMAGE_RE     = re.compile(r"(!\[[^\]]*\]\()([^)]+)(\))")
REFERENCES_HEADING = "References"

# Matches:  <!-- tikz:path.tex --> ... ``` mermaid ... ``` ... <!-- /tikz -->
TIKZ_BLOCK_RE = re.compile(
    r"<!--\s*tikz:([^\s>]+)\s*-->\s*```mermaid.*?```\s*<!--\s*/tikz\s*-->",
    re.DOTALL,
)
# Fallback: bare Mermaid block (no tikz marker)
MERMAID_RE = re.compile(r"```mermaid\n(.*?)```", re.DOTALL)


# ── Chapter ordering ──────────────────────────────────────────────────────────

def chapter_files() -> list[pathlib.Path]:
    """Return theory doc paths in the order README.md lists them."""
    if not INDEX.is_file():
        print(f"ERROR: index not found: {INDEX}", file=sys.stderr)
        return []
    text = INDEX.read_text(encoding="utf-8")
    seen: set[pathlib.Path] = set()
    docs: list[pathlib.Path] = []
    for _, target in LINK_RE.findall(text):
        target = target.split("#", 1)[0].strip()
        if target.startswith(("http://", "https://")):
            continue
        path = (DOC_ROOT / target).resolve()
        if path in seen or not path.is_file():
            continue
        seen.add(path)
        docs.append(path)
    return docs


# ── TikZ compilation ──────────────────────────────────────────────────────────

def _has_tool(name: str) -> bool:
    return shutil.which(name) is not None


def compile_tikz(tex_rel: str) -> str | None:
    """Compile a TikZ standalone .tex file to SVG; return the SVG path string.

    tex_rel is the path relative to DOC_ROOT (e.g. 'diagrams/foc-full-loop.tex').
    Returns the absolute path to the compiled SVG, or None on failure.
    """
    tex_path = (DOC_ROOT / tex_rel).resolve()
    if not tex_path.is_file():
        print(f"WARNING: TikZ source not found: {tex_path}", file=sys.stderr)
        return None
    if not _has_tool("pdflatex"):
        print("WARNING: pdflatex not found — falling back to Mermaid diagram.", file=sys.stderr)
        return None
    if not _has_tool("pdf2svg"):
        print("WARNING: pdf2svg not found — falling back to Mermaid diagram.", file=sys.stderr)
        return None

    DIAGRAMS_DIR.mkdir(parents=True, exist_ok=True)
    stem = tex_path.stem
    digest = hashlib.sha1(tex_path.read_bytes()).hexdigest()[:8]
    svg_out = DIAGRAMS_DIR / f"{stem}-{digest}.svg"

    if svg_out.exists():
        return str(svg_out)

    with tempfile.TemporaryDirectory() as tmpdir:
        tmp = pathlib.Path(tmpdir)
        result = subprocess.run(
            ["pdflatex", "-interaction=nonstopmode", "-output-directory", str(tmp),
             str(tex_path)],
            capture_output=True, text=True,
        )
        if result.returncode != 0:
            print(f"WARNING: pdflatex failed for {tex_path.name}:\n"
                  f"  {result.stdout[-400:]}", file=sys.stderr)
            return None

        pdf_file = tmp / (tex_path.stem + ".pdf")
        if not pdf_file.exists():
            print(f"WARNING: pdflatex produced no PDF for {tex_path.name}", file=sys.stderr)
            return None

        result2 = subprocess.run(
            ["pdf2svg", str(pdf_file), str(svg_out)],
            capture_output=True, text=True,
        )
        if result2.returncode != 0:
            print(f"WARNING: pdf2svg failed for {tex_path.name}", file=sys.stderr)
            return None

    print(f"  Compiled {tex_path.name} → {svg_out.name}")
    return str(svg_out)


def _replace_tikz_block(m: re.Match, skip_tikz: bool) -> str:
    """Replace a <!-- tikz:... --> + mermaid + <!-- /tikz --> block."""
    tex_rel = m.group(1).strip()

    if not skip_tikz:
        svg_path = compile_tikz(tex_rel)
        if svg_path:
            caption = tex_rel.split("/")[-1].replace(".tex", "").replace("-", " ").title()
            return f"![{caption}]({pathlib.Path(svg_path)})"

    # Fallback: extract and keep the Mermaid block
    mermaid_match = re.search(r"```mermaid\n(.*?)```", m.group(0), re.DOTALL)
    if mermaid_match:
        return f"```mermaid\n{mermaid_match.group(1)}```"
    return ""


# ── Text processing ───────────────────────────────────────────────────────────

def _accumulate_ref_line(line: str, current: list[str], refs: list[str]) -> None:
    stripped = line.strip()
    if stripped.startswith(("-", "*")):
        if current:
            refs.append(" ".join(current))
            current.clear()
        current.append(stripped.lstrip("-* ").strip())
    elif stripped:
        current.append(stripped)
    elif current:
        refs.append(" ".join(current))
        current.clear()


def split_references(text: str) -> tuple[str, list[str]]:
    lines = text.splitlines()
    out: list[str] = []
    refs: list[str] = []
    current: list[str] = []
    in_refs = False

    for line in lines:
        heading = HEADING_RE.match(line)
        if heading and REFERENCES_HEADING.lower() in line.lower():
            in_refs = True
            continue
        if in_refs and heading:
            if current:
                refs.append(" ".join(current))
                current.clear()
            in_refs = False
        if in_refs:
            _accumulate_ref_line(line, current, refs)
            continue
        out.append(line)

    if current:
        refs.append(" ".join(current))
    return "\n".join(out), refs


def demote_and_rewrite(text: str, doc: pathlib.Path) -> str:
    lines = text.splitlines()
    out: list[str] = []
    in_fence = False
    for line in lines:
        stripped = line.lstrip()
        if stripped.startswith("```"):
            in_fence = not in_fence
            out.append(line)
            continue
        if in_fence:
            out.append(line)
            continue
        if HEADING_RE.match(line):
            line = "#" + line
        line = IMAGE_RE.sub(lambda m: _abs_image(m, doc), line)
        out.append(line)
    return "\n".join(out)


def _abs_image(match: re.Match, doc: pathlib.Path) -> str:
    target = match.group(2).strip()
    if target.startswith(("http://", "https://", "/")):
        return match.group(0)
    resolved = (doc.parent / target).resolve()
    return f"{match.group(1)}{resolved}{match.group(3)}"


def normalize_ref(line: str) -> str:
    body = line.lstrip("-* ").strip()
    return re.sub(r"\s+", " ", body).lower()


# ── Assembly ──────────────────────────────────────────────────────────────────

def preprocess(text: str, skip_tikz: bool) -> str:
    """Replace tikz+mermaid blocks with compiled SVGs (or mermaid fallback)."""
    return TIKZ_BLOCK_RE.sub(
        lambda m: _replace_tikz_block(m, skip_tikz),
        text,
    )


def assemble(book_md: pathlib.Path, skip_tikz: bool = False) -> int:
    docs = chapter_files()
    if not docs:
        print("ERROR: no chapter files found in README index.", file=sys.stderr)
        return 1

    parts: list[str] = []
    references: list[str] = []
    seen_refs: set[str] = set()

    for doc in docs:
        raw = doc.read_text(encoding="utf-8")
        # Strip YAML front matter
        if raw.startswith("---"):
            end = raw.find("\n---", 3)
            if end != -1:
                raw = raw[end + 4:].lstrip("\n")

        raw = preprocess(raw, skip_tikz)
        body, refs = split_references(raw)
        parts.append(f"# {_title_of(doc)}\n")
        parts.append(demote_and_rewrite(body, doc).strip() + "\n")

        for ref in refs:
            key = normalize_ref(ref)
            if key and key not in seen_refs:
                seen_refs.add(key)
                references.append(ref.lstrip("-* ").strip())

    if references:
        parts.append("# References\n")
        parts.extend(f"- {ref}" for ref in sorted(references))
        parts.append("")

    book_md.parent.mkdir(parents=True, exist_ok=True)
    book_md.write_text("\n\n".join(parts), encoding="utf-8")
    print(f"Assembled {len(docs)} chapters, {len(references)} references → {book_md}")
    return 0


def _title_of(path: pathlib.Path) -> str:
    text = path.read_text(encoding="utf-8")
    if text.startswith("---"):
        end = text.find("\n---", 3)
        if end != -1:
            text = text[end + 4:]
    m = H1_RE.search(text)
    return m.group(1).strip() if m else path.stem.replace("-", " ").title()


# ── Pandoc rendering ──────────────────────────────────────────────────────────

def _git_version() -> str:
    try:
        out = subprocess.run(
            ["git", "describe", "--tags", "--always", "--dirty"],
            cwd=ROOT, capture_output=True, text=True, check=True,
        )
        return out.stdout.strip()
    except (subprocess.CalledProcessError, FileNotFoundError):
        return "dev"


def pandoc_common(book_md: pathlib.Path) -> list[str]:
    return [
        "pandoc", str(book_md),
        "--metadata-file", str(BOOKLET_ASSETS / "metadata.yaml"),
        "-M", f"date={date.today():%B %d, %Y}",
        "-M", f"version={_git_version()}",
        "--toc", "--toc-depth=2", "--number-sections",
    ]


def render_pdf(book_md: pathlib.Path) -> int:
    out = BUILD_DIR / f"{OUTPUT_STEM}.pdf"
    cmd = pandoc_common(book_md) + [
        "--pdf-engine=xelatex",
        "-H", str(BOOKLET_ASSETS / "preamble.tex"),
        "--include-after-body", str(BOOKLET_ASSETS / "back-cover.tex"),
        "-o", str(out),
    ]
    return _run(cmd, out)


def render_html(book_md: pathlib.Path) -> int:
    out = BUILD_DIR / "index.html"
    shutil.copyfile(BOOKLET_ASSETS / "book.css", BUILD_DIR / "book.css")
    if DIAGRAMS_DIR.exists():
        dest = BUILD_DIR / "diagrams"
        if dest != DIAGRAMS_DIR:  # avoid self-copy
            if dest.exists():
                shutil.rmtree(dest)
            shutil.copytree(DIAGRAMS_DIR, dest)
    cmd = pandoc_common(book_md) + [
        "--standalone",
        f"--mathjax={MATHJAX_CDN}",
        "--css=book.css",
        "-o", str(out),
    ]
    return _run(cmd, out)


def _run(cmd: list[str], out: pathlib.Path) -> int:
    result = subprocess.run(cmd, cwd=ROOT)
    if result.returncode != 0:
        print(f"ERROR: pandoc failed for {out.name}", file=sys.stderr)
        return 1
    print(f"Wrote {out}")
    return 0


# ── Entry point ───────────────────────────────────────────────────────────────

def main() -> int:
    parser = argparse.ArgumentParser(description="Build the e-foc FOC Theory booklet.")
    parser.add_argument("--format", choices=["pdf", "html", "all"], default="all")
    parser.add_argument(
        "--assemble-only", action="store_true",
        help="Write build/booklet/book.md only (no Pandoc).",
    )
    parser.add_argument(
        "--skip-tikz", action="store_true",
        help="Skip TikZ compilation; use Mermaid fallback diagrams.",
    )
    args = parser.parse_args()

    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    book_md = BUILD_DIR / "book.md"

    status = assemble(book_md, skip_tikz=args.skip_tikz)
    if status or args.assemble_only:
        return status

    if shutil.which("pandoc") is None:
        print("ERROR: pandoc not found on PATH.", file=sys.stderr)
        return 2

    rc = 0
    if args.format in ("pdf", "all"):
        rc |= render_pdf(book_md)
    if args.format in ("html", "all"):
        rc |= render_html(book_md)
    return 1 if rc else 0


if __name__ == "__main__":
    raise SystemExit(main())
