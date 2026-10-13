---
paths:
  - "tools/**"
---

# Python tools rules

- Python only under `tools/`. Never imported by `core/`.
- The Python reference phonemizer is the source of truth; C++ must match it exactly.
- pytest for tests; type hints; pin dependencies in `tools/requirements.txt` with licenses noted.
- Downloads write to `data/quran/` once, record SHA-256 in `checksums.txt`, and never overwrite
  existing verified files silently.
- Run Python with `-I` when processing downloaded/untrusted data.
