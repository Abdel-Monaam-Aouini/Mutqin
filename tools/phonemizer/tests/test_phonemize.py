"""Phonemizer tests. Synthetic cases use arbitrary code points that do not spell Quran words;
Quran text is only read from data/quran/."""

import pytest

from tools.phonemizer import uthmani as u
from tools.phonemizer.inventory import BY_SYMBOL, Variant
from tools.phonemizer.phonemize import phonemize_ayah, phonemize_range, select_reading, word_key
from tools.quran_text import loader
from tools.quran_text.model import Ayah, Word

needs_data = pytest.mark.skipif(not loader.data_available(), reason="data/quran/ missing")

BA, TA, NOON, LAM, WAW, YEH, ALEF, MAKSURA = ("ب", "ت", "ن", "ل", "و", "ي", "ا", "ى")
FATHA, DAMMA, KASRA, SHADDA, SUKUN = "َ", "ُ", "ِ", "ّ", "ْ"
FATHATAN, SUPERSCRIPT_ALEF, ROUNDED_ZERO = "ً", "ٰ", "۟"


@pytest.fixture(scope="module")
def full():
    """The whole Quran phonemized once as a single range."""
    quran = loader.load_quran()
    return phonemize_range([a for s in quran.surahs for a in s.ayahs])


WASLA = "\u0671"


def run(*words: str):
    ayah = Ayah(1, 1, tuple(Word(1, 1, i, w) for i, w in enumerate(words, 1)), "")
    return phonemize_ayah(ayah)


def syms(result):
    return [(p.ph, p.dur) for p in result.phonemes]


def test_consonant_with_short_vowels():
    assert syms(run(BA + FATHA + TA + KASRA + LAM + DAMMA)) == [
        ("b", ()),
        ("a", (1,)),
        ("t", ()),
        ("i", (1,)),
        ("l", ()),
        ("u", (1,)),
    ]


def test_sukun_emits_no_vowel():
    assert [p for p, _ in syms(run(TA + SUKUN))] == ["t"]


def test_shaddah_doubles_the_consonant():
    assert [p for p, _ in syms(run(TA + SHADDA + FATHA))] == ["t", "t", "a"]


def test_alef_after_fatha_is_long_a():
    r = run(BA + FATHA + ALEF)
    assert syms(r) == [("b", ()), ("a", (2,))]
    assert r.phonemes[1].char_span == (1, 3)  # fatha .. alef


def test_waw_and_yeh_madd_letters():
    assert syms(run(BA + DAMMA + WAW))[-1] == ("u", (2,))
    assert syms(run(BA + KASRA + YEH))[-1] == ("i", (2,))


def test_waw_with_sukun_is_a_consonant():
    assert [p for p, _ in syms(run(BA + FATHA + WAW + SUKUN))] == ["b", "a", "w"]


def test_tanween_is_vowel_plus_n():
    assert [p for p, _ in syms(run(TA + FATHATAN))] == ["t", "a", "n"]


def test_superscript_alef_lengthens_the_fatha():
    assert syms(run(BA + FATHA + SUPERSCRIPT_ALEF)) == [("b", ()), ("a", (2,))]


def test_silent_alef_marked_with_rounded_zero():
    assert [p for p, _ in syms(run(BA + DAMMA + WAW + ALEF + ROUNDED_ZERO))] == ["b", "u"]


def test_alef_maksura_after_fatha_is_long_a():
    assert syms(run(BA + FATHA + MAKSURA))[-1] == ("a", (2,))


def test_yeh_shaped_letter_with_own_vowel_is_consonant_y():
    assert [p for p, _ in syms(run(BA + KASRA + MAKSURA + FATHA))] == ["b", "i", "y", "a"]


def test_unexpected_pattern_is_a_diagnostic_not_a_guess():
    r = run(BA + SUKUN + ALEF)  # alef after a consonant with no fatha
    assert [d.kind for d in r.diagnostics] == ["alef_without_fatha"]


