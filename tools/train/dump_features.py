"""Write a deterministic test signal and its numpy log-mel features for the C++ parity test.

Usage: python3 -I tools/train/dump_features.py OUT
File: int32 sample count N, int32 frame count F, N float32 samples, F*80 float32 features.
"""

from __future__ import annotations

import struct
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.train.features import NUM_MELS, log_mel  # noqa: E402


def test_signal() -> np.ndarray:
    t = np.arange(24000) / 16000.0
    rng = np.random.default_rng(7)
    x = 0.3 * np.sin(2 * np.pi * 220 * t) + 0.2 * np.sin(2 * np.pi * 1700 * t * (1 + 0.1 * t))
    x += 0.05 * rng.standard_normal(len(t))
    x[8000:9600] *= 0.01  # a quiet stretch
    return x.astype(np.float32)


def main() -> int:
    x = test_signal()
    feats = log_mel(x)
    assert feats.shape[1] == NUM_MELS
    with Path(sys.argv[1]).open("wb") as fh:
        fh.write(struct.pack("<ii", len(x), len(feats)))
        fh.write(x.tobytes())
        fh.write(np.ascontiguousarray(feats).tobytes())
    print(f"wrote {len(x)} samples, {len(feats)} frames")
    return 0


if __name__ == "__main__":
    sys.exit(main())
