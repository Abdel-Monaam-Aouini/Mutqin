---
name: add-dependency
description: Propose a new third-party dependency (C++, Python, model, dataset, font) with its purpose and license before adding it. Use whenever a library, model, or data source is about to be introduced.
---

# add-dependency

Do NOT install or vendor anything first. Report to the user:

| Field | Content |
|-------|---------|
| Name + version | exact |
| Why needed | what breaks without it, alternatives considered (incl. writing it ourselves) |
| License | SPDX id, verified from the upstream repo/LICENSE file, not memory |
| Compatibility | permissive (MIT/Apache-2.0/BSD) preferred; flag GPL/LGPL/CC-NC/unknown for commercial release |
| Platforms | works on Linux, macOS, Windows; mobile-friendly? |
| Size/footprint | binary size impact, transitive deps |

Wait for approval. After approval, add it, then record it in `docs/LICENSES.md`.
