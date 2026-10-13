---
paths:
  - "core/**"
  - "cli/**"
---

# C++ core rules

- C++17, CMake. Portable: Linux, macOS, Windows. No `#ifdef __ANDROID__`/`__APPLE__` platform
  code in `core/`; platform concerns (audio capture) belong behind an interface the CLI/apps supply.
- Only public header: `core/include/mutqin.h`, plain C, prefix `mq_`, opaque handles, no C++ types,
  no exceptions across the boundary, explicit ABI-stable structs. Internals live in `namespace mutqin`.
- Audio inside the core: 16 kHz, mono, float32.
- Audio path is real-time safe: no allocation, no locks, no logging in the push-audio path.
- Tests: GoogleTest, every module. Warnings on (`-Wall -Wextra`/`/W4`), treat as errors in CI.
- Mobile later: keep the API free of anything that blocks Kotlin/Swift bindings (callbacks via
  poll, not threads owned by the caller).
- `mutqin-cli` is a thin client of the C API (dogfooding it); microphone capture lives in `cli/`,
  never in `core/`.