def test_vowel_never_lengthens_across_words():
    r = run(BA + FATHA, ALEF + BA + FATHA)
    assert [d.kind for d in r.diagnostics] == ["alef_without_fatha"]


def test_article_before_assimilated_letter(tmp_path=None):
    # wasla + bare lam + (ta with shaddah): lam is silent, the next consonant is doubled and tagged
    r = run(WASLA + LAM + TA + SHADDA + FATHA)
    cont = [(p.ph, p.rules) for p in r.phonemes if p.variant in (Variant.BOTH, Variant.CONTINUE_ONLY)]
    assert [ph for ph, _ in cont] == ["t", "t", "a"]
    assert cont[0][1] == ("lam_shamsiyyah",) and cont[1][1] == ("lam_shamsiyyah",)
    start = [p.ph for p in r.phonemes if p.variant is Variant.START_ONLY]
    assert start == ["hz", "a"]  # hamzat al-wasl of the article, only when starting


def test_article_before_clear_letter():
    r = run(WASLA + LAM + SUKUN + BA + FATHA)
    cont = [(p.ph, p.rules) for p in r.phonemes if p.variant in (Variant.BOTH, Variant.CONTINUE_ONLY)]
    assert [ph for ph, _ in cont if ph != "hz"] == ["l", "b", "a"]
    assert cont[0][1] == ("lam_qamariyyah",)


def test_wasla_of_a_non_article_word_is_reported_not_guessed():
    r = run(WASLA + BA + SUKUN + TA + FATHA)
    assert [p.ph for p in r.phonemes] == ["b", "t", "a"]
    assert [d.kind for d in r.diagnostics] == ["wasla_start_vowel_unknown"]


def test_bare_lam_outside_the_article_is_tagged_neutrally():
    r = run(BA + FATHA + LAM + TA + SHADDA + FATHA)
    assert [p.ph for p in r.phonemes] == ["b", "a", "t", "t", "a"]
    assert r.phonemes[2].rules == ("lam_assimilated",)


# Locations the provisional tables leave to task 1.9 (special cases, keyed by word id).
EXPECTED_SPECIAL = {
    "long_a_without_fatha": {"11:41:6"},
    "bare_yeh_after_a": {"68:6:1"},
    "bare_waw_after_a": {"83:3:3"},
    "noon_sukun_before_non_izhar_letter": {"75:27:2"},  # sakt location listed in spec 3.9
}


@needs_data
def test_whole_quran_phonemizes_with_only_known_special_cases():
    quran = loader.load_quran()
    found: dict[str, set[str]] = {}
    disconnected = set()
    wasla_unknown = 0
    for surah in quran.surahs:
        for ayah in surah.ayahs:
            result = phonemize_ayah(ayah)
            words = {w.index: w for w in ayah.words}
            for p in result.phonemes:  # ExpectedPhoneme validated its own fields on creation
                assert p.ph in BY_SYMBOL
                assert 0 <= p.char_span[0] < p.char_span[1] <= len(words[p.word].text)
                if p.ph in "aiu":
                    assert p.dur in ((1,), (2,), (4, 5), (6,), (2, 4, 6))
                elif p.rules and any(r == "madd_leen" for r in p.rules):
                    assert p.dur in ((), (2, 4, 6))  # leen letter at a stop
                else:  # consonants have no duration, except ghunnah (2 counts) from noon / meem rules
                    assert p.dur == () or (
                        p.dur == (2,)
                        and any(
                            r.startswith(("ikhfa", "iqlab", "idgham_ghunnah", "idgham_mithlayn", "ghunnah"))
                            for r in p.rules
                        )
                    )
            for d in result.diagnostics:
                if d.kind == "disconnected_letters":
                    disconnected.add(d.word_id)
                elif d.kind == "wasla_start_vowel_unknown":
                    wasla_unknown += 1  # starting vowels of non-article words: open question
                else:
                    found.setdefault(d.kind, set()).add(d.word_id)
    assert found == EXPECTED_SPECIAL
    assert wasla_unknown == 1488
    assert len(disconnected) == 30  # surah-opening letters: all at word 1 of ayah 1, plus 42:2:1
    assert all(w.endswith(":1:1") or w == "42:2:1" for w in disconnected)


