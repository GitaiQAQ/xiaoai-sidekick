# Report: Factory Two-Stage Wake Engine and Isolated Invocation

## 结论与范围

继续还原原厂唤醒方案，保持“豆包豆包”需求，寻找直接复用原厂引擎的路径。本轮没有更换识别方案，也没有修复音乐自激活。

新进展：从静态调用关系推进到实机读取有效配置、独立初始化原厂两级模型、离线调用原厂推理接口，并实际观察到“一级候选 → 二级未确认 → 超时拒绝”。原厂引擎可以在单独进程中复用；当前内置权重仍不支持直接声明为“豆包豆包”模型。

## 证据与复现材料

- 静态源：原厂 `libxaudio_engine.so`，SHA256 `ae3f7ca5502f74eb88d2001253cb0c3cd4fdb9d6e860c902b44964f0aa2dc65c`。以下指令地址是 ELF VA，不是运行地址。设备补丁只改既有录音设备字符串，相关代码布局此前已核对。
- `src/native_wake_config_probe.c`：校验进程可执行路径、映射范围、两级类型后，只读固定白名单中的控制字。实机成功读取 108 B，不读音频、模型权重或凭据。对错误目标拒绝，退出码 2。
- `src/native_wake_engine_probe.c`：独立进程调用现有动态库；支持仅初始化、生成静音，以及显式离线 S16LE 文件。没有调用 `init_system`、`create_xaudio`、录音接口或注册设备回调。
- `tools/Build-WakeProbes.ps1` 构建两份探针；`tools/native-wake-dependencies.py` 从固件 ELF 导入/导出查找依赖所有者，不查询网络。
- 本地原始日志：`logs/native-wake-config-round24.txt`、`native-wake-config-guard-round24.txt`、`native-wake-engine-init4-round24.txt`、`native-wake-engine-silence-round24.txt`、`native-wake-engine-xiaoai-round24.txt`、`native-wake-engine-syscalls-round24.txt`、`native-wake-probe-health-round24.txt`。这些日志被 Git 忽略。
- 静态函数提取：`temp/analysis/native-wake-stage-round24.txt`、`native-wake-handles-round24.txt`、`native-init-system-round24.txt`、`native-wake-thread-round24.txt`、`native-wake-suspect-round24.txt`；由既有 `tools/dump-native-functions.py` 对本轮涉及的符号生成。依赖归属在 `native-wake-import-owners.json`。

