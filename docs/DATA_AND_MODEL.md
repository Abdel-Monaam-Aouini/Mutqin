# Data and Models

Note: names below are starting points to verify. Check every license before use,
especially for commercial release. Record decisions in `docs/LICENSES.md`.

## 1. Quran text

- **Tanzil.net**: Uthmani text (primary) and simple clean text (for search / matching).
- **King Fahd Complex (QPC) fonts / text**: for rendering the Madinah mushaf style in the UI.
- Store originals unmodified in `data/quran/` with SHA-256 checksums.

## 2. Tajweed annotations (for validating the rule engine)

- Open tajweed annotation datasets on GitHub (search "quran tajweed annotations"):
  rule spans per ayah in JSON.
- Quran.com API tajweed-colored text.
- Quranic Arabic Corpus for word segmentation and morphology.

## 3. Audio data

| Source | Use | Limitation |
|--------|-----|------------|
| EveryAyah.com reciters | correct recitation, per-ayah audio | professionals only, almost no mistakes |
| Tarteel AI datasets (Hugging Face) | crowd recitations, varied voices | check license and labels |
| Own collection (Phase 4+) | learners with real mistakes | needs consent + expert labeling |

### Getting error data

1. **Synthetic text-side errors** (Phase 2): take a correct recording, but align it against
   a *modified* expected text (word swapped/skipped/added). The detector must catch the
   mismatch. Cheap and unlimited.
2. **Audio-side edits**: cut, repeat, or splice words in correct recordings using alignments.
3. **Real learners** (Phase 4–5): consent form, record in the app (opt-in), label with
   teachers using a simple labeling tool (`tools/data/label_tool`). Target: 50+ hours
   with labeled mistakes, many voices (men, women, children, accents).

Keep test sets separated by **speaker**, never by ayah only, to avoid leaking voices.

## 4. Models

### Phase 2–3: word level

- Start with an existing Quran-fine-tuned **CTC** model (wav2vec2 / XLS-R, char-level), or
  fine-tune a small streaming Conformer/Zipformer with icefall/k2 for sherpa-onnx.
- Avoid relying on Whisper alone: its language model tends to output the correct
  memorized text even when the user made a mistake.

### Phase 4–5: phoneme level

- Fine-tune on the phoneme inventory from `TAJWEED_SPEC.md`; labels come from running the
  rule engine on the reference text of each recording.
- Streaming Zipformer-CTC (small, ~20–70M params) is the target for mobile.
- Separate light classifiers can be added later for ghunnah and qalqalah if CTC posteriors
  are not enough.

### Export

- ONNX export → int8 dynamic quantization → test accuracy drop (< 1% absolute allowed).
- Run with sherpa-onnx / ONNX Runtime inside the C++ core (desktop first; Android and iOS
  later in Phase 3b).

## 5. Evaluation

Scripts in `tools/eval/`. Report for every model version:

- **False alarm rate** on correct recitations (per word, per phoneme) — most important.
- Precision / recall per error type (word skipped, wrong, repeated; harakah; each tajweed rule).
- Latency and real-time factor on a reference desktop CPU (mid-range Android phone and
  iPhone added in Phase 3b).
- Results broken down by speaker group (men / women / children) to catch bias.

Keep results in `docs/EVAL_RESULTS.md`, one row per model version.
