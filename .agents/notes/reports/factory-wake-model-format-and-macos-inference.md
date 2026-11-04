# Report: Factory Wake-Model Format and macOS Reference Inference

## 结论与研究范围

用户确定路线：逆向原厂 Rice/Paddy 方案，按原厂格式装载我们自己的“豆包豆包”小模型；先在 macOS 验证，验证通过后再装进音箱。本轮聚焦模型格式。

结论：

- **声学网络（am blob）的二进制格式已完整还原。** 解析器逐字消费两份模型的全部字节，序列化器能逐字节还原原文件。按这个格式，我们可以写出自己的网络。
- **网络结构确定**：标准 Kaldi 风格 TDNN，34 个组件。一级每层 96 维，约 15.9 万参数；二级每层 160 维，约 41.9 万参数。输出 10 个 pdf，经 logsoftmax。
- **macOS 上有了参考前向推理和 Viterbi**（numpy）。对 Apple 婷婷 TTS 的“小爱同学”，一级模型在 5 条正例中有 4 条走通关键词路径。8 条负例全部未走通，其中包括“小白同学”“小海同学”“你好小爱”“晓得同学们”“豆包豆包”。前端特征尚未与设备对齐，这只是合理性证据，不是准确率。
- **自定义模型缓冲的装载入口已确认**：导出函数 `RiceEngineInit` 在 state+0x68 非空时直接解析该缓冲。隔离探针可以自建 state 调用它，不需要 hook 生产进程。尚未上机验证。
- 本轮没有写设备，没有改固件、模型或服务。问题（音乐误激活）仍未修复。

## soil 声学网络二进制格式（实测与反汇编）

静态源与前几轮相同：`libxaudio_engine.so`，SHA256 `ae3f7ca5…dc65c`。地址为 ELF VA。

### 读取流程

- `SoilLoadModel` @0x6da40：模式 4/6/7 下，先读组件数（0x6ec84，二进制模式下为 1 个 int32），再对每个组件调用 0x6f028。
- 0x6f028 依次做三件事：
  1. 读类型码（二进制模式下为 int32）；
  2. 按 23 项跳转表分派到各组件的 Load；
  3. 在 0x6eeb8 读输入数 n 和 n 个输入组件编号（类型 1 `input` 跳过这一步）。
- 读取原语是 0x6ee00（int32）、0x6ede0（float）和 0x6edc4（指针直接指向缓冲，不拷贝）。权重在原缓冲中原地使用。

### 类型码

由文本名映射函数 0x6e3c4 得出：

| 码 | 名称 | 码 | 名称 | 码 | 名称 |
|---|---|---|---|---|---|
| 1 | input | 9 | relu | 17 | normalize |
| 2 | gru | 10 | tanh | 18 | stats_pooling |
| 3 | lstm | 11 | add | 19 | softmax |
| 4 | convolution | 12 | reducesum | 20 | embedding |
| 5 | max_pooling | 13 | unfixedscale | 21 | reshape |
| 6 | splice | 14 | sigmoid | 22 | alphaconvolution |
| 7 | padding | 15 | pnorm | 23 | disabled |
| 8 | affine | 16 | fixedscale | | |

### 本模型用到的组件字段（二进制分支）

| 类型 | Loader | 字段（int32，除注明外） |
|---|---|---|
| padding | 0x85dac | left_context, right_context, padding_type, zero |
| splice | 0x832f0 | in, out, 5 个标志, has_offsets；有 offsets 时再读 n, f44, offsets×n |
| affine | 0x8c494 | in, out, W（float，out×in 行主序）, bias（float×out） |
| relu | — | 无字段 |
| normalize | 0x86030 | size, need_divide, need_minus, need_reverse, target_rms（float）, vector_source |
| softmax | 0x83d38 | dolog, logsoftmax |

`affine` 文本模式的源码断言检查 col==in、row==out。权重按 out 行、每行 in 个 float 依次写入，所以二进制布局是 W[out][in]。

数值上也做了检查：把 W 当成 in×out 来算，负例的平均最大后验从 0.99 降到 0.73，Viterbi 代价离谱。这是推断级佐证。

### 两份模型的实际结构

```text
input > padding(40,9) > splice[-2..2] 40→200 > affine 200→D > relu > normalize
  > (affine D→D > relu > normalize) ×2
  > splice[-6,-3,0,3] → affine 4D→D > relu > normalize > (affine D→D > relu > normalize) ×2
  > splice[-32,-23,-14,-5,4] → affine 5D→D > relu > normalize > (affine D→D > relu > normalize) ×2
  > affine D→10 > softmax(logsoftmax=1)
```

