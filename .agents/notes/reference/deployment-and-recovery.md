# Reference: Deployment and recovery

This guide defines the current operational boundary for installing Sidekick files on an LX06. It is not a firmware flashing guide. The repository does not contain a complete, reproducible flash image or a verified full-device recovery process.

## Supported target

The only tested target is Xiaomi XiaoAI Speaker Pro (LX06), firmware 1.94.13, with the community SSH patch. Other models and firmware versions are unverified. SSH access does not prove that factory voice, Mi Home, mesh gateway, alarms, or playback will continue to work after a change.

## Before a device write

1. Identify the exact device, firmware, target file or partition, and intended change.
2. Prepare a device-specific rollback plan. Save deployment recovery files on the Mac under ignored `private/deploy-recovery/`; do not create `/data/lx06-agent/revision*-original` directories on the speaker.
3. Record the expected hashes and verify the recovery copies before changing the device.
4. Review the actual deployment command and its stop/start behavior. A successful shell exit does not prove that the device read or accepted the intended bytes.
5. Do not write a firmware partition unless the exact image, target partition, and recovery procedure are independently reviewable.

## After a device write

Read back and hash the installed files. Check service state, available storage and memory, and the factory processes relevant to the change. Treat file-length equality as a transfer check, not a recovery test. After acceptance, remove temporary staging files from the speaker. Do not remove factory files, account state, or mesh data to free space unless the owner and purpose are known.

## Recovery

Stop the Sidekick services affected by the deployment, restore only the files covered by the prepared recovery plan, then verify their hashes and service state. Keep Netease account state unless the user explicitly requests logout or removal. Recovery of Sidekick application files does not prove that a firmware partition can be restored.

## Historical reports

Older reports may describe device-side `revision*-original` directories, fixed local paths, process IDs, or deployment sequences. Those details record past operations only and are not current instructions. Current device and recovery status is summarized in [project context](context.md); experimental evidence remains in [reports](../reports/).

## Deployment reports

- [Round 14 wake and conversation update](../reports/wake-feedback-and-follow-up-dialogue.md)
- [Round 16 web console and Netease login](../reports/web-console-and-netease-login.md)
- [Round 30 custom wake model deployment](../reports/custom-wake-model-isolated-deployment.md)
- [Round 31 local response and voice service](../reports/local-response-without-credentials.md)
- [Round 34 decoder configuration](../reports/factory-wake-decoder-sensitivity-tuning.md)
