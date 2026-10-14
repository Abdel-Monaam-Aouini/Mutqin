# Mutqin: common tasks. Run `make` or `make help` for the list.
#
# Variables (override on the command line, e.g. `make live SURAH=112 AYAH=1-4`):
#   SURAH, AYAH   what you recite (default: Al-Fatihah)
#   WAV           a 16 kHz mono recording for `make check-wav`
#   SECONDS       length of `make record`
#   SHERPA_DIR    where `make sherpa` puts sherpa-onnx (default ~/sherpa-onnx)
#   LEVEL         strictness: relaxed | standard | strict (default standard)
#   NDK, ABI      for `make android`: the NDK folder (default: ANDROID_NDK_HOME or the first one in ~/Android/Sdk/ndk)
#                 and arm64-v8a | armeabi-v7a | x86_64
#   ADB           the adb tool for the android-* targets (default: adb on the PATH or ~/Android/Sdk/platform-tools)
#   ANDROID_JAVA_HOME  the JDK for Gradle (default: OpenJDK 21)
#   MODEL         the model manifest (default models/zipformer_p-quran/mutqin_model.txt)

SURAH      ?= 1
AYAH       ?= 1-7
WAV        ?= take.wav
SECONDS    ?= 60
NDK        ?= $(or $(ANDROID_NDK_HOME),$(firstword $(wildcard $(HOME)/Android/Sdk/ndk/*)))
ABI        ?= arm64-v8a
ADB        ?= $(or $(shell command -v adb 2>/dev/null),$(HOME)/Android/Sdk/platform-tools/adb)
ANDROID_JAVA_HOME ?= /usr/lib/jvm/java-21-openjdk-amd64
KTLINT     := android/.tools/ktlint
APP_ID     := app.mutqin
APK        := android/app/build/outputs/apk/debug/app-debug.apk
APP_FILES  := /sdcard/Android/data/$(APP_ID)/files
SHERPA_VER ?= v1.13.8
SHERPA_DIR ?= $(HOME)/sherpa-onnx
SHERPA_PKG := sherpa-onnx-$(SHERPA_VER)-linux-x64-shared-no-tts
SHERPA_SHA := d0f96c8b65c6cd0974fada22737e337de81bc8cd2abbec2e39caf358b1eec5fc
LEVEL      ?= standard
MODEL      ?= models/zipformer_p-quran/mutqin_model.txt
LIVE_CLI   := build-live/cli/mutqin-cli
VENV       ?= .venv
PY         := $(VENV)/bin/python
JOBS       ?= $(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

.DEFAULT_GOAL := help
.PHONY: help venv build test test-py format lint check data units sherpa build-live model-check \
        live check-wav record demo bench shared install android android-app android-run android-model android-replay android-log android-format android-lint android-check clean

help: ## show this list
	@awk 'BEGIN {FS = ":.*## "} /^[a-zA-Z_-]+:.*## / {printf "  \033[1m%-12s\033[0m %s\n", $$1, $$2}' $(MAKEFILE_LIST)
	@echo
	@echo "  e.g. make check | make live SURAH=112 AYAH=1-4 | make record SECONDS=90 WAV=take.wav"

# ---------------------------------------------------------------- development

venv: ## create .venv with the pinned dev tools (pytest, numpy, ruff, clang-format)
	@test -x $(PY) || (command -v uv >/dev/null 2>&1 && uv venv $(VENV) || python3 -m venv $(VENV))
	@command -v uv >/dev/null 2>&1 && uv pip install --python $(PY) -r tools/requirements-dev.txt \
		|| $(VENV)/bin/pip install -r tools/requirements-dev.txt

build: ## build the core, the CLI and the tests (warnings are errors)
	cmake -S . -B build -DMUTQIN_WERROR=ON
	cmake --build build -j $(JOBS)

test: build ## run the C++ tests
	ctest --test-dir build --output-on-failure --parallel 4

test-py: ## run the Python tests
	$(PY) -I -m pytest -q

format: ## format Python and C++ in place
	$(VENV)/bin/ruff format tools
	git ls-files '*.h' '*.cpp' | xargs $(VENV)/bin/clang-format -i

lint: ## check formatting and lint without changing anything
	$(VENV)/bin/ruff format --check tools
	$(VENV)/bin/ruff check tools
	git ls-files '*.h' '*.cpp' | xargs $(VENV)/bin/clang-format --dry-run --Werror

check: lint test test-py ## everything CI runs: lint, C++ tests, Python tests

bench: ## micro-benchmark of the audio front end (Release build)
	cmake -S . -B build-rel -DCMAKE_BUILD_TYPE=Release -DMUTQIN_BUILD_BENCH=ON
	cmake --build build-rel -j $(JOBS)
	./build-rel/mutqin_bench 120

shared: ## build the shared library libmutqin (C API only) into build-shared/
	cmake -S . -B build-shared -DCMAKE_BUILD_TYPE=Release -DMUTQIN_BUILD_SHARED=ON
	cmake --build build-shared -j $(JOBS)
	ctest --test-dir build-shared -R CHeader --output-on-failure

android: ## cross-compile libmutqin for Android with the NDK (NDK=, ABI=), API 21, into build-android-ABI/
	@test -f "$(NDK)/build/cmake/android.toolchain.cmake" || { echo "no Android NDK found: set NDK=/path/to/ndk"; exit 1; }
	cmake -S . -B build-android-$(ABI) -DCMAKE_TOOLCHAIN_FILE=$(NDK)/build/cmake/android.toolchain.cmake \
		-DANDROID_ABI=$(ABI) -DANDROID_PLATFORM=android-21 -DCMAKE_BUILD_TYPE=Release \
		-DMUTQIN_BUILD_SHARED=ON -DMUTQIN_BUILD_TESTS=OFF -DMUTQIN_CLI_MIC=OFF -DMUTQIN_WERROR=ON
	cmake --build build-android-$(ABI) -j $(JOBS) --target mutqin_shared
	@ls -l build-android-$(ABI)/core/libmutqin.so

install: shared ## install header, libraries and the CLI under PREFIX (default /usr/local)
	cmake --install build-shared --prefix $(or $(PREFIX),/usr/local)

android-app: ## build the Android debug app (android/); needs make data; fetches sherpa-onnx on first use
	android/scripts/fetch_sherpa.sh
	cd android && JAVA_HOME=$(ANDROID_JAVA_HOME) ./gradlew --no-daemon -q :app:assembleDebug
	@ls -l $(APK)

android-run: android-app ## install the debug app on the connected phone and start it (then: make android-model)
	$(ADB) install -r $(APK)
	$(ADB) shell am start -n $(APP_ID)/.MainActivity

android-model: model-check ## copy the model files into the app's folder on the connected phone (start the app once first)
	$(ADB) shell mkdir -p $(APP_FILES)/model
	for f in quran_phoneme_zipformer.int8.onnx tokens.txt expected_units.tsv; do $(ADB) push models/zipformer_p-quran/$$f $(APP_FILES)/model/; done
	@echo "model copied; open the app again"

android-replay: ## replay a WAV through the app on the phone instead of the microphone (WAV=, SURAH=, AYAH=A-B, LEVEL=)
	$(ADB) push $(WAV) $(APP_FILES)/replay.wav
	$(ADB) logcat -c
	$(ADB) shell am start -n $(APP_ID)/.MainActivity --es wav replay.wav --ei surah $(SURAH) \
		--ei from $(word 1,$(subst -, ,$(AYAH))) --ei to $(or $(word 2,$(subst -, ,$(AYAH))),$(word 1,$(subst -, ,$(AYAH)))) \
		--ei level $(if $(filter relaxed,$(LEVEL)),0,$(if $(filter strict,$(LEVEL)),2,1))
	@echo "waiting for the result (Ctrl-C to stop watching)..."; \
	$(ADB) logcat -s Mutqin:I | sed -n '/REPLAY done/q;p'

android-format: ## format the Kotlin code of the app with ktlint (fetches ktlint on first use)
	android/scripts/fetch_ktlint.sh
	cd android && ../$(KTLINT) -F

android-lint: ## Kotlin format check (ktlint) and Android lint, as in CI; fetches sherpa-onnx and ktlint on first use
	android/scripts/fetch_ktlint.sh
	cd android && ../$(KTLINT)
	android/scripts/fetch_sherpa.sh
	cd android && JAVA_HOME=$(ANDROID_JAVA_HOME) ./gradlew --no-daemon -q :app:lintDebug

android-check: android-lint android-app ## everything CI does for the app: format, lint, debug build

android-log: ## show the app's log lines from the connected phone
	$(ADB) logcat -s Mutqin:V

clean: ## remove build directories
	rm -rf build build-rel build-live build-shared build-android-* android/app/build android/build

# ---------------------------------------------------------------- data and model

data: ## download the verified Quran text from Tanzil (you accept its terms)
	python3 tools/quran_text/download.py

sherpa: ## download sherpa-onnx (Linux x86_64) into SHERPA_DIR and verify its checksum
	@mkdir -p $(SHERPA_DIR)
	@if [ ! -d $(SHERPA_DIR)/$(SHERPA_PKG) ]; then \
		cd $(SHERPA_DIR) && curl -fLO https://github.com/k2-fsa/sherpa-onnx/releases/download/$(SHERPA_VER)/$(SHERPA_PKG).tar.bz2 \
		&& echo "$(SHERPA_SHA)  $(SHERPA_PKG).tar.bz2" | sha256sum -c - \
		&& tar xjf $(SHERPA_PKG).tar.bz2 && rm $(SHERPA_PKG).tar.bz2; \
	fi
	@echo "sherpa-onnx is in $(SHERPA_DIR)/$(SHERPA_PKG)"

build-live: sherpa ## Release build with the sherpa-onnx backend and the microphone (-> build-live/)
	cmake -S . -B build-live -DCMAKE_BUILD_TYPE=Release -DMUTQIN_WITH_SHERPA=ON \
		-DSHERPA_ONNX_DIR=$(SHERPA_DIR)/$(SHERPA_PKG)
	cmake --build build-live -j $(JOBS)

units: ## build the expected-units table from the model files in models/zipformer_p-quran/
	$(PY) -I tools/zipformer/expected_units.py

model-check: ## check that the model manifest and files are in place
	@for f in $(MODEL) models/zipformer_p-quran/tokens.txt models/zipformer_p-quran/expected_units.tsv \
		models/zipformer_p-quran/quran_phoneme_zipformer.int8.onnx; do \
		test -s $$f || { echo "missing: $$f"; echo "  see README: 'With the real model' (download, make units)"; exit 1; }; \
	done; echo "model files OK"

# ---------------------------------------------------------------- try it

demo: build ## expected phonemes of the range, and a replay with a simulated skipped word (no model needed)
	./build/cli/mutqin-cli --surah $(SURAH) --ayah $(AYAH) | head -20
	$(PY) -I tools/prototype/make_posteriors.py /tmp/mutqin_demo.f32 --surah $(SURAH) --ayahs $(AYAH) --scenario skip
	./build/cli/mutqin-cli --surah $(SURAH) --ayah $(AYAH) --posteriors /tmp/mutqin_demo.f32

$(LIVE_CLI):
	$(MAKE) build-live

live: model-check $(LIVE_CLI) ## recite into the microphone and see events as they appear (Ctrl-C to stop)
	$(LIVE_CLI) --surah $(SURAH) --ayah $(AYAH) --model $(MODEL) --strictness $(LEVEL) --mic

record: ## record the microphone to WAV for SECONDS seconds (press q to stop early)
	ffmpeg -f pulse -i default -ac 1 -ar 16000 -t $(SECONDS) $(WAV)

check-wav: model-check $(LIVE_CLI) ## check a recording against the ayah range, with event times
	$(LIVE_CLI) --surah $(SURAH) --ayah $(AYAH) --model $(MODEL) --strictness $(LEVEL) --wav $(WAV) --timeline