| | 一级 `mdata_wakeup_mobile` | 二级 `mdata_wakeup_mobile_clone` |
|---|---|---|
| 隐层 D | 96 | 160 |
| affine 参数 | 159274 | 419050 |
| am 总字数 | 159499 | 419275 |
| 上下文 | 左 40、右 9（与 prior 中的 left/right_context 一致） | 同左 |

- 所有组件都只接收前一个组件的输出，是纯链式结构。
- normalize 的参数为 need_divide=1、target_rms=1.0、vector_source=0，等同 Kaldi NormalizeComponent。
- normalize 在 0x86dcc–0x86dfc 的计算是 `scale = 1/sqrt(max(sumsq·rms/dim, 1.3553e-20))`，与该语义一致。

### 序列化器验证

`tools/native-model-am.py` 的 `parse` 与 `emit` 互为逆操作。脚本会对两份原厂 am 字节做 parse → emit，要求结果与原字节逐字节相同，不同则报错退出。本轮两份都通过。

这只证明我们的格式理解与文件一致，不证明原厂运行时会接受我们生成的新文件。后者要在设备隔离探针中确认（见第 4 节）。

## 其他模型部件

- **TransitionModel**：Kaldi 二进制格式。共 10 个 phone，每个 1 个 HMM 状态。Triples 为 (k, 0, k−1)，即 pdf = phone − 1。
- **FST**：OpenFST vector/standard，10 个状态、19 条弧。
  - 关键词路径的输入 transition-id 是 18,10,4,6,14,12,18,16，对应 phone 9,5,2,3,7,6,9,8。
  - 垃圾/静音弧的输入是 2、8，对应 phone 1、4。
  - 输出标签 3、4 分别在首弧和末弧上，与 conf 中的 keyword-seq=3,4 对应。
  - 弧权重：一级 2.25，二级 4.0。
- **prior**：10 个 log_priors，left_context=40、right_context=9。
- **conf**：MFCC 40 维、40 个 mel 滤波器、16 kHz、low=20、high=−200、use-energy=false，每块 5 帧。
- phone 9 在关键词中出现两次，与“小爱同学”中“x”出现两次相符。8 个音素按“x / iao / … / ue”对应仍是推断。

## macOS 参考推理

新增 `tools/native-model-forward.py`，用 numpy 实现：

- **MFCC**：Kaldi 默认的帧长、帧移、预加重、povey 窗、梅尔刻度、DCT 和倒谱提升；用整句均值做 CMVN。
- **网络前向**：严格按 am-decoded.json 中的组件实现。
- **Viterbi**：按 FST 计算 `声学分 = 0.1 × (log 后验 − log prior)`。
- **指标**：输出“到达关键词末状态的代价 − 垃圾状态代价”的最小值。负值表示关键词路径更优。这是自定义的对照指标，不是原厂的 score。

测试音频用 macOS `say` 合成（婷婷/美佳，转 16 kHz S16 单声道），首尾各补 0.5 秒静音。日志在 `logs/native-model-forward-round25.txt`，音频在 `temp/analysis/r25-tts/`，两者都被忽略。

| 样本 | 一级 margin | 二级 margin |
|---|---:|---:|
| 小爱同学（默认语速） | −12.69 | 10.14 |
| 小爱同学（160 语速） | −14.34 | 10.14 |
| 小爱同学（220 语速） | 2.78 | 15.75 |
| 小愛同學（美佳） | −10.17 | 18.96 |
| 小爱同学，今天几点了 | −13.64 | 10.23 |
| 小白同学 | 3.44 | 24.78 |
| 小海同学 | 7.72 | 24.06 |
| 你好小爱 | 12.85 | 27.40 |
| 晓得同学们 | 11.29 | 24.93 |
| 豆包豆包 | 17.25 | 27.56 |
| 一般句子 | 14.69 | 29.13 |

解读：

- 一级模型对 TTS“小爱同学”整体可分，相近词也被拒绝。这说明我们对网络、TransitionModel、FST 和 prior 的理解大体正确（推断）。
- 二级模型对所有 TTS 都没有走通关键词路径。第二十四轮设备实测中，二级同样拒绝了 SAPI 合成的“小爱同学”，两者方向一致。但本地前端还没对齐，不能下结论。
- 对齐前不应拿这些数值调阈值。

### 已知未对齐项（需要设备金标准）

