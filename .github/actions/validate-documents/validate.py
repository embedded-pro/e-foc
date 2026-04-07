#!/usr/bin/env python3
"""Validate markdown documentation files against architecture and design templates."""

import argparse
import re
import sys
from pathlib import Path

import yaml


ARCHITECTURE_REQUIRED_SECTIONS = [
    "Assumptions & Constraints",
    "System Overview",
    "Component Decomposition",
    "Interfaces & Contracts",
]

DESIGN_REQUIRED_SECTIONS = [
    "Responsibilities",
    "Component Details",
    "Interfaces",
]

THEORY_REQUIRED_SECTIONS = [
    "Overview",
    "Mathematical Foundation",
    "Numerical Properties",
]

REQUIRED_FRONTMATTER_FIELDS = ["title", "type", "status", "version", "component"]

FORBIDDEN_CODE_LANGUAGES = {
    "cpp", "c", "c++", "cxx",
    "python", "py",
    "java",
    "javascript", "js",
    "typescript", "ts",
    "rust",
    "go",
    "bash", "sh", "shell",
    "cmake",
    "makefile",
}


def parse_frontmatter(text: str, relative_path: Path) -> tuple[dict | None, list[str]]:
    """Parse YAML frontmatter from a markdown document.

    Returns (parsed_dict_or_None, list_of_errors).
    """
    errors = []
    if not text.startswith("---"):
        errors.append("missing YAML frontmatter (document must start with ---)")
        return None, errors

    end = text.find("\n---", 3)
    if end == -1:
        errors.append("unclosed YAML frontmatter (no closing ---)")
        return None, errors

    raw_yaml = text[3:end].strip()
    try:
        data = yaml.safe_load(raw_yaml)
    except yaml.YAMLError as exc:
        errors.append(f"YAML parse error in frontmatter: {exc}")
        return None, errors

    if not isinstance(data, dict):
        errors.append("frontmatter is not a YAML mapping")
        return None, errors

    return data, errors


def check_required_fields(frontmatter: dict) -> list[str]:
    """Return a list of error messages for missing required frontmatter fields."""
    errors = []
    for field in REQUIRED_FRONTMATTER_FIELDS:
        if field not in frontmatter or frontmatter[field] is None:
            errors.append(f"missing required frontmatter field: '{field}'")
    return errors


def check_required_sections(text: str, required_sections: list[str]) -> list[str]:
    """Return error messages for required H2 sections that are absent."""
    errors = []
    for section in required_sections:
        pattern = re.compile(r"^## " + re.escape(section) + r"(\s|$)", re.MULTILINE)
        if not pattern.search(text):
            errors.append(f"missing required section: '## {section}'")
    return errors


def check_implementation_blindness(text: str) -> list[str]:
    """Scan fenced code blocks and report any that use forbidden programming languages."""
    errors = []
    fence_pattern = re.compile(r"^```(\w*)", re.MULTILINE)
    for match in fence_pattern.finditer(text):
        lang = match.group(1).strip().lower()
        if lang in FORBIDDEN_CODE_LANGUAGES:
            line_number = text[: match.start()].count("\n") + 1
            errors.append(
                f"forbidden code block language '{lang}' at line {line_number} "
                "(design documents must not contain programming-language code blocks)"
            )
    return errors


def check_no_image_references(text: str) -> list[str]:
    """Check that no external image references (![alt](src)) are used.

    All visuals must be Mermaid code blocks or ASCII art.
    """
    errors = []
    image_pattern = re.compile(r"!\[([^\]]*)\]\(([^)]+)\)")
    for line_num, line in enumerate(text.splitlines(), 1):
        if image_pattern.search(line):
            errors.append(
                f"line {line_num}: external image reference not allowed "
                "— use a Mermaid code block or ASCII art instead"
            )
    return errors


def is_template_file(path: Path, documents_dir: Path) -> bool:
    """Return True if the file is a template (skipped during validation)."""
    try:
        parts = path.relative_to(documents_dir).parts
    except ValueError:
        return False
    return len(parts) >= 2 and parts[-2] == "templates" and path.name in (
        "architecture.md",
        "design.md",
        "theory.md",
    )


def validate_file(
    md_file: Path,
    documents_dir: Path,
    doc_type_filter: str,
) -> tuple[list[str], list[str]]:
    """Validate a single markdown file.

    Returns (errors, warnings).
    """
    errors: list[str] = []
    warnings: list[str] = []

    text = md_file.read_text(encoding="utf-8")
    if not text.strip():
        warnings.append("file is empty")
        return errors, warnings

    frontmatter, fm_errors = parse_frontmatter(text, md_file)
    errors.extend(fm_errors)
    if frontmatter is None:
        return errors, warnings

    errors.extend(check_required_fields(frontmatter))

    doc_type = str(frontmatter.get("type", "")).lower()

    if doc_type_filter != "all" and doc_type != doc_type_filter:
        # Not the type we are filtering for — skip silently.
        return [], []

    if doc_type == "architecture":
        errors.extend(check_required_sections(text, ARCHITECTURE_REQUIRED_SECTIONS))
        errors.extend(check_no_image_references(text))
    elif doc_type == "design":
        errors.extend(check_required_sections(text, DESIGN_REQUIRED_SECTIONS))
        errors.extend(check_implementation_blindness(text))
        errors.extend(check_no_image_references(text))
    elif doc_type == "theory":
        errors.extend(check_required_sections(text, THEORY_REQUIRED_SECTIONS))
        # Image references are allowed in theory documents (generated plots from documentation/tools/)
    else:
        warnings.append(f"unknown document type '{doc_type}' — skipped type-specific checks")

    return errors, warnings


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Validate markdown documentation against architecture and design templates."
    )
    parser.add_argument(
        "--documents-dir",
        required=True,
        type=Path,
        help="Directory to search for markdown documentation files.",
    )
    parser.add_argument(
        "--doc-type",
        default="all",
        choices=["architecture", "design", "theory", "all"],
        help="Filter validation to a specific document type (default: all).",
    )
    args = parser.parse_args()

    documents_dir: Path = args.documents_dir
    if not documents_dir.is_dir():
        print(f"✗ --documents-dir '{documents_dir}' is not a directory", file=sys.stderr)
        return 1

    md_files = sorted(documents_dir.rglob("*.md"))
    failed = False

    for md_file in md_files:
        if is_template_file(md_file, documents_dir):
            continue

        relative_path = md_file.relative_to(documents_dir)
        errors, warnings = validate_file(md_file, documents_dir, args.doc_type)

        for warning in warnings:
            print(f"⚠ {relative_path}: {warning}")

        if errors:
            for error in errors:
                print(f"✗ {relative_path}: {error}")
            failed = True
        elif not warnings:
            # Only print ✓ when the file was actually checked (not skipped).
            # A file is skipped when validate_file returns ([], []) due to type mismatch.
            pass

        if not errors and not warnings:
            # Determine whether we actually validated this file or skipped it.
            text = md_file.read_text(encoding="utf-8")
            if text.strip():
                fm, _ = parse_frontmatter(text, relative_path)
                if fm is not None:
                    doc_type = str(fm.get("type", "")).lower()
                    if args.doc_type == "all" or doc_type == args.doc_type:
                        print(f"✓ {relative_path}")

    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
