#!/usr/bin/env python3
"""Build the e-foc Design Booklet as a PDF and/or a multi-page static site.

Chapter order is derived from documentation/booklet/README.md: every
`[Title](target.md)` link found in that index is a chapter, in the order the
index lists them. Targets are resolved relative to documentation/booklet/, so
chapters can live anywhere in the tree (theory/, design/, architecture/).

Diagrams are authored as ```mermaid fences inside the chapters. They render on
GitHub as-is; for the booklet each fence is compiled to an SVG with
mermaid-cli (mmdc) and replaced by an image reference. When mmdc is not
available the fence is left in place as a code block so the build still
succeeds.

Outputs (under build/booklet/):
    eFocDesign.pdf    single-file PDF, rendered by Pandoc + XeLaTeX
    site/             static site: index.html + one page per chapter
    book.md           the assembled single-document Markdown (PDF source)

Usage:
    python scripts/build-foc-booklet.py [--format pdf|html|all] [--assemble-only]
                                        [--skip-diagrams]

Exit codes:
    0  success
    1  missing sources or a Pandoc render failed
    2  a required tool (pandoc) is not installed
"""

import argparse
import hashlib
import html
import os
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile
from datetime import date

ROOT = pathlib.Path(__file__).resolve().parent.parent
BOOKLET_DIR = ROOT / "documentation" / "booklet"
INDEX = BOOKLET_DIR / "README.md"
ASSETS = BOOKLET_DIR / "assets"
TIKZ_DIR = ROOT / "documentation" / "tikz"

BUILD_DIR = ROOT / "build" / "booklet"
SITE_DIR = BUILD_DIR / "site"
DIAGRAMS_DIR = BUILD_DIR / "diagrams"

OUTPUT_STEM = "eFocDesign"
BOOK_TITLE = "e-foc"
BOOK_SUBTITLE = "The Design Booklet"
REPO_BLOB = "https://github.com/embedded-pro/e-foc/blob/main"
MATHJAX_CDN = "https://cdn.jsdelivr.net/npm/mathjax@3/es5/tex-mml-chtml.js"

LINK_RE = re.compile(r"\[([^\]]+)\]\(([^)]+\.md)(#[^)]*)?\)")
H1_RE = re.compile(r"^#\s+(.+?)\s*$", re.MULTILINE)
FRONT_MATTER_TITLE_RE = re.compile(r'^title:\s+"?(.+?)"?\s*$', re.MULTILINE)
HEADING_RE = re.compile(r"^(#{1,6})\s")
IMAGE_RE = re.compile(r"(!\[[^\]]*\]\()([^)]+)(\))")
MERMAID_RE = re.compile(r"```mermaid\n(.*?)```", re.DOTALL)
TIKZ_INPUT_RE = re.compile(r"```\{=latex\}\n\\input\{([^}\n]+)\}\n```")
PART_RE = re.compile(r"^##\s+(.+?)\s*$")

DIAGRAM_FAILURES = []


class Chapter:
    def __init__(self, number, title, source, part, page):
        self.number = number
        self.title = title
        self.source = source
        self.part = part
        self.page = page


def read_index():
    if not INDEX.is_file():
        print(f"ERROR: booklet index not found: {INDEX}", file=sys.stderr)
        return []

    part = ""
    seen = set()
    chapters = []

    for line in INDEX.read_text(encoding="utf-8").splitlines():
        part_match = PART_RE.match(line)
        if part_match:
            part = part_match.group(1)
            continue

        for _, target, _anchor in LINK_RE.findall(line):
            target = target.strip()
            if target.startswith(("http://", "https://")):
                continue

            source = (BOOKLET_DIR / target).resolve()
            if source in seen:
                continue
            if not _is_inside_repository(source):
                print(f"ERROR: index links outside the repository: {target}", file=sys.stderr)
                return []
            if (BOOKLET_DIR / target).is_symlink():
                print(f"ERROR: index links a symlink: {target}", file=sys.stderr)
                return []
            if not source.is_file():
                print(f"WARNING: index links a missing file: {target}", file=sys.stderr)
                continue

            seen.add(source)
            number = len(chapters) + 1
            chapters.append(Chapter(number, _title_of(source), source, part, _page_name(source)))

    return chapters


