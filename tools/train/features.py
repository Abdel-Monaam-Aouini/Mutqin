"""Log-mel features identical to core/src/features/logmel.cpp (a parity test compares them).

16 kHz mono float audio -> frames of 25 ms every 10 ms, per-frame mean removal, pre-emphasis 0.97,
Hann window, 512-point power spectrum, 80 triangular mel filters (20-8000 Hz), natural log.
"""

from __future__ import annotations

import numpy as np

SAMPLE_RATE = 16000
FRAME_LENGTH = 400
FRAME_SHIFT = 160
FFT_SIZE = 512
NUM_MELS = 80
LOW_FREQ = 20.0
HIGH_FREQ = 8000.0
PREEMPHASIS = 0.97
FLOOR = 1.1920929e-7


def _hz_to_mel(hz: np.ndarray | float) -> np.ndarray | float:
    return 1127.0 * np.log(1.0 + np.asarray(hz) / 700.0)


def mel_filters() -> np.ndarray:
    bins = FFT_SIZE // 2 + 1
    mel_lo, mel_hi = _hz_to_mel(LOW_FREQ), _hz_to_mel(HIGH_FREQ)
    step = (mel_hi - mel_lo) / (NUM_MELS + 1)
    mel = _hz_to_mel(np.arange(bins) * (SAMPLE_RATE / FFT_SIZE))
    filters = np.zeros((NUM_MELS, bins), dtype=np.float64)
    for m in range(NUM_MELS):
        left = mel_lo + m * step
        center, right = left + step, left + 2 * step
        up = (mel > left) & (mel <= center)
        down = (mel > center) & (mel < right)
        filters[m, up] = (mel[up] - left) / (center - left)
        filters[m, down] = (right - mel[down]) / (right - center)
    return filters


_FILTERS = mel_filters()
_WINDOW = 0.5 - 0.5 * np.cos(2.0 * np.pi * np.arange(FRAME_LENGTH) / (FRAME_LENGTH - 1))


def log_mel(samples: np.ndarray) -> np.ndarray:
    """(frames, 80) float32 features of 16 kHz mono audio in [-1, 1]."""
    x = np.asarray(samples, dtype=np.float64)
    if len(x) < FRAME_LENGTH:
        return np.zeros((0, NUM_MELS), dtype=np.float32)
    count = 1 + (len(x) - FRAME_LENGTH) // FRAME_SHIFT
    index = np.arange(FRAME_LENGTH)[None, :] + FRAME_SHIFT * np.arange(count)[:, None]
    frames = x[index]
    frames = frames - frames.mean(axis=1, keepdims=True)
    shifted = np.concatenate([frames[:, :1], frames[:, :-1]], axis=1)
    frames = (frames - PREEMPHASIS * shifted) * _WINDOW
    spectrum = np.fft.rfft(frames, n=FFT_SIZE, axis=1)
    power = spectrum.real**2 + spectrum.imag**2
    return np.log(np.maximum(power @ _FILTERS.T, FLOOR)).astype(np.float32)
