# Report: Real Session Observation and Factory Wake Models (Round 5)

日期：2026-11-01。继续使用设备原厂固件和运行环境。本轮完成首个真实唤醒会话的协议观察，并完整拆开两份内置唤醒模型，未接管原厂会话。

## 结论与范围

本轮首次观察到一次真实原厂唤醒会话，并解包分析了两份内置唤醒模型。观察器只收集有限协议元数据，没有接管会话；模型解包也不代表获得可直接部署的自有模型。报告保留当时的运行证据、模型结构发现及后续未决问题。

## 首个真实对话验证

用户准备好后，开启60秒观察窗口，请用户说“小爱同学，今天星期几”。实际结果：

- 65条上行消息全部解析成功，错误0；日志记录prepare、wakeup_latency，剩余63条为stream消息。
- stream字段累计长度157440B；观察器只读取961B字段键、长度和标量，跳过音频内容，不保存录音、识别文本或dialog_id。
- 用户随后明确反馈“正常唤醒并回答”。这验证的是该次原厂查询在短时观察下正常，不是全部原厂功能回归。
- 结束检查10:42:34，目标线程State=S、TracerPid=0；mipns=1282和aivs=2527未变；Mesh支持/启用/运行均true；原厂socket保持，临时观察器已删除。

原始记录：`logs/native-observer-user-wakeup-runtime.txt`、`logs/native-observer-user-wakeup-health.txt`。本轮只观察AIVS的speech接收线程，因此这是**上行会话**证据，下行与播放时序仍需验证。日志未保存每条音频流的分布或时间戳，不能仅凭累计长度确认编码/采样格式。后续版本补充流消息计数及长度范围；不把它当作已经在本次窗口取得的数据。

## 观察器如何接入与退出

`src/native_speech_observer.c` 编译为ARM32静态程序。通过`/proc/net/unix`和FD链接确认：mico PID2527，TID2601正在recvfrom(292)，FD8对应speech.usock inode8402；TID2602/FD9是common.usock，不能混用。

程序校验目标原厂可执行文件和准确的speech socket，再通过PTRACE_SEIZE观察单个通信线程。Linux4.9 ARM64内核对ARM32目标用r12标记入口/出口，依据[内核原始实现](https://raw.githubusercontent.com/torvalds/linux/v4.9/arch/arm64/kernel/ptrace.c)，并已在同一音箱的合成进程实测，寄存器集为72B。逐字节reader仅读取需要的Protobuf元数据，所有长度限定的字节内容都跳过。未设置寄存器/内存或EXITKILL，未改socket或回调。

先在合成目标测试数据不变、正常detach后继续收包、观察器SIGKILL后目标继续收包，再做5秒原厂空闲窗口，最后才做用户配合的60秒窗口。21项协议合成检查通过，其中reader guard禁止读取合成音频区域也通过。原始结果见`logs/native-observer-test-runtime.txt`、`logs/native-probe-runtime.txt`。本报告所述观察器只读取元数据，不复制音频载荷；该实验不构成当前部署或设备操作指南。

此方法在系统调用边界暂停目标线程，有诊断开销，**不是最终常驻音频接入方案**。它先帮助核实真实接口，后续再实现能转发原厂消息并按会话分流的程序。

## 从配置标量确认录音链路

反汇编AudioRecord_Init，定位内部控制结构ELF VA=0x4254e0。仅通过/proc读取其中32B配置，不读取音频缓存：

| 环节 | 采样率 | 通道 | 每采样字节 | ALSA格式值 |
| --- | --- | --- | --- | --- |
| AudioRecord采集配置 | 48000 | 8 | 2 | 2 |
| AudioRecord输出配置 | 16000 | 7 | 2 | 2 |

格式值2按[ALSA官方格式枚举](https://raw.githubusercontent.com/alsa-project/alsa-lib/master/include/pcm.h)对应S16_LE；须与设备ALSA头/库语义保持一致。输入参数分别用于snd_pcm_hw_params_set_rate_near/channels/format；输出配置参与重采样与数据整形。这是AudioRecord中间输出，后面仍有AEC、DNN和唤醒/ASR，不应把7路原始流直接交给豆包。

进一步定位ASR cache控制结构ELF VA=0x4253f0，只读取16B控制字段，确认：+0x18回调=0x19740（mipns_asr_cb），+0x1c另一回调=0x18c48，+0x20上下文=0x5fc20，+0x24当时enable=0。静态调用点0x1eaec用r0=context、r1=缓存位置、r2=1920调用该回调。这为复用处理后的语音提供了更具体的入口；最终流采样格式仍需完整数据流论证。

证据：`native-audio-format-disassembly.txt`、`live-audio-record-config.json`、`live-asr-cache-callbacks.json`及对应32B/16B二进制。以上均为定点控制结构读取，没有扫描进程私有信息或读取缓存音频。

## 原厂模型已完整解包

两份模型的外层标记与[BINN官方类型定义](https://raw.githubusercontent.com/liteserver/binn/master/src/binn.h)相符。固件blob长度使用固定4B大端，与当前上游不同，因此实现的是**经样本验证的固件方言解析器**，不能称通用BINN实现。对容器长度、元素数、重复键、深度与最终偏移逐一核对，两份均完整消费所有字节，各46个结构节点，未靠字符串搜索截取模型。

| 部件 | mobile / level1 | mobile_clone / level2 |
| --- | ---: | ---: |
| 完整容器 | 639597B | 1678701B |
| conf配置 | 378B | 378B |
| TransitionModel | 542B | 542B |
| FST识别图 | 490B | 490B |
| am声学网络 | 637996B | 1677100B |
| prior及上下文 | 98B | 98B |

已解出配置：MFCC40维、40个mel频带、16000Hz；frames-per-chunk=5；keyword-seq=3,4；left_context=40、right_context=9；prior中10项数值。模型版本分别标注level1和level2。两者是相同识别图结构、不同声学网络及部分权重；这支持分级检测的解释，不能把它们当成已经支持两种中文唤醒词。

两份FST完整解析为OpenFST vector/standard v2：10个状态、19条边，文件490B无尾随数据；只有两条边输出非零标签：0→2输出3，8→9输出4，中间处于同一识别路径。故3,4不能直接解释为“小爱”和“豆包”两个入口。中文标签映射仍缺失，改图或改keyword-seq也不代表声学网络能识别新词。

模型结构及SHA在`temp/analysis/models/manifest.json`；每份目录有`decoded.json`、`fst-decoded.json`、`wake-graph.dot`和五个提取blob。脚本`tools/native-model-container.py`、`native-model-fst.py`可重现，原始rootfs和备份未改动。

## 下一步

优先继续恢复声学网络加载结构与标签语义，评估在原生小型运行时中增加第二关键词需要哪些模型/训练输入。另一条主线是完善会话和音频分流：先观察下行/播放时序，再验证可撤回的消息转发和豆包接口。尚未接入豆包，未实现第二唤醒词，也未宣称保留所有功能的最终改造完成。
