# Tools

This directory contains project-owned scripts for building, inspecting, training, and deploying XiaoAI Sidekick. Third-party runtimes and downloaded datasets are not part of the repository. Install them on the host or create an ignored local environment; do not commit virtual environments, credentials, recordings, firmware images, or generated analysis data.

本文档描述工具入口和输入输出。面向用户的安装流程见根目录 README；设备恢复边界见 Agent Notes 中的部署说明。

## Tool groups

| Group | Examples | Purpose |
|---|---|---|
| Build | `Build-NativeGlibc.sh`, `Build-NativeProbe.sh`, `Build-WakeProbes.sh`, `build-factory-wake.sh` | Cross-compile ARM programs for the LX06 runtime and inspect ELF dependencies. |
| Device operations | `Invoke-LX06.sh`, `Manage-LX06Service.sh`, `Manage-LX06Web.sh`, `Invoke-DoubaoSmoke.sh` | Run SSH operations and manage the Sidekick services. |
| Wake model | `wake-train.py`, `wake-label.py`, `wake-review.py`, `native-model-am.py`, `native-model-pack.py` | Prepare labels, train and evaluate acoustic models, and read/write the factory BINN container. |
| Audio data | `lx06-record.py`, `lx06-aec-offline.c`, `tts-corpus.py` | Capture user-authorized recordings, replay the AEC chain offline, and generate synthetic speech data. |
| Firmware analysis | `native-*.py`, `analyze-native-wakeup.py`, `Compare-Rootfs.sh`, `Run-NativeProbe.sh` | Inspect locally available firmware and write derived results under ignored `temp/`. |
| Inputs and tuning | `Fetch-Inputs.sh`, `kws-tune.sh`, `Test-NativeMusic.sh` | Fetch checksum-pinned public inputs, tune local wake scoring, and run explicit device-side music checks. |
| Checks | `Check-LocalCredentials.py`, `Check-WebConsole.py`, `Check-NeteaseCharts.py` | Check local configuration or service behavior without printing credential values. |

## Host dependencies

The project scripts use Bash on Windows, macOS, and Linux. On Windows, install Git for Windows and run them from Git Bash; on other hosts use the system Bash. Invoke scripts with `bash tools/NAME.sh` from any working directory. They resolve the checkout from their own location. Git Bash searches the merged Windows `PATH`, while macOS/Linux use their native `PATH`. Set `LX06_ZIG` or `LX06_PYTHON` to override a tool path. The required curl 7.55.1 headers are included at `third_party/curl-7.55.1/include/` with the upstream license.

On Windows, install the system tools from PowerShell or Windows Terminal:

```powershell
winget install --id zig.zig --exact --version 0.13.0
winget install --id Python.Python.3.12 --exact --version 3.12.10
python -m pip install --user capstone==5.0.3 pyelftools==0.31
```

Then run a project script in Git Bash, for example `bash tools/Build-NativeProbe.sh`. The shell entry points require Bash features and are not intended for `cmd.exe` or PowerShell execution.

7-Zip is optional and is only needed to unpack archives. Other hosts should install compatible system packages with their native package manager.

For model training, install Python 3.9 or newer and the packages `numpy`, `torch`, `sherpa-onnx`, and `soundfile` in a user-managed virtual environment. Install ASR and TTS models separately and follow their upstream licenses. The exact model set depends on whether you generate synthetic speech, transcribe recordings, or train from an existing prepared corpus.

Set `LX06_MODEL_CACHE` to the user-local directory that contains `tts-models/`. The default is `~/Library/Caches/xiaoai-sidekick` on macOS, `%LOCALAPPDATA%\xiaoai-sidekick` on Windows, and `$XDG_CACHE_HOME/xiaoai-sidekick` (or `~/.cache/xiaoai-sidekick`) on Linux. The repository does not include these model assets or Python environments.

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
- Device scripts need a host-local `known_hosts` file (`LX06_KNOWN_HOSTS` selects its path). Windows Git Bash also uses the ignored `tools/local/ssh-askpass.cmd`. On Linux/macOS, use an SSH key or set `LX06_ASKPASS` to a local askpass program; the included `tools/ssh-askpass.sh` reads `LX06_SSH_PASSWORD` from the environment and needs its executable bit set after checkout (`chmod +x tools/ssh-askpass.sh`). Do not put passwords or API keys in command arguments or shell history.

## Current limitations

- The training workflow is research tooling, not a turnkey user-facing trainer. It needs private recordings or a prepared corpus, separately installed models, and factory graph templates.
- Some analysis scripts still require local firmware templates, recordings, or separately downloaded speech models. These inputs are not included in a clean checkout.
- The build scripts produce selected ARM executables. They do not currently rebuild and package a complete flashable ROM from a clean checkout.
