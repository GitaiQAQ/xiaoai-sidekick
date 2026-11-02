# Reference: Documentation guide

This guide maps each reader task to its current documentation owner. It is curated rather than generated. Use the root [README](../../../README.md) for the project overview and public status; use this guide to find technical detail and evidence.

## Read by task

| Reader task | Start here | Evidence and limits |
|---|---|---|
| Understand the supported device and current project boundary | [README](../../../README.md), [device and deployment scope](deployment-and-recovery.md) | [ROM capacity and extraction report](../reports/rom-capacity-and-rootfs-extraction.md) |
| Build the native applications | [Tools guide](../../../tools/README.md), [Zig build design](zig-architecture-design.md) | The repository does not currently provide a clean-clone, complete firmware-image build and recovery workflow. |
| Install or recover a device | [Deployment scope](deployment-and-recovery.md), [SSH access](ssh.md), [credentials](credentials.md) | Device writes require a target-specific recovery plan. Past reports are evidence, not authorization or a current recovery procedure. |
| Understand audio capture and echo cancellation | [Audio pipeline](audio-pipeline.md) | [Capture-path AEC report](../reports/capture-path-echo-cancellation-integration.md), [voice/AEC analysis](../reports/voice-interaction-and-aec-analysis.md) |
| Understand the web and local control interfaces | [Control protocol](control-protocol.md), [real-time client](realtime-client.md) | [Tool registry report](../reports/agent-loop-and-tool-registry.md) |
| Understand wake-to-speech handoff and general dialogue | [Local wake detection](kws.md), [real-time client](realtime-client.md) | [Round 35 handoff and dialogue report](../reports/realtime-wake-handoff-and-general-dialogue-fix.md); candidate changes are not yet field-validated. |
| Train or evaluate a wake model | [Wake model primer](wake-model-primer.md), [training workflow](wake-model-training.md), [wake detection](kws.md) | [Training evaluation](../reports/custom-wake-model-training-and-macos-evaluation.md), [shared-state experiment](../reports/shared-state-wake-model-training.md), [decoder tuning](../reports/factory-wake-decoder-sensitivity-tuning.md), [device deployment](../reports/custom-wake-model-isolated-deployment.md) |
| Understand music playback integration | [Control protocol](control-protocol.md) | [Playlist queue](../reports/player-track-metadata-and-playlist-queue.md), [native playback](../reports/on-device-music-stream-and-playback-control.md), [redirect refresh](../reports/local-redirect-refresh-and-native-loop.md) |
| Contribute or report a security issue | [Contributing](../../../CONTRIBUTING.md), [Security](../../../SECURITY.md) | Do not publish credentials, personal recordings, device dumps, or recovery copies. |

## Evidence language

Use these labels consistently in references and reports:

- **Measured:** directly observed in a named local run or on a named device. Include the conditions that affect the result.
- **Public source:** documented by an upstream project, vendor, or standard. Link to the source.
- **Inference:** a conclusion drawn from available evidence; state what remains uncertain.
- **Unknown:** the current evidence does not establish the behavior.

Historical reports describe what happened during that experiment. They do not establish current code behavior by themselves. For current behavior, check the implementation and the owning reference.

## Agent Notes

Agent Notes record durable decisions and their alternatives, not task progress. The [Agent Notes guide](../README.md) defines lifecycle folders, the closed class set, required headings, filename rules, and the validation command. Keep the reference guide and reports outside the lifecycle tree.

## Operational record

The [operations log](../../../logs/operations.md) remains a chronological record of device and development actions. It is not the user guide or the current architecture specification.
