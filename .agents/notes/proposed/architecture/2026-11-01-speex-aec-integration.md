# Agent Note: Speex echo cancellation for the capture path

Status: proposed

## Problem

音乐与播报时，麦克风混入扬声器输出：唤醒词距离升高、误唤醒增多、云端转写为空。原厂小爱能在同一台设备上精确打断，说明硬件链路里有可用的回声消除。

排查结果：厂商的 AEC 在 `mipns-xiaomi` 进程内，随 `libxaudio_engine.so` 加载，没有以 ALSA PCM、FIFO、socket 或共享内存的形式暴露；`/tmp/mipns/usock/speech.usock` 只传 ASR 控制消息。当前客户端读 `mico_record`，它把 8 通道直接混成 16 kHz 单声道，参考信号被混进麦克风。

## Proposal

把正式采集路径改为：

1. 读 `hw:0,3` 的 dsnoop `Capture`，8 通道 S32/48k。
2. ch0 作为麦克风，ch6 作为播放参考；两者在同一采样流里，天然对齐。
3. 3 点平均降到 16 kHz，按 128 点一帧跑 Speex AEC 与预处理，参数与反汇编得到的原厂实现一致：`speex_echo_state_init(128, 4096)`、`speex_preprocess_state_init(128, 16000)`、`speex_echo_ctl(state, 24, 16000)`、`speex_preprocess_ctl(pp, 24, state)`。
4. AEC 输出同时供本地唤醒与云端上行使用；保留现有半双工静音作为兜底，稳定后再评估放开为全双工 VAD 打断。

原型已经用只读模式 `--aec-test` 验证：音乐下 mic RMS 5309 → 585（约 19 dB），静音 4843 → 893。

## Acceptance criteria

- 音乐播放时 AEC 输出相对输入的抑制不低于 15 dB，语音段不被明显吞掉。
- 唤醒词在音乐下的最差距离和误唤醒率都优于当前 4 连击策略，现场可复现。
- 不做 AEC 的旧路径仍可回退（启动参数或环境变量），切换不需要重新刷机。
- 采集与 AEC 的常驻 CPU、内存增量在设备可接受范围内，语音会话的端到端延迟不增加超过 50 ms。
- `--afe-channels`、`--aec-test` 保持只读，不影响原厂进程。

## Risks

- `speex_echo_ctl` 用 `dlsym` 取不到（其余符号正常），未设置采样率时用的是库默认配置；需要 `dlvsym` 或调用引擎封装确认。
- 3 点平均降采样没有正式抗混叠，可能影响唤醒特征；需要 FIR。
- 只用了 ch0，没有波束成形；换通道或简单 delay-sum 需要重新验证。
- 8 通道读取和 AEC 的 CPU 成本尚未在长时间会话中测量。
- 若参考通道包含音量处理后的信号，滤波器需要重新收敛，可能影响首字。

## Alternatives considered

- **读取厂商 AFE 的输出**：最省事，但排查后没有暴露的 PCM、FIFO 或音频 socket；只有控制消息。
- **直接调用 `libxaudio_engine` 的 AEC 接口**：符号齐全，但需要还原初始化序列、成员枚举和并发使用方式；与 `mipns-xiaomi` 同时运行存在冲突风险。
- **自研 NLMS 自适应滤波**：可控，但需要处理延迟估计和双讲，调参成本高于直接复用设备已有的 Speex 实现。
- **维持半双工 + 唤醒词打断**：不引入新 DSP，但音乐下误唤醒和漏唤醒无法根治。
