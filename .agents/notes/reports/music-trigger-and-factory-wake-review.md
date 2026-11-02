# Report: 音乐自激活与原厂判醒复核（第二十三轮）

## 结论与范围

用户反馈无人呼叫时仍不断自激活，并明确要求保留“豆包豆包”。第二十二轮的两次判醒与用户短时反馈不足以证明误激活已解决；当前问题仍未修复。没有把本轮实验当成正式修复，没有更换设备上的正式程序、原厂模型、服务或固件。

本轮新增 `--wake-audit <秒>` 隔离诊断，读取 AEC 输出及硬件播放参考的同源样本，仅在内存中比较并打印标量。不连接云端、不操作播放器、不保存新采集音频。原厂链路研究依据本机 ELF、模型解包结果和既有实机记录；下述指令地址均为 ELF 虚拟地址，不能直接作为运行地址使用。

## 线上日志证据

取回现有 `/data/lx06-agent/client.log` 到本地忽略文件 `logs/music-false-wake-before-round23.txt`：

- `stage=wake_detected` 共 71 次，`session_stop=no_speech` 共 21 次。
- 候选暂停 48 次，其中 27 次记录候选确认。
- 至少 8 次出现相邻的“hits=2 → 暂停 → hits=3”且距离完全相同。
- 这些是整份日志的事件统计，不能把所有 71 次都自动标注成已独立核实的误触发；用户的无人呼叫反馈与重复自激活片段相互支持。

代表片段：

```text
win=1798 ... dist=13.9133 hits=1 hit=1 wake=0
win=1799 ... dist=13.9133 hits=2 hit=1 wake=0
music_paused_for_candidate
win=1800 ... dist=13.9133 hits=3 hit=1 wake=1
stage=wake_detected ... best_dist=13.9133
...
session_stop=no_speech turns=0
```

源码解释：`native_kws.h` 使用 1.6 秒历史窗、0.2 秒评分间隔。暂停不会清空旧特征、旧命中数或已缓存采样，因此下一个窗口可以再次裁出同一段音乐；第三次过线不等于取得了暂停后的独立确认。`kws_listen()` 还允许未暂停的音乐候选直接达到 `min_hits`；暂停有 6 秒冷却，但冷却并不禁止直接判醒。播放器状态每 5 秒轮询，暂停后仍可能沿用 15.0 阈值。这些是实际代码缺陷，不能靠继续增加重叠窗口次数修成词级复核。

日志中还有 `best_dist` 为约 1e30 的判醒记录。该值是初始化哨兵，不能当作有效声学距离或模型置信度；其状态来源尚需单独追踪。

## 原厂具体做法与证据边界

原厂没有通过 Web UI 判醒。运行入口为 `mipns-xiaomi -c /usr/share/xiaomi/xaudio_engine.conf -r opus32 -l`，核心在 `libxaudio_engine.so`。

| 层次 | 已确认的接口或算法 | 证据 |
|---|---|---|
| 音频输入 | ALSA 多麦与播放参考；原厂采集配置为 48 kHz / 8 通道，中间输出 16 kHz / 7 通道 | 既有实机控制结构与反汇编 |
| AEC | `CreatAECHandler`、`AECProcess` 调用 Speex AEC 与预处理，128 样本帧、4096 样本滤波长度、16 kHz | 0x2958c、0x29714；不能把它等同于全部前处理 |
| 特征与解码 | `RiceEngineProcess` 内调用 `RiceMfccCompute`、`RiceCmvnCompute`、`RiceTdnnProcPriorsSoilViterbi`、`RiceTdnnDecodeViterbi` | `RiceEngineProcess` @0x558f4，本轮提取到 `temp/analysis/native-rice-decoder-round23.txt` |
| 识别图约束 | Viterbi 发射/非发射路径更新，`IsKeyword` 检查解码 token 状态，命中数满足配置后置位 | `RiceTdnnDecodeViterbi` @0x58048、`IsKeyword` @0x57d90；不是对 MFCC 模板距离简单计数 |
| 两级处理 | `PaddyEngineImpl::ProcessTask` 先对一个状态对象调用 `RiceEngineProcess`，维护缓存，再对另一个状态对象处理缓存 | 一级调用 0x548c0，缓存读取 0x54d58，二级调用 0x54db4 / 0x54e60 |
| 模型 | 两份内置模型标为 level1 / level2，639597 / 1678701 字节，识别图均为 10 状态、19 条边 | 已解包容器及 FST；不代表有两种中文唤醒词 |
| 回调与反馈 | 检出后进入原厂回调、会话和设备间仲裁；`/bin/wakeup.sh` 负责提示音、灯效等反馈 | 既有原厂唤醒报告；脚本不是关键词识别器 |