@needs_data
def test_article_lam_sun_and_moon_letters_come_out_consistent():
    """Assimilated (bare lam) and clear (lam with sukun) next-letters must be disjoint, apart from
    teh, where the clear cases are verbs, not the article. 14 letters assimilate."""
    quran = loader.load_quran()
    assimilated, clear = set(), set()
    for surah in quran.surahs:
        for ayah in surah.ayahs:
            for word in ayah.words:
                units = u.parse_word(word.text)
                for i, unit in enumerate(units):
                    if (
                        ord(unit.char) == 0x0644
                        and i
                        and ord(units[i - 1].char) == 0x0671
                        and i + 1 < len(units)
                    ):
                        if not unit.marks:
                            assimilated.add(units[i + 1].char)
                        elif unit.has(u.SUKUN):
                            clear.add(units[i + 1].char)
    assert len(assimilated) == 14
    assert assimilated & clear == {"\u062a"}


# -- noon sakinah and tanween (spec 3.1) --------------------------------------------------------
HEH, KAF, YEH_, REH, MEEM = "\u0647", "\u0643", "\u064a", "\u0631", "\u0645"


def two_words(first: str, second: str):
    return run(first, second)


def vs(result, kinds=None):
    """(ph, variant value, dur, rules) of consonant items, optionally restricted by symbol."""
    return [
        (p.ph, p.variant.value, p.dur, p.rules)
        for p in result.phonemes
        if p.ph not in "aiu" and (kinds is None or p.ph in kinds)
    ]


def test_izhar_halqi_keeps_the_noon():
    r = two_words(BA + FATHA + NOON + SUKUN, HEH + FATHA)
    assert vs(r, "n") == [("n", "both", (), ("izhar_halqi",))]


def test_ikhfa_hides_the_noon_with_ghunnah_only_when_continuing():
    r = two_words(BA + FATHA + NOON, KAF + FATHA)
    assert vs(r, "n") == [("n", "continue_only", (2,), ("ikhfa",)), ("n", "waqf_only", (), ())]


def test_iqlab_turns_the_noon_into_meem():
    r = two_words(BA + FATHA + NOON, BA + FATHA)
    assert vs(r, "nm") == [("m", "continue_only", (2,), ("iqlab",)), ("n", "waqf_only", (), ())]


def test_idgham_without_ghunnah_doubles_the_next_letter():
    r = two_words(BA + FATHA + NOON, LAM + FATHA)
    assert vs(r, "nl") == [
        ("n", "waqf_only", (), ()),
        ("l", "both", (), ()),
        ("l", "after_continue", (), ("idgham_no_ghunnah",)),
    ]


def test_idgham_with_ghunnah_gives_the_doubled_letter_two_counts():
    r = two_words(BA + FATHA + NOON, YEH_ + FATHA)
    assert vs(r, "ny") == [
        ("n", "waqf_only", (), ()),
        ("y", "both", (), ()),
        ("y", "after_continue", (2,), ("idgham_ghunnah",)),
    ]


def test_idgham_inside_a_word_is_izhar_mutlaq():
    r = run(BA + FATHA + NOON + SUKUN + YEH_ + FATHA)
    assert vs(r, "n") == [("n", "both", (), ("izhar_mutlaq",))]