def _is_inside_repository(path):
    try:
        path.relative_to(ROOT)
    except ValueError:
        return False
    return True


def _page_name(source):
    return f"{source.stem}.html"


def _title_of(path):
    text = path.read_text(encoding="utf-8")
    if text.startswith("---"):
        end = text.find("\n---", 3)
        if end != -1:
            m = FRONT_MATTER_TITLE_RE.search(text[3:end])
            if m:
                return m.group(1).strip()
            text = text[end + 4:].lstrip("\n")
    match = H1_RE.search(text)
    return match.group(1).strip() if match else path.stem.replace("-", " ").title()


def _strip_front_matter(text):
    if not text.startswith("---"):
        return text
    end = text.find("\n---", 3)
    return text[end + 4:].lstrip("\n") if end != -1 else text


def _has_tool(name):
    return shutil.which(name) is not None


def _render_mermaid(code, skip_diagrams):
    if skip_diagrams or not _has_tool("mmdc"):
        return None

    DIAGRAMS_DIR.mkdir(parents=True, exist_ok=True)
    digest = hashlib.sha1(code.encode("utf-8")).hexdigest()[:12]
    svg_out = DIAGRAMS_DIR / f"diagram-{digest}.svg"
    if svg_out.exists():
        return svg_out

    mmd_in = DIAGRAMS_DIR / f"diagram-{digest}.mmd"
    mmd_in.write_text(code, encoding="utf-8")

    result = subprocess.run(
        [
            "mmdc",
            "--input", str(mmd_in),
            "--output", str(svg_out),
            "--outputFormat", "svg",
            "--backgroundColor", "white",
            "--configFile", str(ASSETS / "mermaid.json"),
            "--puppeteerConfigFile", str(ASSETS / "puppeteer.json"),
        ],
        capture_output=True, text=True,
    )
    mmd_in.unlink(missing_ok=True)

    if result.returncode != 0 or not svg_out.exists():
        first_err = next((l for l in result.stderr.splitlines() if "rror" in l), "")
        print(f"ERROR: mmdc failed for diagram {digest}: {first_err}", file=sys.stderr)
        if digest not in DIAGRAM_FAILURES:
            DIAGRAM_FAILURES.append(digest)
        return None

    print(f"  diagram-{digest}.svg")
    return svg_out


def _resolve_tikz(tex_name):
    """Return the resolved Path for a TikZ file name, or None if invalid.

    Rejects absolute paths, path traversal (``..``), symlinks, and directories
    so that a crafted Markdown ``\\input{}`` cannot escape TIKZ_DIR or trigger
    unexpected behaviour (e.g. read_text() raising on a directory).
    """
    try:
        candidate = (TIKZ_DIR / tex_name).resolve()
    except (ValueError, OSError):
        return None
    if candidate.is_symlink() or not candidate.is_file():
        return None
    try:
        candidate.relative_to(TIKZ_DIR.resolve())
    except ValueError:
        return None
    return candidate


