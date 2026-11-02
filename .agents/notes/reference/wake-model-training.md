# Reference: Training a Custom Wake Word Model

This document describes the training pipeline that currently exists in the repository and the inputs needed to run it. It does not claim that a newly trained model is production-ready or that the pipeline works from a clean checkout without local setup.

## What the tools produce

`tools/wake-train.py` trains a compact acoustic model, exports it in the factory Rice/soil BINN container format, and evaluates the exported model against the available development clips and recording sessions. `tools/native-model-am.py`, `tools/native-model-forward.py`, and `tools/native-model-pack.py` parse, inspect, and check the model container and its reference inference path.

The current deployed candidate uses two models: a level-1 model and a level-2 verifier. Their repository copies are under [`models/wake-doubao-v5/`](../../../models/wake-doubao-v5/). The model files are artifacts, not training data or a complete reproducibility package. Their measured recall is limited; use the evaluation reports before selecting them for another device.

## Inputs and privacy

The training script currently reads these paths relative to the repository root:

| Input | Expected location | Purpose |
|---|---|---|
| Generated TTS corpus and manifests | `temp/analysis/tts-corpus/` | Synthetic positive and negative examples |
| Factory level-1/level-2 model templates | `temp/analysis/models/mdata_wakeup_mobile/` and `mdata_wakeup_mobile_clone/` | Network configuration, transition model, and decoding graph |
| Device recordings and derived AEC audio | `private/inputs/recordings/wake-corpus/` | Real speech, quiet background, and music evaluation |
| Optional human labels | `private/inputs/recordings/wake-corpus/review/labels.csv` | Overrides ASR-derived labels for sessions represented in the file |

Do not publish personal recordings, review labels, device readbacks, or extracted factory firmware files. The corpus generator can use both local models and online TTS services; its source comments identify which engines send text to external services. Review the selected engine and its license before generating or redistributing audio.

## Current environment limitations

The scripts were developed and run in local Python environments under `vendor/`, which is ignored by Git. The training pipeline uses Python packages including PyTorch, NumPy, SoundFile, SciPy, Sherpa-ONNX, and audio/model assets. Some scripts contain fixed `vendor/...` paths and are not yet configurable through a standard system Python environment variable or a lockfile.

Therefore a clean clone does not currently contain everything needed to reproduce the published candidate. A contributor must prepare compatible Python dependencies, TTS/ASR assets, factory model templates obtained from an authorized local firmware image, and private recordings. The repository does not redistribute those assets.

## Record and prepare device audio

`tools/lx06-record.py` streams 8-channel capture from a configured LX06 over SSH to the Mac. It writes only on the Mac, beneath `private/inputs/recordings/wake-corpus/`; it refuses to overwrite an existing raw recording. Set `LX06_HOST` when the device address differs from the script default. Review the SSH host-key configuration before recording.

Convert a raw capture and replay the device-style AEC on the Mac:

```sh
python3 tools/lx06-record.py convert <session>
mkdir -p build/speex
ln -sf /opt/homebrew/lib/libspeexdsp.1.dylib build/speex/libspeexdsp.so.1
clang -O2 -I src -o build/host-aec tools/lx06-aec-offline.c
DYLD_LIBRARY_PATH=build/speex build/host-aec \
  private/inputs/recordings/wake-corpus/<session>.raw \
  private/inputs/recordings/wake-corpus/<session>.aec16k.s16
```

The raw input is the 8-channel, 48 kHz S32_LE stream. `convert` also writes downsampled WAV files for inspection; the AEC utility itself consumes the original `.raw` capture. The offline AEC replay approximates the device capture path and is not a substitute for on-device validation. Avoid recording third parties without consent.

## Generate and label training material

`tools/tts-corpus.py` creates positive, hard-negative, and ordinary-negative samples and writes a manifest under `temp/analysis/tts-corpus/`. The generator has several engines and model dependencies; some modes contact external TTS services. Its default environment and asset paths are currently fixed in the script.

`tools/wake-label.py <session> [...]` segments AEC-processed recordings and writes two ASR transcripts per segment. ASR output is a proposal, not ground truth. Listen to clips and export reviewed labels with `tools/wake-review.py` before using difficult music recordings as supervised examples. For sessions listed in `labels.csv`, unlabeled segments are excluded rather than falling back to ASR text.

## Train and evaluate

After preparing all inputs and dependencies, the current command shape is:

```sh
python3 tools/wake-train.py all --dim 96 --epochs 12 --out temp/analysis/wake-train/d96
python3 tools/wake-train.py eval --out temp/analysis/wake-train/d96
python3 tools/wake-train.py all --level 2 --dim 160 --out temp/analysis/wake-train/l2-d160
python3 tools/wake-train.py eval --out temp/analysis/wake-train/d96 \
  --verify temp/analysis/wake-train/l2-d160
```

These commands will fail until the required corpus, templates, private recordings, and Python packages exist at the script's expected paths. A training run writes `model.binn`, a PyTorch `net.pt`, decoded model/FST metadata, `prior.json`, `eval.json`, and `train.log` to the output directory. Keep raw training data and intermediate artifacts out of Git unless their distribution rights and privacy have been reviewed.

Review the evaluation output before deployment. It reports synthetic development outcomes and results on available recording sessions; thresholds are reference-model margins and cannot be copied directly into the device runtime. A model is only a candidate until the exact exported files pass container checks, isolated ARM/device inference, quiet and music evaluation, and a separately defined live acceptance run. The current v5 candidate has not met a broad field-acceptance standard.

## Reproducibility work still needed

The project needs a supported dependency manifest/lock, configurable asset paths, a clean-checkout setup command, documented sources and licenses for each required public asset, and a small non-sensitive fixture suite. Until those exist, describe custom training as an experimental developer workflow rather than a self-service feature.

## Evidence

The model experiments and measured outcomes are recorded in the [training reports](../reports/). For the model format and inference concepts, see [`wake-model-primer.md`](wake-model-primer.md). For current system behavior, see [`kws.md`](kws.md).
