# Tools

This directory contains Bash entry points for speaker connection and deployment, diagnostics, firmware inspection, and wake-model research. [GitHub Actions](../.github/workflows/build-speaker-tools.yml) is configured to build the Web and Duplex binaries and publish workflow artifacts. Do not use these artifacts as firmware images.

## Device operations

`Invoke-LX06.sh` provides the shared SSH connection path. `Manage-LX06Service.sh` and `Manage-LX06Web.sh` install, start, stop, and inspect the corresponding Sidekick services. Set `LX06_HOST` and `LX06_KNOWN_HOSTS` for the target and host-key file. For password authentication, set `LX06_SSH_PASSWORD` in the environment or configure `LX06_ASKPASS`; never pass credentials as command-line arguments or write them to logs.

Before deploying Web or Duplex binaries, run `bash tools/test-speaker-tools-local.sh`. After deploying Web, run `python3 tools/Check-WebConsole.py`. Follow the [deployment and recovery guide](../.agents/notes/reference/deployment-and-recovery.md) before any device write.

## Research and diagnostics

Wake-model training, audio preparation, and firmware inspection scripts consume locally supplied inputs under `private/` or `temp/`. They are research tools, not a clean-clone training or firmware-image workflow. See the [wake-model training reference](../.agents/notes/reference/wake-model-training.md) before using them.

The local test suite uses synthetic inputs and fake device interfaces. `Check-WebConsole.py` and `Check-NeteaseCharts.py` can target a speaker when `LX06_BASE_URL` is set. Credential checks report status only and must never print secret values.