def test_tanween_before_iqlab_and_idgham():
    iq = two_words(BA + DAMMA + TA + "\u064c", BA + FATHA)  # dammatan then beh
    assert vs(iq, "nm") == [("m", "continue_only", (2,), ("iqlab",))]
    idg = two_words(BA + FATHA + TA + FATHATAN, MEEM + FATHA)  # fathatan then meem
    assert vs(idg, "nm") == [("m", "both", (), ()), ("m", "after_continue", (2,), ("idgham_ghunnah",))]


def test_tanween_before_izhar_keeps_a_continue_only_noon():
    r = two_words(BA + FATHA + TA + FATHATAN, HEH + FATHA)
    assert vs(r, "n") == [("n", "continue_only", (), ("izhar_halqi",))]


def test_next_letter_is_found_past_a_silent_hamzat_wasl():
    r = two_words(BA + FATHA + NOON, WASLA + LAM + SUKUN + KAF + FATHA)
    assert any("idgham_no_ghunnah" in x[3] for x in vs(r, "l"))


def test_noon_at_an_ayah_end_only_sees_the_next_ayah_in_a_range():
    a1 = Ayah(1, 1, (Word(1, 1, 1, BA + FATHA + NOON),), "")
    a2 = Ayah(1, 2, (Word(1, 2, 1, BA + FATHA),), "")
    alone = phonemize_ayah(a1)
    chained = phonemize_range([a1, a2])
    assert vs(alone, "nm") == [("n", "both", (), ())]
    assert ("m", "continue_only", (2,), ("iqlab",)) in vs(chained, "nm")
    assert all(p.ayah in (1, 2) and p.surah == 1 for p in chained.phonemes)


def test_noon_with_a_vowel_or_shaddah_is_not_sakin():
    r = two_words(BA + FATHA + NOON + FATHA, KAF + FATHA)
    assert vs(r, "n") == [("n", "both", (), ())]
    r = two_words(BA + FATHA + NOON + SHADDA + FATHA, KAF + FATHA)
    assert vs(r, "n") == [("n", "both", (), ()), ("n", "both", (2,), ("ghunnah",))]  # mushaddad


@needs_data
def test_noon_rules_agree_with_the_encoding_of_the_text(full):
    """The mushaf marks izhar with a sukun on the noon, hiding / merging with a bare noon and
    iqlab with a meem sign. The spec's next-letter rules must predict exactly that."""
    iqlab_sign = {0x06E2, 0x06ED}
    seen_sukun_rules, seen_bare_rules, iqlab_without_sign = set(), set(), 0
    for item in full.items:
        rule = next((r for r in item.rules if r.startswith(("izhar", "ikhfa", "iqlab", "idgham"))), None)
        sakin = not item.host_marks & {u.FATHA, u.DAMMA, u.KASRA, u.SHADDA}
        if item.role != "cons" or item.letter != 0x0646 or rule is None or not sakin:
            continue
        (seen_sukun_rules if u.SUKUN in item.host_marks else seen_bare_rules).add(rule)
        if rule == "iqlab" and not item.signs & iqlab_sign:
            iqlab_without_sign += 1
    assert seen_sukun_rules == {"izhar_halqi", "izhar_mutlaq"}
    assert seen_bare_rules == {"ikhfa", "iqlab"}
    assert iqlab_without_sign == 0


# -- meem sakinah, mushaddad ghunnah, qalqalah (spec 3.2 - 3.4) ----------------------------------
DAL, QAF = "\u062f", "\u0642"


def test_meem_before_beh_is_ikhfa_shafawi():
    r = two_words(BA + FATHA + MEEM, BA + FATHA)
    assert vs(r, "m") == [("m", "continue_only", (2,), ("ikhfa_shafawi",)), ("m", "waqf_only", (), ())]


def test_meem_before_meem_is_idgham_mithlayn_saghir():
    r = two_words(BA + FATHA + MEEM, MEEM + FATHA)
    assert vs(r, "m") == [
        ("m", "waqf_only", (), ()),
        ("m", "both", (), ()),
        ("m", "after_continue", (2,), ("idgham_mithlayn_saghir",)),
    ]


