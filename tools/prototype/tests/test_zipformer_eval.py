from __future__ import annotations

import pytest

from tools.prototype.zipformer_eval import tokenize


def test_tokenize_uses_the_longest_matching_unit():
    units = {"a", "b", "bb", "ba", "c"}
    assert tokenize("bbac", units) == ["bb", "a", "c"]
    assert tokenize("bac", units) == ["ba", "c"]


def test_tokenize_rejects_a_unit_outside_the_vocabulary():
    with pytest.raises(ValueError):
        tokenize("abz", {"a", "b"})
