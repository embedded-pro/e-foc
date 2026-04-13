#!/usr/bin/env python3
"""
convert_docs.py  —  Converts documentation Markdown files and YAML
requirements to PDF using Pandoc.

Usage:
    python3 convert_docs.py \\
        --version v1.2.3 \\
        --output-dir docs-pdf \\
        --docs-dirs documentation/architecture documentation/design documentation/theory \\
        --requirements-dir documentation/requirements
"""

from __future__ import annotations

import argparse
import subprocess
import sys
import tempfile
from pathlib import Path

import yaml


PANDOC_FLAGS = [
    "--pdf-engine=pdflatex",
    "-V", "geometry:margin=2cm",
    "-V", "fontsize=11pt",
    "--toc",
    "--standalone",
]


def _run_pandoc(output_path: Path, *input_paths: Path) -> bool:
    cmd = ["pandoc", *PANDOC_FLAGS, "-o", str(output_path), *[str(p) for p in input_paths]]
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"WARNING: pandoc failed for {output_path.name}: {result.stderr.strip()}", file=sys.stderr)
        return False
    print(f"Generated: {output_path}")
    return True


def convert_markdown_dirs(docs_dirs: list[str], version: str, output_dir: Path) -> None:
    for docdir in docs_dirs:
        path = Path(docdir)
        if not path.is_dir():
            continue
        category = path.name
        for md in sorted(path.rglob("*.md")):
            name = md.stem
            out = output_dir / f"e_foc-{category}-{name}-{version}.pdf"
            _run_pandoc(out, md)


def _requirement_to_markdown(req: dict) -> str:
    req_id = req.get("id", "").strip()
    title = req.get("title", "").strip()
    shall = req.get("shall", "").strip()
    lines = [f"## {req_id}: {title}", ""]
    if shall:
        lines.append(shall)
        lines.append("")
    return "\n".join(lines)


def convert_requirements(requirements_dir: Path, version: str, output_dir: Path) -> None:
    yaml_files = sorted(requirements_dir.rglob("*.yaml"))
    if not yaml_files:
        print(f"INFO: No YAML files found in {requirements_dir}", file=sys.stderr)
        return

    with tempfile.TemporaryDirectory() as tmp:
        tmp_mds: list[Path] = []
        for yaml_file in yaml_files:
            with yaml_file.open(encoding="utf-8") as fh:
                items = yaml.safe_load(fh) or []
            if not isinstance(items, list):
                continue
            md_content = "\n".join(_requirement_to_markdown(r) for r in items)
            tmp_md = Path(tmp) / f"{yaml_file.stem}.md"
            tmp_md.write_text(md_content, encoding="utf-8")
            tmp_mds.append(tmp_md)

        if not tmp_mds:
            print("INFO: No requirements content to convert", file=sys.stderr)
            return

        out = output_dir / f"e_foc-requirements-{version}.pdf"
        _run_pandoc(out, *tmp_mds)


def main() -> int:
    parser = argparse.ArgumentParser(description="Convert documentation to PDF")
    parser.add_argument("--version", required=True, help="Release version tag (e.g. v1.2.3)")
    parser.add_argument("--output-dir", default="docs-pdf", help="Output directory for PDFs")
    parser.add_argument(
        "--docs-dirs",
        nargs="+",
        default=["documentation/architecture", "documentation/design", "documentation/theory"],
        help="Space-separated list of documentation directories to convert",
    )
    parser.add_argument(
        "--requirements-dir",
        default="documentation/requirements",
        help="Directory containing YAML requirement files",
    )
    args = parser.parse_args()

    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    convert_markdown_dirs(args.docs_dirs, args.version, output_dir)
    convert_requirements(Path(args.requirements_dir), args.version, output_dir)

    return 0


if __name__ == "__main__":
    sys.exit(main())