def test_meem_before_other_letters_is_izhar_shafawi():
    r = two_words(BA + FATHA + MEEM + SUKUN, KAF + FATHA)
    assert vs(r, "m") == [("m", "both", (), ("izhar_shafawi",))]


def test_mushaddad_noon_and_meem_get_two_counts_of_ghunnah():
    for letter, sym in ((NOON, "n"), (MEEM, "m")):
        r = run(BA + FATHA + letter + SHADDA + FATHA)
        assert vs(r, sym) == [(sym, "both", (), ()), (sym, "both", (2,), ("ghunnah",))]


def test_qalqalah_inside_a_word_is_sughra():
    r = run(BA + FATHA + QAF + SUKUN + TA + FATHA)
    assert vs(r, "q") == [("q", "both", (), ("qalqalah_sughra",))]


def test_qalqalah_at_a_word_end_is_sughra_when_continuing_and_kubra_at_waqf():
    r = two_words(BA + FATHA + DAL + SUKUN, KAF + FATHA)
    assert vs(r, "d") == [
        ("d", "continue_only", (), ("qalqalah_sughra",)),
        ("d", "waqf_only", (), ("qalqalah_kubra",)),
    ]


def test_qalqalah_needs_a_sukun_and_a_qalqalah_letter():
    assert vs(run(BA + FATHA + DAL + FATHA + KAF + FATHA), "d") == [("d", "both", (), ())]
    assert vs(run(BA + FATHA + KAF + SUKUN + TA + FATHA), "k") == [("k", "both", (), ())]


@needs_data
def test_meem_rules_agree_with_the_encoding_of_the_text(full):
    by_rule: dict[str, set[str]] = {}
    for item in full.items:
        if item.role != "cons" or item.letter != 0x0645:
            continue
        rule = next((r for r in item.rules if r in ("izhar_shafawi", "ikhfa_shafawi")), None)
        if rule:
            by_rule.setdefault(rule, set()).add("sukun" if u.SUKUN in item.host_marks else "bare")
    assert by_rule["izhar_shafawi"] == {"sukun"}
    assert by_rule["ikhfa_shafawi"] <= {"sukun", "bare"}


def test_nothing_continues_across_a_surah_boundary():
    a1 = Ayah(1, 1, (Word(1, 1, 1, BA + FATHA + NOON),), "")
    a2 = Ayah(2, 1, (Word(2, 1, 1, BA + FATHA),), "")
    r = phonemize_range([a1, a2])
    assert vs(r, "nm") == [("n", "both", (), ())]


# -- madd (spec 3.7) ----------------------------------------------------------------------------
HAMZA_LETTER = "\u0621"
MADDAH = "\u0653"


def madd(result, ph="a"):
    """(variant, dur, madd rules) of the long vowels in a result."""
    return [
        (p.variant.value, p.dur, tuple(r for r in p.rules if r.startswith("madd")))
        for p in result.phonemes
        if p.ph == ph and p.dur not in ((), (1,))
    ]


def test_tabii_is_two_counts():
    assert madd(run(BA + FATHA + ALEF + TA + FATHA + KAF + FATHA)) == [("both", (2,), ("madd_tabii",))]


def test_muttasil_hamza_in_the_same_word():
    r = run(BA + FATHA + ALEF + HAMZA_LETTER + FATHA)
    assert madd(r) == [("both", (4, 5), ("madd_muttasil",))]


def test_munfasil_hamza_in_the_next_word_only_when_continuing():
    r = two_words(BA + FATHA + ALEF, HAMZA_LETTER + FATHA)
    assert madd(r) == [("continue_only", (4, 5), ("madd_munfasil",)), ("waqf_only", (2,), ("madd_tabii",))]


def test_no_munfasil_before_a_hamza_that_follows_a_silent_wasla():
    r = two_words(BA + FATHA + ALEF, WASLA + HAMZA_LETTER + SUKUN + TA + FATHA)
    assert madd(r) == [("both", (2,), ("madd_tabii",))]


