---
description: "e-foc document specifics: allowed image references and the content each document has."
applyTo: "documentation/**"
---

# e-foc Document Specifics

## Image syntax

The only image references allowed, each from a document one folder below `documentation/`:

- TikZ preview, next to its ` ```{=latex} ` block: `![alt](../tikz/images/<name>.svg)`
- Animation, in `documentation/theory/` only: `![alt](../animations/<name>.svg)`

## Content

Each document includes, where it applies:

- Mathematical background (equations for transforms, PID tuning, ...)
- Control-loop diagram (Mermaid or ASCII)
- Hardware dependencies, as capabilities the platform provides (theory and performance documents may name parts;
  design and architecture documents do not)
- Tuning guidance