- 原厂 `RiceCmvnCompute` @0x5be44 是滑窗累计的在线 CMVN，本地实现用的是整句均值。
- 原厂可能有抖动：`RiceFeatureInitialize` 中有 rand/logf/sqrtf/cosf 序列。
- 窗函数、提升系数、FFT 长度以及 povey 窗的具体常量尚未逐一核对。
- 原厂的 score 计算方式，以及 `IsKeyword`、`score-threshold` 的有效用法，尚未在本地复刻。

## 自定义模型装载路径（静态分析）

- `PaddyEngineImpl::Init` @0x553ec 在 0x55478/0x55488 把两个 state 的 +0x68 清零，再调用 `RiceEngineInit`。因此在标准初始化路径上无法传入自定义模型，这与第二十四轮系统调用跟踪的结论一致。
- `RiceEngineInit` @0x55600 是全局导出函数，参数为 (r0, state)，r0 原样透传自 Init 的参数。它先创建 0x927c0 B 的内存池，再调用 `RiceLoadModelBin(r0, state)`。
- `RiceLoadModelBin` @0x589c0 在 0x589d8 读取 state+0x68：
  - 非空时，直接把它当 BINN 容器解析；
  - 为空时，按 state+0x3b8 的类型 1/2 分配内存并拷贝内置模型（一级 0x9c26d B，二级 0x199d6d B）。
- `RiceEngineProcess` @0x558f4 同样是全局导出函数。

可行的隔离方案：探针自己分配 0x7b8 B 的 state，设置 +0x3b8 的类型、在 +0x68 填入我们的容器缓冲，再调用 `RiceEngineInit` 和 `RiceEngineProcess`。这样完全绕开 Paddy 和生产进程。`RiceEngineProcess` 的栈参数（Paddy 在 0x548a0–0x548c0 传入了 3 个栈参数）还需要逐一核对。

## 自有小模型仍需完成的工作

已具备：

- 网络格式与读写器；
- TransitionModel、FST 和 prior 的语义；
- 容器解析器；
- macOS 参考前向推理；
- 绕过 Paddy 的装载入口。

待完成，按依赖顺序：

1. 容器（BINN 方言）写出器及逐字节往返验证。工作量小。
2. 设备金标准：用隔离探针把原厂内置模型的字节通过 +0x68 原样喂回，确认与内置路径结果一致。再对同一批 fixture 记录一级/二级 score，用来校准本地前端和打分。这一步需要上机运行 `/tmp` 下的隔离探针，不写持久存储。
3. “豆包豆包”的图与标签设计。按“d ou b ao d ou b ao”组织 phone，加上垃圾和静音单元；FST、TransitionModel 和 prior 按我们的 pdf 数生成。
4. 训练：同构 TDNN（PyTorch 或 numpy 均可，结构很小），帧级标签需要强制对齐。
   - 数据：用户的“豆包豆包”真人录音，最好经设备 AEC 链路采集；负例为语音和音乐。
   - 合成语音只能作补充。
5. macOS 验证：正例召回、音乐与相近词负例，然后在设备隔离探针中复测。通过后，再制定生产接入与回滚方案。

**唯一的硬依赖是训练数据。** 格式和装载在技术上已经打通。

## 复现材料

- `tools/native-model-am.py`（重写）：soil 二进制网络的解析与序列化，带逐字节往返校验；输出 `temp/analysis/models/*/am-decoded.json`。
- `tools/native-model-forward.py`（新增）：macOS 参考 MFCC、前向推理与 FST Viterbi。
- `temp/analysis/r25-soil-component-loader.txt`：0x6f028 分派函数的反汇编。`temp/analysis/r25-RiceLoadModelBin.txt` 是上一阶段的产物。

工具 SHA256：

| 文件 | SHA256 |
|---|---|
| `native-model-am.py` | `70f46103…3c28c96`（am-decoded 一级 `13db9c91…`，二级 `0238d9e7…`） |
| `native-model-forward.py` | `b4ea9489…53fc8` |

原厂模型字节和解码出的权重都只放在被忽略的 `temp/` 下，不入库，也不发布。

## 安全范围与未验证事项

- 本轮的设备操作只有一次只读检查：21:39 核对原厂 PID 1282/2527 存活、TracerPid 为 0，正式 Duplex 哈希仍为 `cf231f7b…332e4bcf`。
- 没有上传、运行或写入任何文件，也没有采集现场声音。
- Web 进程（PID 1212）二进制的 SHA256 为 `4f8c5792…24b1ce`，与记录不一致，来源待查。

## 关联证据

- [第二十四轮：原厂两级判醒与独立调用](factory-two-stage-wake-engine.md)
- [第二十三轮：自激活证据](music-trigger-and-factory-wake-review.md)
- [第五轮：模型容器与识别图](real-session-and-wake-model-analysis.md)
