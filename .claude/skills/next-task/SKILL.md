---
name: next-task
description: Execute the next unchecked task in docs/TASKS.md following the Mutqin one-task-at-a-time workflow. Use when the user says "next", "next task", or "do task N.M".
---

# next-task

1. Open `docs/TASKS.md`; pick the first unchecked task (or the one the user named). Skip tasks marked deferred.
2. Say in 2–3 lines what you will do and which files you will create or change.
3. Re-read the doc relevant to the task (`docs/ARCHITECTURE.md`, `TAJWEED_SPEC.md`, ...) and `.claude/rules/`.
4. Implement only that task. Respect the hard rules in `CLAUDE.md` (no Quran text from memory,
   no edits to `data/quran/`, no invented tajweed, ask before dependencies).
5. Run the checks and show the real output; if anything fails, say so and do not tick the box:
   tests (C++: `ctest --test-dir build --output-on-failure`; Python: `.venv/bin/python -I -m pytest`),
   format (`.venv/bin/ruff format tools`, `.venv/bin/clang-format -i` on C++ files) and lint
   (`.venv/bin/ruff check tools`, build with `-DMUTQIN_WERROR=ON`).
6. Tick the box in `docs/TASKS.md` only when tests pass.
7. Commit with a Conventional Commits message (`type(scope): subject`, lower-case, no period, no AI
   mention), push to origin, then continue with the next task. Stop only if blocked on the user.
