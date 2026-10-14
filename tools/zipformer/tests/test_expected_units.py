from __future__ import annotations

import pytest

from tools.zipformer.expected_units import _monotone, tokenize


def test_tokenize_prefers_the_longest_unit_and_rejects_unknown_text():
    units = {"a", "b", "bb", "ba"}
    assert tokenize("bba", units) == ["bb", "a"]
    with pytest.raises(ValueError):
        tokenize("az", units)


def test_monotone_assignment_uses_every_column_in_order():
    # 4 words, 3 groups: word 1 -> group 0, words 2-3 -> group 1 (merged), word 4 -> group 2
    score = [[3, 0, 0], [0, 2, 0], [0, 2, 0], [0, 0, 3]]
    assert _monotone(score, 4, 3) == [0, 1, 1, 2]
    assert _monotone([[1]], 1, 2) is None  # fewer rows than columns cannot use every column