已解出的模型配置：16 kHz、40 个 mel 频带、40 维 MFCC、`frames-per-chunk=5`、`keyword-seq=3,4`、`max-self-loop-frame=80`、`min-self-loop-frame=1`、`wake-durations=1`、`wakeup-duration-frames=1`。源码字符串中有 `--score-threshold`、`--acoustic-scale` 等参数注册；注册默认值不等于当前两级模型的全部有效阈值，尚未完整追踪其覆盖与判定语义。

库中有 DNN 增强和 MVDR 波束形成实现，但各待机/播放分支的准确路由及所有拒绝条件还没有完整动态验证。不能把库里存在某算法说成每次唤醒都经过它。原厂是否具有额外专门针对自播放关键词的最终拒绝逻辑，仍待核实。

### 为什么不能声称已经原样复用

原厂 Rice/Paddy 与 Sherpa 是不同引擎和模型。当前原厂模型对应“小爱同学”，没有证据证明改 `keyword-seq` 或中文字符串就能识别“豆包豆包”。FST 标签 3、4 的中文映射也不能凭数字猜测。复用原厂的 AEC 参数、状态机结构，不等于获得原厂训练数据、权重或同等误触发表现。

## 播放参考复核实验

诊断构建 SHA256：`96126C8BCBA57B0DE3929788161309C6B899EC9154E246D44542AE153CB166CF`。上传到 `/tmp/lx06-wake-audit-armhf` 并核对哈希，未覆盖 `/data/lx06-agent/lx06-duplex-armhf`。

45 秒音乐现场窗口（原厂播放器查询音量 17）：217 个评分窗口，18 个距离低于 15.0 的候选窗口。诊断对同一段 AEC 输出与硬件参考计算 MFCC/DTW 距离，尝试判断是否来自扬声器：

| 窗口 | 唤醒模板距离 | 播放参考距离 |
|---|---:|---:|
| 31 | 14.4402 | 9.9634 |
| 103 | 13.3399 | 15.0538 |
| 113 | 12.5410 | 12.2612 |
| 168 | 13.7002 | 16.9672 |

简单“更像播放参考就拒绝”的规则会漏掉部分音乐候选，因此没有作为正式判醒门控。日志为 `logs/music-reference-audit-round23.txt`。这是现场负样本诊断，不是统计充分的误触发率测量，也没有完成本轮真人召回测试。

## 现成声学模型兼容性实验

仅在 PC 下载和检查 Sherpa v1.12.15 与中文 3.3M int8 模型，文件放入忽略的 `vendor/sherpa/`，未安装至音箱。

- 本机 SAPI 合成“豆包豆包”可检出；已有用户录音原始幅度峰值约 427 / 32768，原始与临时放大版本均未检出。临时放大 WAV 已删除，原录音未修改；这个模型尚不满足召回要求。
- ARM 预编译库需要 GLIBC_2.28 的 `fcntl`、GLIBC_2.27 / 2.29 的部分数学符号；设备是 glibc 2.25。检查“patched”版 ONNX Runtime 也存在这些要求，不能把名称当成兼容性证明。
- 设备当时 `MemAvailable` 39964 kB，`/data` 可用 4936 kB。完整推理运行时和权重必须实测存储、常驻/峰值内存及延迟，不能按模型参数量估算能否运行。

这些结果排除了“直接把下载模型装上即可修复”的说法，不证明所有神经关键词模型都不可用。

## 当前设备与验证状态

