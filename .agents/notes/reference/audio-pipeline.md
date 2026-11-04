# Reference: 音频采集与回声消除（设备现状与逆向结论）

本文记录 2026-11-01 为止对 LX06 音频链路的实测与逆向结果，供后续模型直接复用。结论分三类：实测、反汇编证据、待验证。

## 1. ALSA 拓扑（实测）

`/etc/asound.conf`：

- `pcm.mico_record` = `plug(Capture)`，插件到 16 kHz / 单声道 / S16，供原厂与我们的客户端读取。
- `pcm.Capture` = `dsnoop`（ipc_key 1024，权限 0666），slave 为 `hw:0,3`，48 kHz、`S32_LE`、8 通道，period 384、buffer 6144。
- 默认播放 `default` = softvol → dmixer → `hw:0,2`，48 kHz 立体声。
- 播放和采集都能与 `mipns-xiaomi`（PID 1282）共享；它持有 `/dev/snd/pcmC0D3c`，并加载 `libxaudio_engine.so`、`libmdspeech.so`、`libspeexdsp.so.1.5.0`。

无 ALSA 级 AEC/NS 插件；`asound.conf` 里只有 plug/softvol/dmix/dsnoop。

## 2. 8 通道布局（实测）

用 `lx06-duplex-armhf --afe-channels <秒>` 直接读 `Capture`（dsnoop 8 通道 S32/48k），统计每通道 RMS 与峰值：

| 通道 | 静音 RMS | 播放音乐时 RMS | 播放时峰值 | 判断 |
|---|---|---|---|---|
| 0–5 | 3.8e12–5.1e12 | 2.2e13–2.8e13 | 1.9e7–2.1e7 | 6 个麦克风，含扬声器回声 |
| 6 | 0 | 2.0e14 | 5.6e7 | 播放参考（左） |
| 7 | 0 | 2.0e14 | 5.6e7 | 播放参考（右，与 6 完全一致） |

结论：硬件把 6 路麦克风和 2 路播放参考放在同一路采样流里，**天然采样对齐**，这是做 AEC 的理想输入。原厂 `mico_record` 现在把 8 通道直接混成单声道，参考信号被混进了麦克风信号。

## 3. 原厂 AEC 的实现（反汇编证据）

`libxaudio_engine.so` 导出 `CreatAECHandler`、`SetAECMember`、`GetAECMember`、`AECProcess`、`DestroyAECHandler`（`STB_GLOBAL`，见 `temp/analysis/usr_lib_libxaudio_engine.so-symbols.json`）。反汇编（`tools/dump-native-functions.py`）显示：

- `CreatAECHandler` @0x2958c：`calloc(0x14)`；`speex_echo_state_init(frame=128, filter=4096)`；`speex_preprocess_state_init(128, 16000)`；`speex_echo_ctl(st, 24 /*SET_SAMPLING_RATE*/, &16000)`；`speex_preprocess_ctl(pp, 24 /*SET_ECHO_STATE*/, st)`。
- `AECProcess` @0x29714：`speex_echo_cancellation(state, mic, ref, out)` → `speex_preprocess_run(pp, out)`，输出帧长固定 256 字节（128 个 S16 采样）。
- `SetAECMember(handler, value, member)`：member 0 写 handler+8，member 1 写 handler+0xc。
- `DestroyAECHandler` @0x296d4：销毁两个 Speex 状态。

即：原厂用的是 **Speex 回声消除 + 预处理**，8 ms 一帧、256 ms 尾长、16 kHz。设备自带 `libspeexdsp.so.1.5.0`，符号齐全（版本与哈希见操作日志的部署记录）。

## 4. 我们的 AEC 原型（实测）

`lx06-duplex-armhf --aec-test <秒>`：读 `Capture` 8 通道 → ch0 作麦克风、ch6 作参考 → 3 点平均降采样到 16 kHz → 同一套 Speex 调用（128/4096，预处理 128/16000）。

| 场景 | mic RMS | AEC 输出 RMS | 参考 RMS | 抑制比 |
|---|---|---|---|---|
| 播放音乐（音量 30） | 52566 | 664 | 346155 | 79.1×（约 38 dB） |
| 播放音乐（早期小音量测试） | 5309 | 585 | 53456 | 9.08×（约 19 dB） |
| 静音 | 4843 | 893 | 41015 | 5.42×（约 14.7 dB） |

说明 AEC 确实在吃参考信号工作。两个关键细节：