def _render_tikz(tex_name, skip_diagrams):
    """Compile a TikZ .tex snippet to SVG for HTML output.

    Extracts the tikzpicture environment, wraps it in a standalone document,
    compiles with xelatex, and converts the resulting PDF to SVG with pdf2svg.
    Returns the SVG Path on success, None if tools are unavailable or compilation
    fails (the caller omits the block silently so HTML still assembles).
    """
    if skip_diagrams:
        return None

    tex_path = _resolve_tikz(tex_name)
    if tex_path is None:
        print(f"WARNING: TikZ source not found or invalid: {tex_name}", file=sys.stderr)
        return None

    latex_engine = next(
        (e for e in ("xelatex", "pdflatex", "lualatex") if _has_tool(e)), None
    )
    if latex_engine is None or not _has_tool("pdf2svg"):
        return None

    content = tex_path.read_text(encoding="utf-8")  # tex_path already validated by _resolve_tikz
    digest = hashlib.sha1(content.encode("utf-8")).hexdigest()[:12]

    DIAGRAMS_DIR.mkdir(parents=True, exist_ok=True)
    svg_out = DIAGRAMS_DIR / f"tikz-{digest}.svg"
    if svg_out.exists():
        return svg_out

    begin_tag = r"\begin{tikzpicture}"
    end_tag = r"\end{tikzpicture}"
    start = content.find(begin_tag)
    end = content.rfind(end_tag)
    if start == -1 or end == -1:
        print(f"WARNING: no tikzpicture environment found in {tex_name}", file=sys.stderr)
        return None

    tikzpicture = content[start: end + len(end_tag)]
    wrapper = "\n".join([
        r"\documentclass[border=8pt,tikz]{standalone}",
        r"\usepackage[dvipsnames]{xcolor}",
        r"\usepackage{pgfplots}",
        r"\pgfplotsset{compat=1.18}",
        r"\usetikzlibrary{arrows.meta,calc,positioning}",
        r"\tikzset{arr/.style={-{Stealth[length=5pt]},thick}}",
        r"\begin{document}",
        tikzpicture,
        r"\end{document}",
    ])

    with tempfile.TemporaryDirectory() as tmpdir:
        tmpdir = pathlib.Path(tmpdir)
        tex_file = tmpdir / "figure.tex"
        tex_file.write_text(wrapper, encoding="utf-8")
        pdf_file = tmpdir / "figure.pdf"

        result = subprocess.run(
            [
                latex_engine,
                "-interaction=nonstopmode",
                "-halt-on-error",
                f"-output-directory={tmpdir}",
                str(tex_file),
            ],
            capture_output=True, text=True, cwd=tmpdir,
        )
        if result.returncode != 0 or not pdf_file.exists():
            errors = [l for l in result.stdout.splitlines() if l.startswith("!")]
            print(
                f"WARNING: {latex_engine} failed for {tex_name}: "
                f"{errors[0] if errors else 'see build log'}",
                file=sys.stderr,
            )
            return None

        conv = subprocess.run(
            ["pdf2svg", str(pdf_file), str(svg_out)],
            capture_output=True, text=True,
        )
        if conv.returncode != 0 or not svg_out.exists():
            print(f"WARNING: pdf2svg failed for {tex_name}", file=sys.stderr)
            return None

    print(f"  tikz-{digest}.svg ({tex_name})")
    return svg_out


def _strip_tikz_svg_refs(text):
    """Remove pre-committed SVG image refs that precede {=latex} blocks.

    In the PDF path the TikZ source is rendered natively; the markdown image
    reference must be removed so the figure does not appear twice.
    """
    lines = text.splitlines(keepends=True)
    out = []
    i = 0
    while i < len(lines):
        line = lines[i]
        # Detect: ![any alt](…/tikz/images/….svg) on its own line
        stripped = line.rstrip("\n")
        if (
            stripped.startswith("![")
            and "/tikz/images/" in stripped
            and stripped.endswith(")")
        ):
            # Drop this line and the blank line that follows it (if present)
            if i + 1 < len(lines) and lines[i + 1].strip() == "":
                i += 2
                continue
        out.append(line)
        i += 1
    return "".join(out)


def _replace_tikz(text, skip_diagrams):
    def replace(match):
        tex_name = match.group(1)
        if not tex_name.endswith(".tex"):
            return match.group(0)
        # Pre-committed SVG already present in the markdown — no runtime work needed
        svg_name = pathlib.Path(tex_name).with_suffix(".svg").name
        precommitted = TIKZ_DIR / "images" / svg_name
        if precommitted.is_file() and not precommitted.is_symlink():
            return ""
        svg = _render_tikz(tex_name, skip_diagrams)
        if svg is None:
            return ""
        alt = pathlib.Path(tex_name).stem.replace("-", " ").title()
        return f"![{alt}]({svg})"

    return TIKZ_INPUT_RE.sub(replace, text)


