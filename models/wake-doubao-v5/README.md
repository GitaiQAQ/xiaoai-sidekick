# Experimental Doubao wake model, v5

This directory contains the baseline v5 acoustic model pair for the experimental LX06 two-stage wake path. The files are checked in so users can inspect and evaluate the artifacts without access to the original development machine. A later device configuration changed FST costs and loop settings without changing the acoustic neural networks; those exact adjusted containers are not included here.

| File | Size | SHA-256 | Role |
|---|---:|---|---|
| `level1-model.binn` | 639,597 bytes | `de899046087da8729a245764ef853ba10884b555e9599d01dbb592995476610f` | First-stage candidate model |
| `level2-model.binn` | 1,678,701 bytes | `5945e591204a6ce2dedbab1c8bd18197e5e9fb49b1ba9bf4b0f88dd2be00464f` | Second-stage confirmation model |

These are experimental artifacts, not a production-quality wake model. The first-stage model had limited recall in music playback; the second stage reduced false accepts in offline evaluation but also reduced positive recall. Device replay results do not establish real-world performance across speakers, rooms, voices, or music. Review the [training report](../../.agents/notes/reports/custom-wake-model-training-and-macos-evaluation.md), [isolated deployment report](../../.agents/notes/reports/custom-wake-model-isolated-deployment.md), and [decoder tuning report](../../.agents/notes/reports/factory-wake-decoder-sensitivity-tuning.md) before using or replacing the pair.

The model container is derived from the factory wake-engine format. It is not a standalone model for a general-purpose inference runtime. The current application expects its own model paths and the LX06 factory wake engine; see the [wake model training guide](../../.agents/notes/reference/wake-model-training.md).

Verify the files with:

```sh
cd models/wake-doubao-v5
shasum -a 256 -c SHA256SUMS
```

Do not treat a matching checksum as evidence of device compatibility or recognition quality. Those require runtime and acoustic evaluation on the intended target.
