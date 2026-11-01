# Reference: 机内唤醒词检测（豆包豆包）

第十二轮实现，第十三轮收紧阈值、加上会话上限和常驻部署。纯 C11，无新依赖；唤醒前不联网，命中后才建立豆包实时会话。

## 组成

- `src/native_kws.h`：自写 cos/log/sqrt、MFCC（25 ms/400 点、10 ms 步、Hamming、512 点 radix-2 FFT、
  24 mel、log、DCT 13 维 + 差分 = 26 维）、DTW（Sakoe-Chiba 20 %）、端点裁剪、滑窗检测器、WAV 打分。
- `src/kws_templates.h`：生成物。3 个模板的特征数组与阈值 `lx_kws_template_threshold`。
- `src/native_duplex_client.c`：实时客户端与 `--kws*` 模式、会话上限、信号处理、`--daemon`。
- `tools/kws-build-templates.py`：由设备 dump 生成模板头文件。
- `tools/Run-NativeProbe.ps1`：`kws-score` / `kws-live` / `kws-capture` / `kws-replay` / `kws-standby` / `kws-signal`。
- `deploy/`：`start.sh` / `stop.sh` / `status.sh`，安装到设备 `/data/lx06-agent/`。

## 运行模式

| 模式 | 联网 | 读 Key | 说明 |
| --- | --- | --- | --- |
| `--kws-score <wav>` | 否 | 否 | 自检：分段、剔除原因、两两 DTW、模板簇、阈值、每段判决、整段滑窗扫描 |
| `--kws-capture <秒>` | 否 | 否 | 现场录音在内存中打分，逐窗打印，不落盘 |
| `--kws-live <秒>` | 否 | 否 | 现场待机监听，逐窗打印，唤醒不建会话 |
| `--kws-wav <wav> [阈值]` | 判醒后 | 是 | 回放 WAV 走同一条检测链路；stdin 无 Key 时停在 `detection_only` |
| `--kws <秒> [阈值]` | 判醒后 | 是 | 待机监听；命中后建会话、上传 300 ms 前滚、回答播完关闭，然后回到待机（`stage=standby_resumed`） |
| `--kws <秒> --daemon <pidfile>` | 判醒后 | 是 | 派生到新会话并写 pidfile，SSH 断开不会带走进程 |

两种网络模式里 Key 只在**判醒之后**才从 stdin 读取并用于 Upgrade。

## 参数

- 帧 400/跳 160/FFT 512/mel 24/倒谱 13+13。
- 滑窗 1.6 s、步长 0.2 s；连续 3 窗低于阈值判醒（`LX_KWS_MIN_HITS`）。
- 端点裁剪：峰值 −15 dB 且不低于窗底噪 +6 dB，音节间隙 ≤150 ms 合并。
- 噪声门：窗内 dB 10 % 分位作运行底噪，峰值需高出 12 dB（或峰值 ≥38 dB）。
- 长度门：跨度与模板长度相差 >±25 % 的模板跳过。
- 距离：不去均值；c0 去均值后 ×0.25 作为能量包络维；差分权重 1.0。
- 阈值：1.10 × 簇内最大距离 = **12.5048**（`src/kws_templates.h`）。

## 会话上限（第十三轮）

每次会话都有硬上限，避免进程活着就一直计费：

- `LX_SESSION_MAX`：单次会话最长秒数，默认 25；`--kws` 与直接 `--duplex` 都受此限制。
- `LX_NO_SPEECH`：判醒后连续多少秒没有有效人声就关闭，默认 6。人声阈值是单帧峰值 >1200（约 −29 dBFS）。
- 收到 SIGTERM/SIGINT/SIGHUP：置停止标志，会话循环退出并发送 `session.close`，日志出现 `session_stop=signal` / `standby_stopped=signal`。
- 回答音频 `response.output_audio.done` 或 `response.done` 后立即关闭；退出前最多等 30 s 播完缓冲。
- 关闭阶段会打印 `session_stop=max_duration` / `session_stop=no_speech` / `session_stop=signal`，便于核对。

## 常驻部署

安装位置 `/data/lx06-agent/`（ubifs，掉电保留；只新建该目录，不改原厂文件）：

