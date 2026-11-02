# Reference: Current project context

This page is the short handoff for maintainers and coding agents. It records the latest confirmed project state, open limitations, and the references that own each technical subject. The [operations log](../../../logs/operations.md) keeps the chronological history; detailed evidence stays in [reports](../reports/). Public-facing documentation was reorganized for an open-source audience; see the [documentation guide](knowledge.md).

## Project status

XiaoAI Sidekick is an experimental, root-level sidecar for the Xiaomi XiaoAI Speaker Pro (LX06). The only tested firmware target is 1.94.13 with the community SSH patch. The project aims to preserve the factory assistant, Mi Home, mesh gateway, and media player, but full factory-feature regression and firmware recovery have not been completed.

The repository contains native application source, web assets, scripts, experimental wake-model tooling, and two checked-in wake-model artifacts. It does not provide a clean-clone process to rebuild a complete flashable firmware image or verify recovery on a new device. See the [root README](../../../README.md) and [tools guide](../../../tools/README.md).

## Latest confirmed behavior

- **Wake model:** round 34 installed an easier-to-trigger FST/configuration using the same acoustic neural networks as the v5 baseline. Offline ARM replay improved the labeled s3 hit count from 5/58 to 13/58; the TTS near-word sample also rose from 1/20 to 3/20 false accepts. Pure-music replay had zero detections in the two tested clips. This is not a live acceptance test, and recall remains low. The repository's `models/wake-doubao-v5/` contains the baseline pair, not the exact adjusted round-34 containers installed on the device. See the [round-34 report](../reports/factory-wake-decoder-sensitivity-tuning.md) and [isolated deployment report](../reports/custom-wake-model-isolated-deployment.md).
- **Voice service:** the latest recorded field session had the service enabled with credentials loaded and successful cloud session events. Some wake candidates were rejected by the second stage; recall remains limited. A later credential retrieval was recorded, but this context does not claim a subsequent service restart or new live acceptance. See [local response and credential report](../reports/local-response-without-credentials.md) and the operations log.
- **Wake-to-speech candidate:** round 35 changed the general system instruction and introduced a single capture owner with a bounded preroll queue to avoid losing speech while the realtime session starts. The candidate passed synthetic handoff checks, session-policy checks, and ARM `-Werror` compilation. It has not been deployed or validated with a live user utterance. See the [round 35 report](../reports/realtime-wake-handoff-and-general-dialogue-fix.md).
- **No-credential behavior:** local wake detection and acknowledgement can run without cloud credentials; the speaker plays a local configuration prompt and does not connect to the cloud in that branch. See the same [report](../reports/local-response-without-credentials.md).
- **Storage:** historical deployment backups revision17–28 were removed from the speaker with the user's authorization. Future rollback material belongs on the Mac under ignored `private/deploy-recovery/`; do not recreate `/data/lx06-agent/revision*-original`. Temporary device staging must be removed after acceptance. See the [speaker storage report](../reports/speaker-storage-and-rom-size-assessment.md).
- **ROM analysis:** the current system0 SquashFS header reports 38,449,988 bytes used in a 40 MiB partition. The remaining capacity is not evidence that the tail is physically writable or that a repacked image is safe to flash. No new image was built or flashed. See [ROM capacity report](../reports/rom-capacity-and-rootfs-extraction.md).

## Model artifacts and training

The repository tracks the current experimental model pair under `models/wake-doubao-v5/`. Their SHA-256 values are recorded in the model manifest. They are usable artifacts for the current loader path, not a claim that the wake model meets a release-quality acceptance threshold.

Training scripts are available, but end-to-end training is not reproducible from a clean clone. The workflow still needs user-provided recordings and labels, separately installed Python packages and speech models, and acoustic/FST templates extracted from a compatible factory image. Some scripts assume local `vendor/` paths. See [wake model training](wake-model-training.md) for exact inputs and gaps.

## Evidence and operating rules

- Separate measured results, public-source claims, inference, and unknown behavior.
- Do not publish credentials, account sessions, device authentication data, personal recordings, private labels, firmware dumps, or recovery copies.
- Do not repeat driver installation without checking the current host first.
- Before any device write, prepare and verify a concrete image/file and recovery plan. A successful command exit or matching file length alone does not prove that the device accepted the intended data or that recovery works.
- Keep original material in `backups/`; do not edit it. Put modified firmware images under `firmware/`.

## Topic owners

- [Documentation guide](knowledge.md) — reader routes, evidence language, and document ownership.
- [Audio pipeline](audio-pipeline.md) — ALSA capture, channels, and echo cancellation.
- [Control protocol](control-protocol.md) — local and HTTP control surfaces.
- [Wake detection](kws.md), [wake model primer](wake-model-primer.md), and [training workflow](wake-model-training.md) — detection and model work.
- [Deployment scope](deployment-and-recovery.md), [SSH access](ssh.md), and [credentials](credentials.md) — device access and operational boundaries.