构建：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/Build-WakeProbes.ps1
vendor/python-analysis/python.exe tools/native-wake-dependencies.py
```

最终配置探针 47348 B，SHA256 `2b64b92e173cddbcad6e0074001dc645b37240750e4293331a15570ec9c0e0b2`；最终引擎探针 7596 B，SHA256 `1dbca9922221e54fe78fe77cd0cc617313d9972e496645c4e6d9dfc01e8efa9c`。旧版初始化与静音探针另有哈希，不能把早期日志当成最终产物的运行记录。

## 原厂有效配置只读核验

固定链路由反汇编交叉确认：`g_handle`（VA 0x4254d4）→ engine+0x10 → wake+0x40 → wrapper+4 → PaddyEngine 首指针 → impl 的两个状态指针。每次 4 B 读取都校验在可读映射内，先验证两状态的类型字段为 1 / 2，再输出配置。

| 字段 | 状态偏移 | 一级实读 | 二级实读 |
|---|---|---:|---:|
| frames-per-chunk | +0x39c | 5 | 5 |
| min-self-loop-frame | +0x73c | 1 | 1 |
| max-self-loop-frame | +0x740 | 80 | 80 |
| acoustic-scale | +0x3bc | 0.1 | 0.1 |
| min-trans-loglikelihood | +0x744 | −1000 | −1000 |
| score-threshold | +0x3e8 | 1.0 | 1.0 |
| wakeup-wait-frame | +0x364 | 60 | 60 |
| frames-forward | +0x300 | 30 | 30 |
| keyword-seq 项数 | +0x738 | 2 | 2 |

这是运行中实际字段值，不只是字符串或注册默认值。`score-threshold=1.0` 不能当成百分比置信度，也没有证明它参与了每条有效拒绝分支。`min-trans-loglikelihood=-1000` 是宽松下界，不能仅凭存在这个检查就声称它能拒绝音乐。

## 解码器拒绝路径的位置

`ProcessEmittingViterbi` @0x577b8 的可确认分支：

- 0x57a14–0x57a24：读取 config+0x744，与当前转移的声学值比较；不满足下界则跳过该候选转移。
- 0x57a50–0x57a5c：离开当前状态时检查其自循环计数是否超过 config+0x73c。
- 0x57b88–0x57b94：在当前状态自循环达到 config+0x740 后不再沿此分支推进。
- 0x57b2c 之后沿已解析的关键词序列标注 token 的起止信息，而不是按中文字符串直接匹配。

`RiceTdnnDecodeViterbi` @0x58048 先做发射/非发射路径更新，再调用 `IsKeyword` @0x57d90；路径上的关键词 token、持续条件与计数共同决定命中。原厂也有计数，但计的是受识别图约束的神经解码路径，不能与我们对同一音乐片段反复做 DTW 的“三连击”等价。

这部分是静态代码与运行配置的对应；尚未通过改变输入逐一覆盖每个拒绝分支。

## 一级候选如何进入二级

`PaddyEngineImpl::Init` @0x553ec 分别申请两个 0x7b8 B 的状态，设置类型 1 / 2，加载不同内置模型，并创建缓存。`RiceEngineInit` 给各状态创建初始容量 600000 B 的内存池；这是初始容量，不是推理峰值内存上限。

`ProcessTask` @0x547e8 的关键路径：

1. 0x548c0 调用一级 `RiceEngineProcess`，维护主音频与 float 缓存。
2. 一级有候选后，0x54c0c / 0x54c18 重置二级解码状态，从候选前部的缓存开始复核，不是直接沿用一级命中计数。
3. 0x54d58 读取缓存，0x54db4 / 0x54e60 调用二级引擎；二级独立状态的 +0x624 命中标志决定确认分支。
4. 二级确认时，0x54f18–0x54f38 将结果结构 +4 置为 1。
5. 候选超过一级状态 +0x364 配置的等待帧数而仍未确认时，0x55074–0x550c8 清理候选，将结果结构 +4 置为 2，同时主结果位清零。

外层 `WakeupProcess` 会把内部结果 +4 复制到外层输出首字。因此本轮探针看到的输出 flag 1 对应上述确认分支，flag 2 对应候选超时。尚未完整枚举其他状态值。

`xaudio_wakeup_thread` 还区分 suspect 与 real wakeup 统计，并存在 VAD、suspect 缓存和超时回调；这些不是已经全部还原的反自激活规则。此处不推断是否还存在云端拒绝或其他播放状态判定。

## 模型装载接口边界

`PaddyEngineImpl::LoadModel` 在本机仍是返回 0 的空实现。

`RiceLoadModelBin` @0x589c0 则有真正的容器解析逻辑：状态+0x68 已有模型缓冲时解析它；为空时，按类型 1 / 2 复制对应的内置 blob。标准 `PaddyEngineImpl::Init` 对两状态都显式把 +0x68 置零，再进入初始化。因此对 `WakeupInit` 传入一个路径字符串，不代表从该文件加载新模型。

独立探针故意传入不存在的 `/tmp/lx06-probe-deliberately-absent-model.bin`，原厂仍成功报告两份内置版本；系统调用跟踪没有打开该路径。这与静态分支一致。

存在更低层的模型缓冲入口，并不等于“已有可替换的豆包模型”。自定义数据必须满足本机容器、声学网络、TransitionModel、FST、prior 与时序配置的共同约束；当前尚未制作或验证这种模型，也未在原厂进程里改指针。

## 原厂引擎独立调用结果

### 加载依赖

原厂引擎 NEEDED 表没有完整声明宿主提供的依赖。前三次 RTLD_NOW 探针分别在依赖加载处退出，日志保留；没有崩溃或操作原厂进程。按固件符号所有者核对后，显式预加载原有 `libpthread`、`libstdc++`、`libasound` 到测试进程的全局符号域，随后引擎加载、初始化和销毁成功。

特别是两个 WebRTC 重采样函数实际由厂商 `libasound.so.2.0.0` 导出，不应替换为普通桌面 ALSA 库。

### 初始化

仅初始化版本输出：两级版本分别为 `kws_s12a_1.1.7_0_level1` / `level2`，耗时 18 ms，进程峰值 RSS 6636 kB。后续带推理版本初始化约 13 ms。没有打开麦克风。

### 生成静音

1000 帧 × 160 个 S16 样本，共 10 秒、16 kHz 的程序生成静音；无实时等待地批量送入原厂 `WakeupProcess`：

- 推理批次耗时 622 ms，墙钟 RTF 0.0622。
- 非零结果标志 0；峰值 RSS 7040 kB（约 6.88 MiB）。
- 初始化和销毁正常。

这是无音频前端的静音批次，不是音乐/真人负载，不是单独二级峰值，也不是全系统 CPU 百分比，不能据此承诺真实常驻成本。

### 合成“小爱同学”

Windows SAPI 的 Microsoft Huihui Desktop 生成 16 kHz / S16 / 单声道“小爱同学”，不通过扬声器播放，而是直接把离线样本送入原厂引擎，另补 1 秒静音刷新结果。

输入 PCM 60640 B，SHA256 `28ef7b7db2720b2ee6d7636dd582c1274fe854c79093b45c993ffbbbcee4b164`。静态 ABI 从 `xaudio_wrapper_wakeup`、`WakeupProcess` 与 C++ 包装层逐一核对：大结构返回值使用隐藏 r0 指针，后续是引擎、task、PCM 与栈上的字节数；`CreateTask` 在本机合法返回 NULL。

```text
rice wakeup 1-level: frame=106, start frame=0, end frame=104, score=8.114393
engine_probe_result frame=167 flag=2
engine_probe_input=offline_fixture frames=290 samples_per_frame=160 input_ms=2900 elapsed_ms=438 nonzero_result_flags=1 confirmed_flags=0 maxrss_kb=7840
engine_probe_destroy=pass
```

一级出现候选，而二级没有确认，随后进入候选超时。候选与结果间约 61 个测试输入帧，与 60 帧等待分支相符。它验证了拒绝路径确实可达，不能当作真人召回测试通过，也不能声称原厂能识别所有合成语音。

## 设备安全范围与收尾

探针只在 `/tmp` 临时运行，文件已删除。原厂 PID1282 / 2527 仍为 sleeping，TracerPid 均为 0；正式 Duplex 哈希仍为 `cf231f7b5a6bb9ff374d51feef5e1a71c7a87a236916c40105052550332e4bcf`，未覆盖。

对独立静音探针跟踪 open/openat/connect/clone：未见 `/dev/snd` 打开或新线程；connect 仅为本地 `/dev/log`，不是云端连接。原厂进程未被跟踪。未改固件、模型、认证数据、米家或启动项，未采集现场声音。

## 限制与后续工作

已跨过“只能看符号”的阶段：能直接调用原厂模型初始化和原厂推理，能验证一级候选被二级拒绝。尚未完成原厂全部音频前端/线程分支还原、真人正样本对照，以及支持“豆包豆包”的模型构造或替换验证。音乐误激活仍未修复。

后续优先在隔离进程中继续还原模型格式与两级确认输入，建立原厂实现的正负对照；不要直接跳到生产进程 hook，也不要把静音通过、一级命中或探针返回 0 当成唤醒准确率验收。

## 关联证据

- [上一轮自激活证据](music-trigger-and-factory-wake-review.md)
- [原厂模型容器和识别图](real-session-and-wake-model-analysis.md)
- [双平面 S16 帧动态验证](audio-frame-and-authentication-control-flow.md)
- [原厂唤醒链路](factory-wake-path-firmware-analysis.md)