def _diagram_alt(code):
    first = code.strip().splitlines()[0].strip().lower() if code.strip() else ""
    if "sequencediagram" in first:
        return "Sequence diagram"
    if "statediagram" in first:
        return "State diagram"
    if "classdiagram" in first:
        return "Class diagram"
    return "Block diagram"


def _replace_diagrams(text, skip_diagrams):
    def replace(match):
        code = match.group(1)
        svg = _render_mermaid(code, skip_diagrams)
        return f"![{_diagram_alt(code)}]({svg})" if svg else match.group(0)

    return MERMAID_RE.sub(replace, text)


def _absolute_images(text, source):
    def replace(match):
        target = match.group(2).strip()
        if target.startswith(("http://", "https://", "/")):
            return match.group(0)
        return f"{match.group(1)}{(source.parent / target).resolve()}{match.group(3)}"

    return IMAGE_RE.sub(replace, text)


def _rewrite_links(text, source, chapters, output):
    by_source = {chapter.source: chapter for chapter in chapters}

    def replace(match):
        label, target, anchor = match.group(1), match.group(2).strip(), match.group(3) or ""
        if target.startswith(("http://", "https://")):
            return match.group(0)

        resolved = (source.parent / target).resolve()
        chapter = by_source.get(resolved)

        if chapter is not None:
            if output == "html":
                return f"[{label}]({chapter.page}{anchor})"
            return f"{label} (Chapter {chapter.number})"

        try:
            relative = resolved.relative_to(ROOT)
        except ValueError:
            return label
        return f"[{label}]({REPO_BLOB}/{relative}{anchor})"

    return LINK_RE.sub(replace, text)


def _prepare(source, chapters, output, skip_diagrams):
    text = _strip_front_matter(source.read_text(encoding="utf-8"))
    text = _replace_diagrams(text, skip_diagrams)
    if output == "html":
        text = _replace_tikz(text, skip_diagrams)
    elif output == "pdf":
        text = _strip_tikz_svg_refs(text)
    text = _absolute_images(text, source)
    text = _rewrite_links(text, source, chapters, output)
    return text.strip() + "\n"


def _git_version():
    try:
        result = subprocess.run(
            ["git", "describe", "--tags", "--always", "--dirty"],
            cwd=ROOT, capture_output=True, text=True, check=True,
        )
        return result.stdout.strip()
    except (subprocess.CalledProcessError, FileNotFoundError):
        return "dev"


def _latex_escape(text):
    for char, replacement in (
        ("\\", r"\textbackslash{}"), ("&", r"\&"), ("%", r"\%"),
        ("$", r"\$"), ("#", r"\#"), ("_", r"\_"), ("{", r"\{"), ("}", r"\}"),
    ):
        text = text.replace(char, replacement)
    return text


def assemble(chapters, book_md, skip_diagrams):
    parts = []
    current_part = None

    for chapter in chapters:
        if chapter.part and chapter.part != current_part:
            current_part = chapter.part
            parts.append(f"\\part{{{_latex_escape(chapter.part)}}}\n")
        parts.append(_prepare(chapter.source, chapters, "pdf", skip_diagrams))

    book_md.parent.mkdir(parents=True, exist_ok=True)
    book_md.write_text("\n\n".join(parts), encoding="utf-8")
    print(f"Assembled {len(chapters)} chapters → {book_md}")


def render_pdf(book_md):
    output = BUILD_DIR / f"{OUTPUT_STEM}.pdf"
    command = [
        "pandoc", str(book_md),
        "--metadata-file", str(ASSETS / "metadata.yaml"),
        "-M", f"date={date.today():%B %d, %Y}",
        "-M", f"version={_git_version()}",
        "--from", "markdown+raw_tex",
        "--top-level-division=chapter",
        "--toc", "--toc-depth=2",
        "--pdf-engine=xelatex",
        "-H", str(ASSETS / "preamble.tex"),
        "--include-after-body", str(ASSETS / "back-cover.tex"),
        "-o", str(output),
    ]
    env = os.environ.copy()
    tikz_dir = str(ROOT / "documentation" / "tikz")
    base = env.get("TEXINPUTS", "")
    # Trailing pathsep preserves TeX's default search path; os.pathsep for portability
    env["TEXINPUTS"] = os.pathsep.join(filter(None, [tikz_dir, base])) + os.pathsep
    return _run(command, output, env=env)