def test_lazim_before_a_permanent_sukun_or_shaddah():
    assert madd(run(BA + FATHA + ALEF + TA + SUKUN + KAF + FATHA)) == [("both", (6,), ("madd_lazim",))]
    assert madd(run(BA + FATHA + ALEF + TA + SHADDA + FATHA)) == [("both", (6,), ("madd_lazim",))]


def test_aarid_lissukun_is_a_stop_only_reading_before_the_last_letter():
    r = run(BA + FATHA + ALEF + TA + KASRA)
    assert madd(r) == [
        ("continue_only", (2,), ("madd_tabii",)),
        ("waqf_only", (2, 4, 6), ("madd_arid_lissukun",)),
    ]


def test_badal_hamza_before_the_long_vowel():
    r = run(HAMZA_LETTER + FATHA + ALEF + TA + FATHA + KAF + FATHA)
    assert madd(r) == [("both", (2,), ("madd_badal",))]


def test_leen_before_the_last_letter_only_at_a_stop():
    r = run(BA + FATHA + WAW + SUKUN + TA + KASRA)
    w = [(p.variant.value, p.dur, p.rules) for p in r.phonemes if p.ph == "w"]
    assert w == [("continue_only", (), ()), ("waqf_only", (2, 4, 6), ("madd_leen",))]


@needs_data
def test_madd_rules_agree_with_the_written_maddah_sign(full):
    """The text writes a maddah exactly where the spec has a madd longer than 2 counts
    (muttasil, munfasil, lazim, silah kubra), apart from three known anomalies."""
    long_rules = {"madd_muttasil", "madd_munfasil", "madd_lazim", "madd_silah_kubra"}
    anomalies, signed_but_short = [], 0
    for item in full.items:
        if item.role != "vowel" or not item.long_by or item.variant.value == "waqf_only":
            continue
        rule = next((r for r in item.rules if r.startswith("madd")), None)
        if rule in long_rules and not item.maddah:
            anomalies.append((rule, f"{item.surah}:{item.ayah}:{item.word}"))
        if rule not in long_rules and item.maddah:
            signed_but_short += 1
    assert signed_but_short == 0
    # text without a maddah where the rule predicts a long madd: 2:72:4 (no sign written) and
    # 41:44:9 (tas-hil location, spec 3.9)
    assert sorted(anomalies) == [("madd_lazim", "41:44:9"), ("madd_muttasil", "2:72:4")]


# -- waqf (spec section 5) ------------------------------------------------------------------------
TEH_MARBUTA, ALEF_MAKSURA_ = "\u0629", "\u0649"
DAMMATAN = "\u064c"


def reading(result, stop=False):
    items = select_reading(result.items, all_stops=stop)
    return [(i.ph, i.dur) for i in items]


def test_final_vowel_is_dropped_at_a_stop():
    r = run(BA + FATHA + TA + DAMMA)
    assert reading(r) == [("b", ()), ("a", (1,)), ("t", ()), ("u", (1,))]
    assert reading(r, stop=True) == [("b", ()), ("a", (1,)), ("t", ())]


def test_tanween_damma_and_kasra_vanish_at_a_stop():
    r = run(BA + FATHA + TA + DAMMATAN)
    assert reading(r) == [("b", ()), ("a", (1,)), ("t", ()), ("u", (1,)), ("n", ())]
    assert reading(r, stop=True) == [("b", ()), ("a", (1,)), ("t", ())]


def test_tanween_fatha_with_alef_becomes_a_long_a_at_a_stop():
    r = run(BA + FATHA + TA + FATHATAN + ALEF)
    assert reading(r, stop=True) == [("b", ()), ("a", (1,)), ("t", ()), ("a", (2,))]
    waqf = [p for p in r.phonemes if p.variant.value == "waqf_only"]
    assert [p.rules for p in waqf] == [("madd_iwad",)]


