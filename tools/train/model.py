"""Small CTC acoustic model: conv subsampling (x4) + Transformer encoder + linear layer.

Output: 32 log-probabilities per frame (blank + the 31 phonemes), 25 frames per second. This is the
pipeline-proving model for CPU runs; the size is set by the config, so the same code scales to a GPU.
The streaming (causal) variant needed for the app is a later step (docs/DATA_AND_MODEL.md).
"""

from __future__ import annotations

import math
from dataclasses import dataclass

import torch
from torch import nn

VOCAB_SIZE = 32


@dataclass(frozen=True)
class ModelConfig:
    d_model: int = 128
    layers: int = 3
    heads: int = 4
    ff: int = 512
    dropout: float = 0.1


class PositionalEncoding(nn.Module):
    def __init__(self, d_model: int, max_len: int = 4000) -> None:
        super().__init__()
        pos = torch.arange(max_len).unsqueeze(1)
        div = torch.exp(torch.arange(0, d_model, 2) * (-math.log(10000.0) / d_model))
        pe = torch.zeros(max_len, d_model)
        pe[:, 0::2], pe[:, 1::2] = torch.sin(pos * div), torch.cos(pos * div)
        self.register_buffer("pe", pe, persistent=False)

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        return x + self.pe[: x.size(1)]


class CtcModel(nn.Module):
    def __init__(self, config: ModelConfig = ModelConfig(), n_mels: int = 80) -> None:  # noqa: B008
        super().__init__()
        d = config.d_model
        self.subsample = nn.Sequential(
            nn.Conv2d(1, 16, 3, stride=2, padding=1),
            nn.ReLU(),
            nn.Conv2d(16, 16, 3, stride=2, padding=1),
            nn.ReLU(),
        )
        self.proj = nn.Linear(16 * math.ceil(math.ceil(n_mels / 2) / 2), d)
        self.pos = PositionalEncoding(d)
        layer = nn.TransformerEncoderLayer(d, config.heads, config.ff, config.dropout, batch_first=True)
        self.encoder = nn.TransformerEncoder(layer, config.layers)
        self.out = nn.Linear(d, VOCAB_SIZE)

    @staticmethod
    def out_lengths(lengths: torch.Tensor) -> torch.Tensor:
        return ((lengths - 1) // 2 + 1 - 1) // 2 + 1

    def forward(self, feats: torch.Tensor, lengths: torch.Tensor) -> tuple[torch.Tensor, torch.Tensor]:
        """feats (B, T, 80) -> log-probs (B, T', 32) and output lengths."""
        x = self.subsample(feats.unsqueeze(1))  # (B, C, T', F')
        b, c, t, f = x.shape
        x = self.proj(x.permute(0, 2, 1, 3).reshape(b, t, c * f))
        out_len = self.out_lengths(lengths).clamp(max=t)
        mask = torch.arange(t, device=x.device)[None, :] >= out_len[:, None]
        x = self.encoder(self.pos(x), src_key_padding_mask=mask)
        return self.out(x).log_softmax(-1), out_len