- 本机库导出的是 `speex_echo_ctl`，不是 `speex_echo_state_ctl`；早期原型取错名字，采样率一直停在库默认的 8 kHz。修正为 16000 后，同一段语音经 AEC 的距离从 13.3 降到 9.6。
- 降采样是 3 点盒式平均，未做正式抗混叠滤波；参考与麦克风用同一滤波器，对齐关系保持。

## 5. 正式采集路径（已接入）

`src/native_capture.h` 把原型接进语音客户端，唤醒与云端上行共用同一份输出：

- 默认读 dsnoop `Capture`，8 通道 S32/48 kHz；麦克风取 ch0–ch5 的平均，ch6 作参考。
- 3 点平均降到 16 kHz，累加器跨读取块保持精确速率；每 128 样本跑一次 `speex_echo_cancellation` + `speex_preprocess_run`，写环形缓冲。
- 采样率通过 `speex_echo_ctl(st,24,&16000)` 设置；启动日志 `capture_speex_rate=16000`。
- 调用方（KWS、agent loop、`--kws-capture`）仍按 320 样本读取。
- 8 通道 PCM / `libspeexdsp` / Speex 状态任一不可用时回退 `mico_record`；`--no-aec`、`LX06_AEC=0` 强制回退。
- 音乐播放且 AEC 生效时：阈值 15.0，连续 2 个命中暂停播放器 2.2 秒做确认，未确认则恢复，6 秒冷却。静音环境仍是 12.5 / 3 连击。
- 实测开销：单核约 19.75% → 21.0%，RSS 2060 → 2288 kB。

`--mic-compare <秒>` 用同一段采集并行打分各版本，音频只留在内存：

| 路径 | 说话时最低距离 | 只放音乐 |
|---|---|---|
| ch0 原始 | 9.24 | 12.57 |
| 6 麦混音原始 | 8.97 | 11.26（误唤醒） |
| ch0 + AEC + 预处理 | 12.05 | 14.55 |
| ch0 + AEC（无预处理） | 9.57 | 14.57 |
| 6 麦混音 + AEC + 预处理 | 11.66 | 13.41–19.34，随音量变化 |
| 6 麦混音 + AEC（无预处理） | 8.85 | 11.03（误唤醒） |

## 6. 现有语音路径与半双工行为

- 采集：`native_capture.h`，AEC 开时 8 通道 → 6 麦平均 → Speex → 16 kHz 单声道；AEC 关时 `mico_record`（8 通道混音，含回声）。
- 播放：回答走 `default`，24 kHz 单声道 PCM，解码后乘 `LX_ANSWER_GAIN`（默认 250%）。
- 回答一开始就发 `input_audio_mute.commit` 并停止上行；播放期间只做本地唤醒词检测，命中后清空播放缓冲、解除静音、进入新回合（`barge_in=1`）。
- 唤醒应答优先播放 `/data/lx06-agent/wake_ack.wav`（豆包音色，峰值归一化到 30000），缺失时回退 `/usr/share/sound/wakeup_wozai.wav`。

## 7. 待办（按优先级）

1. 高音量与远场回归：音量 50 以上、3 米以外的召回率；必要时把候选暂停改成更轻的 duck。
2. 逐麦克风 AEC（`speex_echo_state_init_mc` 可用）或 delay-sum 波束成形，减少对候选暂停的依赖。
3. 降采样换成正经的低通（例如 12–24 抽头 FIR），减少混叠对唤醒词的影响。
4. 评估 AEC 之后是否还保留“回答期间静音上行”；若云端 VAD 能接受，可放开为真正的全双工打断。
5. 统计长时间运行下的误暂停频率，调整 15.0 阈值、2 连击与 6 秒冷却。

## 8. 复现命令

```sh
# 8 通道电平
bash tools/Invoke-LX06.sh --command "/lib/ld-linux-armhf.so.3 /data/lx06-agent/lx06-duplex-armhf --afe-channels 2"

# AEC 抑制比（先让音乐播放，再执行）
bash tools/Invoke-LX06.sh --command "/lib/ld-linux-armhf.so.3 /data/lx06-agent/lx06-duplex-armhf --aec-test 4"

# 采集路径 A/B：同一段语音并行打分，不保存音频
bash tools/Invoke-LX06.sh --command "/lib/ld-linux-armhf.so.3 /data/lx06-agent/lx06-duplex-armhf --mic-compare 20"
```
