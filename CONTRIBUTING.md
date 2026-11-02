# Contributing

XiaoAI Sidekick is an experimental project for a rooted Xiaomi XiaoAI Speaker Pro (LX06). Contributions should make behavior easier to reproduce and should keep device changes recoverable. Please read the [README](README.md), [Agent Notes guide](.agents/notes/README.md), and [Security policy](SECURITY.md) before opening a pull request.

## Contribution areas

Useful contributions include:

- reproducible builds on a clean machine;
- read-only compatibility research for other speaker models or firmware versions;
- wake-word evaluation with consented recordings and clearly described conditions;
- safer deployment, recovery, and regression procedures;
- fixed, least-privilege integrations for Mi Home or infrared control;
- documentation that distinguishes tested behavior from assumptions.

Do not contribute an arbitrary shell or raw ubus interface exposed to models or the LAN. Do not add DRM, trial-playback, or account-permission bypasses.

## Before changing device behavior

Device experiments can affect boot, factory services, audio, accounts, and paid cloud sessions. Before writing to a speaker:

1. Identify the exact target model, firmware, partition, and files.
2. Prepare and verify a recovery copy on the development computer. Do not leave historical deployment backups on the speaker.
3. Record expected hashes and a concrete restore procedure before the write.
4. After the write, read back and verify the installed file or image. A successful command exit alone is not verification.
5. Stop sessions and services started for the experiment, remove temporary staging files, and check factory services and storage.

Do not delete unknown factory files to free space. Do not repeat driver installation without checking the current environment.

## Evidence and documentation

When documenting a result, label its evidence as one of:

- **Measured:** observed on a named device or local build, with conditions and command/output where useful.
- **Public source:** stated by an upstream project, vendor document, or standard; include a direct reference.
- **Inference:** reasoned from available evidence, with the uncertainty stated.
- **Unknown:** not established by the current evidence.

Keep implementation details in code and stable guidance in the maintained references. Use reports for experiment evidence and Agent Notes for decisions that explain why the project chose one approach over alternatives. Follow the repository's current naming and lifecycle rules; filenames and directory names must use ASCII characters.

Do not publish credentials, personal recordings, private labels, device dumps, recovery copies, or account data. See [Security](SECURITY.md).

## Local development

The primary documented native build scripts use Windows PowerShell and Zig 0.13. Some dependencies are local and are not committed. See [tools/README.md](tools/README.md) for the current setup and known platform limitations. A clean, cross-platform build workflow is not yet established.

Before submitting code, run the checks relevant to the changed area and report the exact commands and results. Do not claim a device test when only a local build or simulated input was used. For device-facing changes, include the target, firmware, recovery method, and observed post-change state in the review description.

## Pull requests

- Keep each change focused and explain the user-visible or maintenance outcome.
- Include a reproduction or validation procedure.
- Update the owning reference or decision note when behavior or a stable contract changes.
- Do not include generated build output, personal data, credentials, or unlicensed third-party assets.
- Declare new dependencies with their version, source, and license.

Project-authored code and documentation are licensed under the [MIT License](LICENSE). Before adding third-party code, model assets, firmware-derived files, or datasets, document their source and redistribution terms. The project license does not replace those separate terms.
