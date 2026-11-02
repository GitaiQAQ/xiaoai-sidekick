# Tools

This directory contains project-owned scripts for building, inspecting, training, and deploying XiaoAI Sidekick. Third-party runtimes and downloaded datasets are not part of the repository. Install them on the host or create an ignored local environment; do not commit virtual environments, credentials, recordings, firmware images, or generated analysis data.

本文档描述工具入口和输入输出。面向用户的安装流程见根目录 README；设备恢复边界见 Agent Notes 中的部署说明。

## Tool groups

| Group | Examples | Purpose |
|---|---|---|
| Build | `Build-NativeGlibc.ps1`, `build-factory-wake.sh` | Cross-compile ARM programs for the LX06 runtime and inspect ELF dependencies. |
| Device operations | `Invoke-LX06.ps1`, `Manage-LX06Service.ps1`, `Manage-LX06Web.ps1` | Run approved SSH operations and manage the Sidekick services. |
| Wake model | `wake-train.py`, `wake-label.py`, `wake-review.py`, `native-model-am.py`, `native-model-pack.py` | Prepare labels, train and evaluate acoustic models, and read/write the factory BINN container. |
| Audio data | `lx06-record.py`, `lx06-aec-offline.c`, `tts-corpus.py` | Capture user-authorized recordings, replay the AEC chain offline, and generate synthetic speech data. |
| Firmware analysis | `native-*.py`, `analyze-native-wakeup.py`, `Compare-Rootfs.ps1` | Inspect locally available firmware and write derived results under ignored `temp/`. |
| Checks | `Check-LocalCredentials.py`, `Check-WebConsole.py`, `Check-NeteaseCharts.py` | Check local configuration or service behavior without printing credential values. |

## Host dependencies

Use system-installed tools where possible. The scripts do not require a directory named `vendor/`; specify a tool path through the documented command-line option or environment variable when a script supports one. Some legacy PowerShell scripts still contain machine-specific paths and are not yet portable.

For model training, install Python 3.9 or newer and the packages `numpy`, `torch`, `sherpa-onnx`, and `soundfile` in a user-managed virtual environment. Install ASR and TTS models separately and follow their upstream licenses. The exact model set depends on whether you generate synthetic speech, transcribe recordings, or train from an existing prepared corpus.

For ARM builds, use Zig 0.13.0 and the LX06-compatible glibc 2.25 target. Some native builds also need the matching curl headers and device ABI reference files. These inputs are not all distributed in this repository, so a clean checkout is not currently sufficient to produce a complete flashable firmware image.

## Wake model workflow

The training scripts expect generated synthetic speech under `temp/analysis/tts-corpus/`, user-provided recordings and labels under `private/inputs/recordings/wake-corpus/`, and factory acoustic graph templates under `temp/analysis/models/`. The scripts write model weights and evaluation output under `temp/analysis/wake-train/`. Paths and required inputs are currently tied to this repository layout; see the [wake model training reference](../.agents/notes/reference/wake-model-training.md) before running them.

```sh
python tools/wake-train.py all --dim 96 --epochs 12 --out temp/analysis/wake-train/d96
python tools/wake-train.py eval --out temp/analysis/wake-train/d96
```

These commands illustrate the current interface. They do not fetch datasets or factory templates, and they do not establish that a resulting model is suitable for deployment. Evaluate with user-recorded positive and negative samples, inspect false accepts and misses, and validate the selected model on the target speaker before deployment.

## Data and safety boundaries

- `private/` contains credentials, personal recordings, and device-specific recovery material; never commit it.
- `temp/` contains generated or reconstructable inputs and results; never treat it as a release artifact without a separate review.
- Device write operations must follow the repository's deployment and recovery procedure. A successful command exit alone does not prove that the target device read or accepted the intended bytes.
- Credential checks must report presence or validity flags only, never the secret itself.

## Current limitations

- The training workflow is research tooling, not a turnkey user-facing trainer. It needs private recordings or a prepared corpus, separately installed models, and factory graph templates.
- Several PowerShell and analysis scripts still assume local Windows or `vendor/` paths. They need migration before the repository can claim clean-checkout reproducibility.
- The build scripts produce selected ARM executables. They do not currently rebuild and package a complete flashable ROM from a clean checkout.