开始排查时，自有语音日志已经以 `standby_stopped=signal` 结束；本轮没有重新启动这个会自激活的正式版本。原厂播放器仍在运行。正式 Duplex 哈希仍为 `cf231f7b5a6bb9ff374d51feef5e1a71c7a87a236916c40105052550332e4bcf`（本轮未覆盖）。原厂服务、米家、模型和固件未修改。

诊断代码 ARM/glibc 构建与 ELF 检查通过，隔离诊断实机运行结束。它是定位工具，不是已部署的修复。下一步应完成原厂复核/拒绝分支还原，并找到在资源限制内具备真人召回与音乐拒绝能力的词级验证器；不能再仅凭两次判醒宣布完成。

## 原厂资源与产物补测

用户询问原厂方案成本后，只读核对本地固件与设备文件，并采样原厂 `mipns-xiaomi`（PID 1282）资源。未改设备状态。原始标量记录：`logs/factory-resource-sample-round23.txt`。

| 项目 | 实测或静态读取 | 边界 |
|---|---|---|
| 一级模型 | 639597 字节，约 0.61 MiB | 库内 blob |
| 二级模型 | 1678701 字节，约 1.60 MiB | 库内 blob |
| 两模型合计 | 2318298 字节，约 2.21 MiB | 已包含在引擎库中，不能重复相加 |
| libxaudio_engine.so | 4282440 字节，约 4.08 MiB | 同时包含模型与多种音频处理代码，不是独立 KWS 最小包 |
| mipns-xiaomi | 250840 字节，约 245 KiB | 原厂宿主程序 |
| libmdspeech.so | 265688 字节，约 259 KiB | 原厂通信库 |
| 上述三个文件合计 | 4798968 字节，约 4.58 MiB | 不含其他动态库、配置或系统依赖，不是完整可分发包大小 |
| mipns RSS | smaps 汇总 13976 kB，约 13.65 MiB | 整个进程，包含共享页 |
| mipns PSS | 12672 kB，约 12.38 MiB | 整个进程按比例分摊共享页 |
| mipns CPU | 10.02 秒窗口内约单核 62.5%，四核总能力约 15.6% | 整个进程的瞬时窗口，不是纯 KWS 或最低性能要求 |

CPU 推导：进程 utime+stime 增量为 625 tick，四核 `/proc/stat` 前八项总增量为 4002 tick，故总能力占比 `625/4002`，折合单核占比再乘 4。没有把系统 load average 或 VmSize 当作 CPU / 实际内存。更早一个约 3 秒窗口折合占用更高，说明不能将本次单窗口当作稳定上限。

硬件是实测四核 Cortex-A53、暴露频率 1.2 GHz、内核收到 128 MiB RAM，用户态 ARM32/glibc 2.25。它证明原厂整套链路在这台设备上运行，不证明原厂最低配置需要四核 1.2 GHz，也没有单独测出 KWS 的 CPU/RAM。

直接复用已存在的库无需再次存储整份模型，但新增识别任务的堆、缓存、模型状态及 CPU 增量仍需单独测量；不能按共享库文件已经存在就说增量为零。

## 外部来源

- [原厂唤醒链路直接分析](factory-wake-path-firmware-analysis.md)、[原厂模型解包与真实会话](real-session-and-wake-model-analysis.md)。
- [此前音乐下唤醒修正](../implemented/bug-fix/2026-11-01-music-wake-stopped-firing.md)、[采集架构](../implemented/architecture/2026-11-01-speex-aec-integration.md)、[A/B 诊断方法](../implemented/testing/2026-11-01-capture-ab-comparison.md)。
- [Speex 官方实现](https://github.com/xiph/speexdsp/blob/master/libspeexdsp/mdf.c)：AEC 算法参考，不是本机原厂整条链路的公开源码。
- [Sherpa 官方 KWS 说明](https://k2-fsa.github.io/sherpa/onnx/kws/index.html)、[v1.12.15 ARM 构建](https://github.com/k2-fsa/sherpa-onnx/blob/v1.12.15/.github/workflows/arm-linux-gnueabihf.yaml)：备选方案来源，不是原厂实现。
