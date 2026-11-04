# Report: Wake-Word Threshold, Session Limits, and Daemon Lifecycle

日期：2026-11-01。本文记录 LX06 上早期模板检测器的阈值调整、会话时限和守护进程生命周期验证。相关配置是当时的实验结果，不代表当前模型或部署流程。

**结论：** 本轮的有限 TTS 与现场负样本均未触发，但召回下降；第十二轮中持续有人说话导致误唤醒的场景没有复测。因此，本轮不能证明误唤醒问题已解决。会话上限、无人声超时和信号清理通过了记录中的实验；这些结果也不能代表所有服务端断开和设备重启场景。

## 背景：持续上传与计费风险

用户在控制台看到持续计费。核查发现 `/tmp/lx06-agent-realtime-62d858e0`（PID 3509）仍在运行，持有到 `openspeech.bytedance.com`（157.255.232.105:443）的 ESTABLISHED 连接并持续上行音频。已 kill，连接转 TIME_WAIT，删除 `/tmp/lx06-agent-*`；同时停掉仍在跑现场测试的子代理和它拉起的待机进程（PID 3554）。复核无 `lx06-agent` 进程、无 openspeech 连接、无自启动项，原厂 1282/2527 未动。

根因：演示客户端只要进程活着就持续上行；服务端只在约 18 秒无音频时断会话。唤醒版本虽然能按需开会话，但单次会话没有硬上限，进程也没有信号清理。

## 实现变更

### 唤醒判决参数

- 阈值由 1.3×簇内最大（14.7784）改为 **1.10×（12.5048）**，`tools/kws-build-templates.py` 同步改为默认 1.10。
- 连续命中由 2 窗改为 **3 窗**（`LX_KWS_MIN_HITS`），约 0.6 秒持续匹配才判醒。
- 模板与特征不变（段 0/1/3，MFCC 13+13，c0 包络×0.25）。

### 会话结束条件

- `LX_SESSION_MAX`：单次会话硬上限，默认 25 秒。
- `LX_NO_SPEECH`：判醒后无人声超时，默认 6 秒；单帧峰值 >1200 视为有人声。
- `SIGTERM/SIGINT/SIGHUP`：置停止标志，循环退出并发送 `session.close`，日志打印 `session_stop=signal` 或 `standby_stopped=signal`。
- 回答结束（`response.output_audio.done` / `response.done`）立即关闭；退出前最多等 30 秒播完缓冲。

### 守护进程与密钥传递

- 新增 `--daemon <pidfile>`：`fork` + `setsid` + 0700 pidfile，SSH 断开不带走进程。
- `deploy/start.sh`：Key 经 0600 FIFO 传入，判醒后才读，不落盘、不进 argv；`stop.sh` 杀客户端与写入者、删 FIFO、按路径兜底；`status.sh` 显示进程、openspeech 连接与日志尾。
- 安装到 `/data/lx06-agent/`（ubifs，新建目录，余量 8.2 MB），未配置开机自启。
- 新增 `tools/Manage-LX06Service.sh`：安装/启动/停止/状态；`-FakeKey` 只验证启停。

## 测量结果

| 项目 | 条件 | 结果 |
| --- | --- | --- |
| 离线负样本 | 53 条中文 TTS（34 句日常话术，前 8 句 ±2 语速），16 kHz 单声道 | 0 次唤醒，最低距离 19.68 |
| 现场负样本 20 s | 峰值 52–58 dB、底噪 34–46 dB | 92 窗、0 判醒，最低 17.84 |
| 现场负样本 15 s | 同环境 | 67 窗、0 判醒，最低 15.29 |
| 回放注册录音 | `--kws-wav`，无 Key | 2.07 s 判醒，距离 0.0001，停在 `detection_only` |
| 真实会话闭环 | `--kws-wav` + 真实 Key，`-Seconds 8` | 101 → `session_ready=1` → 前滚 4800 → 299 帧 → 6 s `no_speech` → `session=pass` |
| 信号清理 | `timeout -t 8` 发 SIGTERM 给 300 s 待机 | `standby_stopped=signal`、`duplex_client=pass`、退出 0 |
| 常驻启停 | 假 Key 启动/停止 | pid 912，待机无连接；stop 后无进程 |
| 常驻启停 | 真 Key 启动 5 s/停止 | pid 1073，0 唤醒 0 连接；stop 后无进程 |

每次测试后原厂 `mipns-xiaomi`（1282）与 `mico_aivs_lab`（2527）PID 不变。

## 适用边界与未验证事项

- 收紧后，本轮可复现的负样本（TTS、当前高噪声房间）均为 0 误唤醒；真实会话有 25 s 上限和 6 s 无人声关闭，信号能清理。
- **召回下降**：注册录音 4 段同词只有前两段能在滑窗判醒；第三段被前后长语音并段，第四段距离 14.02 高于阈值。现场可能需要说两遍，且唤醒词要相对孤立。
- 第十二轮记录过“房间持续有人说话 20 s 出现 8 次误唤醒”，本轮没有采集到同类真人负样本，无法证明该场景已经解决。需要现场录一段持续说话（不落盘模式可扩展为落盘）后再调。
- 未验证他人声音、远场、电视/音乐长播；常驻服务重启后需重新启动并传入 Key。

## 复现命令


## 产物与证据

- 二进制：`build/lx06-duplex-armhf`，SHA256 `688100848D23532CBAA22D07E882FE9BA7A00D414A63666848DC09CF08D53870`
- 设备：`/data/lx06-agent/{lx06-duplex-armhf,start.sh,stop.sh,status.sh}`，目录 0700
- 文档：`.agents/notes/reference/kws.md`、`.agents/notes/reference/context.md`
-
