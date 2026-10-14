"""Tests of the Phase 2 prototype. Synthetic cases use arbitrary phoneme symbols; Quran-derived data
is read through the phonemizer (needs data/quran/)."""

import random

import pytest

from tools.phonemizer.inventory import TOKEN_ID
from tools.prototype.acoustic_sim import simulate_frames
from tools.prototype.aligner import Params, StreamingAligner, align_tokens
from tools.prototype.decode import Token, greedy_decode
from tools.prototype.errors import KINDS, edit_distance, make_utterance, pick_range, render
from tools.prototype.phoneme_data import WordPhonemes, build_word_phonemes, collapse
from tools.quran_text import loader

needs_data = pytest.mark.skipif(not loader.data_available(), reason="data/quran/ missing")


def wp(*tokens: str, key=(1, 1, 1)) -> WordPhonemes:
    return WordPhonemes(key, tokens, (tokens,) * 4)


def toks(symbols: str, confidence: float = 0.9) -> list[Token]:
    out, frame = [], 0
    for s in symbols.split():
        out.append(Token(s, confidence, frame, frame + 3))
        frame += 4
    return out


WORDS = [wp("b", "a", "t", "i"), wp("k", "u", "m", "a"), wp("s", "a", "l", "u"), wp("n", "i", "d", "a")]


def test_collapse_merges_consecutive_duplicates():
    assert collapse(["b", "b", "a", "b"]) == ("b", "a", "b")


def test_edit_distance():
    assert edit_distance(("a", "b"), ("a", "b")) == 0
    assert edit_distance(("a", "b", "c"), ("a", "c")) == 1
    assert edit_distance((), ("a", "b")) == 2


def test_greedy_decode_collapses_repeats_and_drops_blanks():
    a, b = TOKEN_ID["a"], TOKEN_ID["b"]
    frames = [(0, 0.9), (a, 0.8), (a, 0.6), (0, 0.9), (a, 0.9), (b, 0.7), (b, 0.9)]
    tokens = greedy_decode(frames)
    assert [t.symbol for t in tokens] == ["a", "a", "b"]  # a blank separates the two a's
    assert tokens[0].confidence == pytest.approx(0.7)
    assert (tokens[0].start, tokens[0].end) == (1, 3)


def test_simulator_without_errors_decodes_back_to_the_input():
    rng = random.Random(1)
    tokens = ["b", "a", "t", "i", "k", "u"]
    frames = simulate_frames(rng, tokens, per=0.0, pauses_after={3})
    decoded = greedy_decode(frames)
    assert [t.symbol for t in decoded] == tokens
    assert all(t.confidence >= 0.7 for t in decoded)
    assert decoded[4].start - decoded[3].end >= 8  # the pause shows as a gap


def test_simulator_error_rate_is_close_to_the_target():
    rng = random.Random(2)
    tokens = ["b", "a", "t", "i"] * 300
    for per in (0.1, 0.2):
        decoded = tuple(t.symbol for t in greedy_decode(simulate_frames(rng, tokens, per=per)))
        measured = edit_distance(decoded, tuple(tokens)) / len(tokens)
        assert per * 0.7 < measured < per * 1.3, (per, measured)


def test_clean_speech_is_all_ok():
    result = align_tokens(toks("b a t i k u m a s a l u n i d a"), WORDS)
    assert result.statuses == {0: "ok", 1: "ok", 2: "ok", 3: "ok"}
    assert result.events == []
    assert result.span == (0, 3)


def test_reciter_may_start_late_and_stop_early():
    result = align_tokens(toks("k u m a s a l u"), WORDS)
    assert result.span == (1, 2) and result.events == []


def test_skipped_word():
    result = align_tokens(toks("b a t i s a l u n i d a"), WORDS)
    assert [(e.kind, e.index) for e in result.events] == [("skipped", 1)]
    assert result.statuses[1] == "skipped"


def test_repeated_word():
    result = align_tokens(toks("b a t i k u m a k u m a s a l u n i d a"), WORDS)
    assert [(e.kind, e.index) for e in result.events] == [("repeated", 1)]