def test_tanween_fatha_on_teh_marbuta_has_no_long_a_at_a_stop():
    r = run(BA + FATHA + TEH_MARBUTA + FATHATAN)
    assert reading(r, stop=True) == [("b", ()), ("a", (1,)), ("h", ())]
    assert reading(r)[-3:] == [("t", ()), ("a", (1,)), ("n", ())]


def test_teh_marbuta_is_heh_at_a_stop():
    r = run(BA + FATHA + TEH_MARBUTA + KASRA)
    assert [ph for ph, _ in reading(r)] == ["b", "a", "t", "i"]
    assert [ph for ph, _ in reading(r, stop=True)] == ["b", "a", "h"]


def test_final_qalqalah_letter_is_kubra_at_a_stop_only():
    r = run(KAF + FATHA + DAL + DAMMA)
    assert [(p.ph, p.variant.value, p.rules) for p in r.phonemes if p.ph == "d"] == [
        ("d", "continue_only", ()),
        ("d", "waqf_only", ("qalqalah_kubra",)),
    ]


def test_a_long_vowel_at_the_end_of_a_word_is_kept_at_a_stop():
    r = run(BA + FATHA + ALEF)
    assert reading(r, stop=True) == [("b", ()), ("a", (2,))]


def test_continuing_reading_has_no_stop_only_items_and_stops_pick_them_up():
    r = two_words(BA + FATHA + NOON, YEH_ + FATHA)  # idgham with ghunnah between the words
    cont = select_reading(r.items)
    assert [i.variant.value for i in cont if i.ph == "y"] == ["both", "after_continue"]
    stopped = select_reading(r.items, stops={(1, 1, 1)})
    assert [i.ph for i in stopped if i.ph in "ny"] == ["n", "y"]  # noon kept, single y


@needs_data
def test_stopping_after_every_word_never_leaves_a_short_vowel_at_a_word_end(full):
    ends = {}
    for item in select_reading(full.items, all_stops=True):
        ends[word_key(item)] = item
    assert len(ends) > 77000
    assert not [k for k, i in ends.items() if i.role == "vowel" and i.dur == (1,)]
    assert not any(
        i.variant.value in ("continue_only", "after_continue")
        for i in select_reading(full.items, all_stops=True)
    )


@needs_data
def test_a_continuing_reading_has_no_stop_only_items(full):
    kinds = {i.variant.value for i in select_reading(full.items)}
    assert "waqf_only" not in kinds


def test_idgham_into_a_letter_the_text_already_doubles_keeps_a_plain_form_after_a_stop():
    r = two_words(BA + FATHA + NOON, LAM + SHADDA + FATHA)
    assert vs(r, "nl") == [
        ("n", "waqf_only", (), ()),
        ("l", "both", (), ()),
        ("l", "start_only", (), ()),
        ("l", "after_continue", (), ("idgham_no_ghunnah",)),
    ]
    cont = [(i.ph, i.rules) for i in select_reading(r.items) if i.ph in "nl"]
    assert cont == [("l", []), ("l", ["idgham_no_ghunnah"])]
    stopped = [(i.ph, i.rules) for i in select_reading(r.items, stops={(1, 1, 1)}) if i.ph in "nl"]
    assert stopped == [("n", []), ("l", []), ("l", [])]


def test_vowel_before_an_idgham_noon_is_kept_when_stopping_on_that_word():
    r = two_words(BA + FATHA + NOON, YEH_ + FATHA)
    stopped = [(i.ph, i.dur) for i in select_reading(r.items, stops={(1, 1, 1)}) if i.ph in "bna"][:3]
    assert stopped == [("b", ()), ("a", (1,)), ("n", ())]
    cont = [i.ph for i in select_reading(r.items) if i.ph in "bna"][:3]
    assert cont == ["b", "a", "a"]  # reading on: the noon disappears into the next letter