def _sidebar(chapters, current, pdf_available=True):
    items = []
    part = None
    for chapter in chapters:
        if chapter.part and chapter.part != part:
            part = chapter.part
            items.append(f'<li class="part-label">{html.escape(part)}</li>')
        current_attr = ' aria-current="page"' if chapter is current else ""
        items.append(
            f'<li><a href="{chapter.page}"{current_attr}>'
            f"{chapter.number}. {html.escape(chapter.title)}</a></li>"
        )

    download = (
        f'<a class="download" href="{OUTPUT_STEM}.pdf">Download PDF</a>\n'
        if pdf_available else ""
    )
    return (
        '<div class="layout">\n'
        '<nav class="sidebar">\n'
        f'<a class="brand" href="index.html">{html.escape(BOOK_TITLE)}</a>\n'
        f'<span class="brand-sub">{html.escape(BOOK_SUBTITLE)}</span>\n'
        "<ol>\n" + "\n".join(items) + "\n</ol>\n"
        + download
        + "</nav>\n<main>\n"
    )


def _pager(chapters, current):
    index = chapters.index(current)
    prev = (
        f'<a href="{chapters[index - 1].page}">← {html.escape(chapters[index - 1].title)}</a>'
        if index > 0 else '<a href="index.html">← Contents</a>'
    )
    nxt = (
        f'<a href="{chapters[index + 1].page}">{html.escape(chapters[index + 1].title)} →</a>'
        if index + 1 < len(chapters) else '<span class="spacer"></span>'
    )
    return f'<div class="pager">{prev}{nxt}</div>\n</main>\n</div>\n'


def render_site(chapters, skip_diagrams, pdf_available=True):
    SITE_DIR.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(ASSETS / "book.css", SITE_DIR / "book.css")

    if DIAGRAMS_DIR.exists():
        dest = SITE_DIR / "diagrams"
        if dest.exists():
            shutil.rmtree(dest)
        shutil.copytree(DIAGRAMS_DIR, dest)

    status = 0
    fragments = BUILD_DIR / "fragments"
    fragments.mkdir(parents=True, exist_ok=True)

    for chapter in chapters:
        markdown = _prepare(chapter.source, chapters, "html", skip_diagrams)
        markdown = re.sub(
            rf"!\[([^\]]*)\]\({re.escape(str(DIAGRAMS_DIR))}/",
            r"![\1](diagrams/",
            markdown,
        )
        markdown = re.sub(r"\A#\s+.+?\n", "", markdown, count=1)

        chapter_md = fragments / f"{chapter.source.stem}.md"
        chapter_md.write_text(markdown, encoding="utf-8")

        heading = f'<h1 class="title">{html.escape(chapter.title)}</h1>'
        before = fragments / f"{chapter.source.stem}-before.html"
        after = fragments / f"{chapter.source.stem}-after.html"
        before.write_text(_sidebar(chapters, chapter, pdf_available) + heading + "\n", encoding="utf-8")
        after.write_text(_pager(chapters, chapter), encoding="utf-8")

        command = [
            "pandoc", str(chapter_md),
            "--standalone",
            "--from", "markdown",
            "--toc", "--toc-depth=3",
            f"--mathjax={MATHJAX_CDN}",
            "--css", "book.css",
            "-M", f"pagetitle={chapter.title} — {BOOK_TITLE} {BOOK_SUBTITLE}",
            "-M", "document-css=false",
            "--include-before-body", str(before),
            "--include-after-body", str(after),
            "-o", str(SITE_DIR / chapter.page),
        ]
        status |= _run(command, SITE_DIR / chapter.page, quiet=True)

    status |= _render_landing(chapters, pdf_available)
    if status == 0:
        print(f"Wrote site with {len(chapters)} chapters → {SITE_DIR}")
    return status