```powershell
# 安装/更新（启动时不带 -Start）
.\tools\Manage-LX06Service.ps1

# 启动：Key 从 private\credentials.json 读出，经 stdin 进入 FIFO，不落盘、不进 argv
.\tools\Manage-LX06Service.ps1 -Start -Standby 60
.\tools\Manage-LX06Service.ps1 -Status
.\tools\Manage-LX06Service.ps1 -Stop

# 只验证启停机制，不消耗额度
.\tools\Manage-LX06Service.ps1 -Start -FakeKey -Standby 60
```

- `start.sh` 把 Key 写入 0600 FIFO，客户端判醒后才读；`stop.sh` 杀客户端、Key 写入者和 FIFO。
- 客户端 `--daemon` 自己 `fork`+`setsid` 并写 0700 pidfile，SSH 退出不会触发 SIGHUP。
- 没有配置开机自启；确认现场可用后再决定是否加启动项。

## 重新生成模板

```powershell
# 1) 主机侧编译开发用打分器（可选，用于快速试参数）
tools\kws-tune.ps1 -Defines '' -Tag run   # 会重建 build\dump-run.txt / build\score-run.txt

# 2) 设备上 dump（同等结果）
.\tools\Run-NativeProbe.ps1 -Kind kws-score     # 打印判决；--dump 变体见 src/native_kws.h

# 3) 生成头文件并重编
vendor\python-analysis\python.exe tools\kws-build-templates.py build\enroll-dump.txt [阈值覆盖]
.\tools\Build-NativeGlibc.ps1 -Duplex
```

阈值可在生成时覆盖（第二个参数），也可运行时用 `--kws <秒> <阈值>` 覆盖。

## 自检与负样本

```powershell
.\tools\Run-NativeProbe.ps1 -Kind kws-score          # 离线、不联网
.\tools\Run-NativeProbe.ps1 -Kind kws-replay         # 回放，判醒后无 Key 即停
.\tools\Run-NativeProbe.ps1 -Kind kws-capture -Seconds 15
.\tools\Run-NativeProbe.ps1 -Kind kws-live -Seconds 30
.\tools\Run-NativeProbe.ps1 -Kind kws-standby -Seconds 12   # --kws 12，无 Key 时只到 detection_only
.\tools\Run-NativeProbe.ps1 -Kind kws-signal -Seconds 8      # SIGTERM 后应打印 standby_stopped=signal 并退出 0

# 真实 Key：回放触发唤醒 -> 建会话 -> 前滚 -> 关闭（不需要对着设备说话）
.\tools\Invoke-DoubaoSmoke.ps1 -Duplex -KwsWav -Seconds 8
# 真实 Key：现场待机，说"豆包豆包"后进入会话
.\tools\Manage-LX06Service.ps1 -Start -Standby 60
```

## 第十三轮实测

- 离线 53 条中文 TTS 负样本（34 句日常话术 × 语速变体，16 kHz 单声道）：0 次误唤醒，最低距离 19.68。
- 现场高噪声（峰值 52–58 dB，底噪 34–46 dB）待机 20 s：92 窗、0 次判醒，最低距离 17.84。
- 同一环境 15 s 待机：67 窗、0 次判醒，最低距离 15.29。
- 回放注册录音：2.07 s 判醒（距离 0.0001），无 Key 停在 `detection_only`，退出 0。
- 真实 Key 回放闭环：判醒 → 101 → `session_ready=1` → 前滚 4800 采样 → 299 帧上行 → 6 s 无人声触发 `session_stop=no_speech` → `session=pass`，全程未播放音频，退出后无进程、无连接、无临时文件。
- SIGTERM：8 s 时发送，打印 `standby_stopped=signal`，`duplex_client=pass`，退出 0。
- 原厂 `mipns-xiaomi`（1282）与 `mico_aivs_lab`（2527）每次测试后 PID 不变。

## 已知限制

- 阈值 12.5048 下，注册录音的 4 段可用同词只有前两段在滑窗里判醒；第三段被前后长语音并段、滑窗裁不出来，第四段距离 14.02 高于阈值。现场可能要说两遍。
- 现场其他人声与本人同词的 DTW 距离分布重叠（10.5–14.7 vs 8–14）。本轮用更严阈值 + 连续 3 窗把现场误唤醒压到 0，但还没有用“真人持续说话”的录音复测；需要一段现场负样本才能继续调。
- 未验证他人声音、远场、电视/音乐长时间播放。
- 唤醒词需要相对孤立地说；前后紧贴长句时可能裁不出来。
- 常驻服务未配置开机自启，Key 只在启动时经 FIFO 传入。