def _ayahs(sizes: list[int]) -> list[WordPhonemes]:
    """Words of consecutive ayahs (1, 2, ...) with symbols that differ from word to word."""
    words = []
    for ayah, size in enumerate(sizes, 1):
        for w in range(1, size + 1):
            tag = f"{ayah}x{w}"
            words.append(wp(*(f"{tag}p{i}" for i in range(4)), key=(1, ayah, w)))
    return words


def test_a_whole_ayah_recited_again_and_again_is_not_reported():
    words = _ayahs([5, 6, 4])
    ayah2 = " ".join(" ".join(w.cont) for w in words[5:11])
    rest = " ".join(" ".join(w.cont) for w in words[11:])
    first = " ".join(" ".join(w.cont) for w in words[:5])
    for times in (2, 5):
        result = align_tokens(toks(" ".join([first] + [ayah2] * times + [rest])), words)
        assert result.events == []
        assert set(result.statuses.values()) == {"ok"}


def test_the_last_two_ayahs_recited_again_are_not_reported():
    words = _ayahs([4, 4, 4])
    text = lambda a, b: " ".join(" ".join(w.cont) for w in words[a:b])  # noqa: E731
    result = align_tokens(toks(" ".join([text(0, 8), text(4, 8), text(4, 8), text(8, 12)])), words)
    assert result.events == []


def test_repeating_part_of_an_ayah_is_still_reported():
    words = _ayahs([6, 6])
    text = lambda a, b: " ".join(" ".join(w.cont) for w in words[a:b])  # noqa: E731
    result = align_tokens(toks(" ".join([text(0, 4), text(2, 6), text(6, 12)])), words)
    assert [e.kind for e in result.events] == ["repeated"]


def test_wrong_word():
    result = align_tokens(toks("b a t i f o r g h s a l u n i d a"), WORDS)
    assert [(e.kind, e.index) for e in result.events] == [("wrong", 1)]


def test_added_word_between_words():
    result = align_tokens(toks("b a t i z y x w v k u m a s a l u n i d a"), WORDS)
    assert [(e.kind, e.index) for e in result.events] == [("added", 0)]


def test_one_noisy_token_in_a_long_word_is_not_an_error():
    long = [wp("b", "a", "t", "i", "k", "u"), wp("m", "a", "s", "a", "l", "u")]
    result = align_tokens(toks("b a t i z u m a s a l u"), long)
    assert result.events == []


def test_low_confidence_tokens_do_not_count_against_a_word():
    heard = toks("b a t i k u m a") + toks("s i l i", confidence=0.3) + toks("n i d a")
    assert align_tokens(heard, WORDS).events == []  # the doubtful word is not called wrong


def test_empty_input_gives_empty_result():
    assert align_tokens([], WORDS).events == []
    assert align_tokens(toks("a b"), []).statuses == {}


def test_streaming_aligner_reports_the_position():
    stream = StreamingAligner(WORDS, Params())
    frames = simulate_frames(random.Random(3), ["b", "a", "t", "i", "k", "u", "m", "a"], per=0.0)
    first = stream.push(frames[: len(frames) // 2])
    assert first.span is not None and first.span[1] <= 1
    assert stream.push(frames[len(frames) // 2 :]).span == (0, 1)


@needs_data
def test_word_phonemes_cover_the_quran_and_contexts_differ_after_a_stop():
    words = build_word_phonemes(loader.load_quran())
    assert len(words) == 77433 - 30  # all words except the surah-opening letters, which have no phonemes
    assert all(w.cont for w in words.values())
    # a word that ends in a vowel: the stopping form is shorter than the continuing form
    shorter = [w for w in words.values() if w.tokens_for(False, True) != w.tokens_for(False, False)]
    assert len(shorter) > 20000
    assert words[(1, 1, 2)].readings(False, False) == [words[(1, 1, 2)].tokens_for(False, False)]


@needs_data
def test_utterances_carry_truth_and_render_tokens():
    words = build_word_phonemes(loader.load_quran())
    rng = random.Random(4)
    expected = pick_range(rng, words, 3)
    for kind in KINDS:
        utt = make_utterance(rng, words, expected, (kind,), pause_prob=0.3)
        assert [t.kind for t in utt.truth] == [kind]
        tokens, pauses = render(utt, words)
        assert tokens and all(i < len(tokens) for i in pauses)
    clean = make_utterance(rng, words, expected, ())
    assert clean.truth == [] and [s.source for s in clean.spoken] == expected