def _render_landing(chapters, pdf_available=True):
    sections = []
    part = None
    for chapter in chapters:
        if chapter.part and chapter.part != part:
            if part is not None:
                sections.append("</div>")
            part = chapter.part
            sections.append(f"<h2>{html.escape(part)}</h2>")
            sections.append('<div class="landing-grid">')
        sections.append(
            f'<a href="{chapter.page}"><span class="num">Chapter {chapter.number}</span>'
            f"{html.escape(chapter.title)}</a>"
        )
    if part is not None:
        sections.append("</div>")

    title = f"{BOOK_TITLE} — {BOOK_SUBTITLE}"
    pdf_blurb = (
        f' Also available as a single <a href="{OUTPUT_STEM}.pdf">PDF</a>.'
        if pdf_available else ""
    )
    page = [
        "<!DOCTYPE html>",
        '<html lang="en">',
        "<head>",
        '<meta charset="utf-8">',
        '<meta name="viewport" content="width=device-width, initial-scale=1.0">',
        f"<title>{html.escape(title)}</title>",
        '<link rel="stylesheet" href="book.css">',
        "</head>",
        "<body>",
        _sidebar(chapters, None, pdf_available),
        f'<h1 class="title">{html.escape(title)}</h1>',
        "<p>Theory, design, and architecture of the e-foc Field-Oriented Control firmware. "
        f"No source code — components, responsibilities, and signal flows only.{pdf_blurb}</p>",
        *sections,
        f'<p class="build-meta">Revision <code>{html.escape(_git_version())}</code> · '
        f"built {date.today():%B %d, %Y}</p>",
        "</main>",
        "</div>",
        "</body>",
        "</html>",
        "",
    ]

    output = SITE_DIR / "index.html"
    output.write_text("\n".join(page), encoding="utf-8")
    return 0


def _run(command, output, quiet=False, env=None):
    result = subprocess.run(command, cwd=ROOT, env=env)
    if result.returncode != 0:
        print(f"ERROR: pandoc failed for {output.name}", file=sys.stderr)
        return 1
    if not quiet:
        print(f"Wrote {output}")
    return 0


def main():
    parser = argparse.ArgumentParser(description="Build the e-foc Design Booklet.")
    parser.add_argument("--format", choices=["pdf", "html", "all"], default="all")
    parser.add_argument("--assemble-only", action="store_true",
                        help="Write build/booklet/book.md only (no Pandoc).")
    parser.add_argument("--skip-diagrams", action="store_true",
                        help="Leave mermaid fences as code blocks instead of rendering them.")
    args = parser.parse_args()

    chapters = read_index()
    if not chapters:
        return 1

    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    book_md = BUILD_DIR / "book.md"

    if not args.skip_diagrams and not _has_tool("mmdc"):
        print("WARNING: mmdc not found — diagrams are kept as code blocks.", file=sys.stderr)

    assemble(chapters, book_md, args.skip_diagrams)
    if args.assemble_only:
        return 0

    if not _has_tool("pandoc"):
        print("ERROR: pandoc not found on PATH.", file=sys.stderr)
        return 2

    status = 0
    pdf_available = args.format in ("pdf", "all")
    if pdf_available:
        status |= render_pdf(book_md)
    if args.format in ("html", "all"):
        status |= render_site(chapters, args.skip_diagrams, pdf_available)
        pdf = BUILD_DIR / f"{OUTPUT_STEM}.pdf"
        if pdf.is_file():
            shutil.copyfile(pdf, SITE_DIR / pdf.name)

    if DIAGRAM_FAILURES:
        print(
            f"ERROR: {len(DIAGRAM_FAILURES)} diagram(s) failed to render: "
            f"{', '.join(DIAGRAM_FAILURES)}",
            file=sys.stderr,
        )
        status |= 1

    return 1 if status else 0


if __name__ == "__main__":
    raise SystemExit(main())
