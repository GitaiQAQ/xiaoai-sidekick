# Reference: Local wake detection

The LX06 duplex process runs wake detection on the speaker before it opens a Doubao real-time session. The implementation defaults to the factory Rice/Paddy decoder loaded with two custom model containers. The project has not established a broad live acceptance rate; false rejects remain common, especially during music playback.

## Runtime path

`src/native_duplex_client.c` selects the factory decoder by default. It initializes the two model files through `src/native_factory_kws.h` and feeds captured 16 kHz mono PCM to the decoder in 160-sample blocks. Only decoder `flag=1` counts as a confirmed wake event. Other decoder events are logged but do not start the voice session.

The default model paths on the speaker are `/data/lx06-agent/models/d96-v5.binn` and `/data/lx06-agent/models/l2-d160.binn`. `LX06_WAKE_MODEL1` and `LX06_WAKE_MODEL2` can override those paths. If either model is unreadable or decoder initialization fails, the factory wake path stops; it does not silently fall back to DTW. The older DTW path is selected only when `LX06_WAKE_ENGINE=dtw` is set explicitly for diagnostic use.

When a wake event is confirmed and a realtime credential is available, the process starts the cloud voice session. Without a credential, it gives a local acknowledgement and plays a local configuration prompt without connecting to the cloud. See [credential handling](credentials.md) and the [training workflow](wake-model-training.md).

## Current model evidence

The repository's `models/wake-doubao-v5/` contains the baseline v5 acoustic model pair. Round 34 changed decoder configuration/FST costs while keeping the acoustic networks unchanged; the exact adjusted containers installed on the device are not checked in. Offline replay raised the labeled positive hit count from 5/58 to 13/58 and the grouped utterance count from 3/35 to 7/35. The TTS near-word false-accept count rose from 1/20 to 3/20. Two music-only clips produced no detections. This is a limited offline comparison, not a live acceptance test or an overall accuracy estimate.

The next validation must include a user-confirmed live wake run, representative speech and music negatives, and a defined acceptance threshold. Do not describe the current model as reliable or the music false-wake problem as fixed. Detailed evidence is in the [round 34 report](../reports/factory-wake-decoder-sensitivity-tuning.md) and the [isolated deployment report](../reports/custom-wake-model-isolated-deployment.md).

## Rebuild and deployment

See the [wake model primer](wake-model-primer.md) for model concepts and the [wake model training guide](wake-model-training.md) for the experimental training workflow. See [deployment and recovery](deployment-and-recovery.md) before writing model files to a device. A model file passing checksum or format validation does not prove that the decoder recognizes the intended phrase on the target speaker.
