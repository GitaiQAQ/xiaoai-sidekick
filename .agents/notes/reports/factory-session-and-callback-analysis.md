# Report: Factory Session Routing and Callback Recovery (Round 3)

设备：LX06 / 1.94.13；时间：2026-11-01 上午。
本轮继续分析原厂二进制，并用进程内少量指针读取核对，不依赖社区KWS实现。

## 结论与证据范围

本报告记录 LX06 / 1.94.13 上对原厂唤醒回调、回调参数、Unix datagram 消息分支和唤醒模型的静态分析及少量只读指针核对。确认运行中的第一唤醒回调地址与静态注册地址一致，并恢复了本地消息的 8 个上行类型和 11 个下行分支。完整唤醒时序、音频编码和豆包分流仍未验证；本报告不表示已接管原厂会话。具体限制见“当前仍需验证的项目”。

## 核心进展

1. 已把唤醒回调从静态候选升级为实机指针验证：运行中实际注册地址为 0x19528，对应 mipns_wakeup_1_cb。
2. 恢复了本地语音消息的8个上行type和11个下行分支；生成可阅读的重建.proto文件。
3. 明确本地asr_partial没有识别文字，只有final和length；不能直接靠这一层取得用户说的话。
4. 找到多设备唤醒仲裁与原始唤醒/确认唤醒之间的关系；分流不能简单把第一回调吞掉而忽略多设备行为。
5. 没有注入代码、写进程内存、改启动项、发伪造消息或启用麦克风上传。原拾音/语音进程PID仍1282/2527，Mesh运行状态正常。

## 1. 真实唤醒回调和注册路径

原厂mipns-xiaomi的main在0x13bc4～0x13c00准备初始化参数：r2=0x19740、r3=0x19528，配置参数在r1；转入本地包装函数0x288e4，最终于0x28a64跳转init_system。

结合init_system中的日志格式及参数转存，可确定这两个地址分别作为asr_cbf和w_cbf传入：

| 地址 | 函数名证据 | 用途与当前把握 |
|---:|---|---|
| 0x19528 | 该地址函数体引用 mipns_wakeup_1_cb 日志标签 | 音频库真实注册的第一唤醒回调，已运行指针核对 |
| 0x19740 | 该地址函数体引用 mipns_asr_cb 日志标签 | 数据回调；处理缓冲区和长度，不是返回识别文字 |
| 0x18e7c | mipns_wakeup_2_cb 日志标签；main另一路传入 | 后续唤醒/分布式链路回调，未做运行指针核对 |

init_system随后调用create_xaudio。create_xaudio在0x20ac0调用xaudio_register_callback；后者把回调和用户上下文保存到内部唤醒对象的+0x64和+0x68。

### 运行内存核对（只读16字节）

先从当前/proc/1282/maps取得libxaudio_engine装载地址，再根据ELF重定位定位导出全局g_handle：

- g_handle得到引擎对象：0x000cfd30；
- 引擎对象+0x10得到唤醒对象：0x003263f0；
- 唤醒对象+0x64得到函数指针：0x00019528；
- +0x68得到用户上下文：0x0005fc20。

它与静态main中传入的回调地址完全一致。操作仅读取了4+4+8字节的指针，没有读取音频缓冲、账户信息或扫描进程内存，更没有改写这些地址。
地址是该次运行的快照，后续进程重启应重新定位，不能把堆地址写死。

## 2. 回调ABI已恢复到什么程度

原厂库唤醒线程0x1f674～0x1f684：
- 从唤醒对象+0x64取函数指针；
- 从+0x68取用户上下文放入r0；
- r1置1；
- 浮点值置s0；
- blx进入回调。

回调内部把s0按float接收，并按日志含义作为唤醒角度使用。候选签名可记为 wake_cb(context, flags, angle_float)，其中整数/指针用r0/r1，float用VFP s0。这是ARM硬浮点调用约定的重要细节，不能按普通可变参数C函数随意拼接。

mipns_asr_cb入口把r0保存在sb，r1保存在r7，r2保存在r4；后续将r1当缓冲指针、r2当长度，按块复制和向下传递。因此候选为 data_cb(context, buffer, byte_count)。

这里只恢复已证实的参数角色，尚未证明每条返回路径的返回值约定、全部flags位和音频编码；不提供一个可以不经验证直接调用的init_system完整C原型。

## 3. 为什么还有wakeup_1和wakeup_2

wakeup_1不是简单“播放提示音并上云”：它先检查服务/静音状态，调用本地事件函数，再通过0x266ec包装调用mdspeech_wakeup_info_send。

mipns中的初始化代码带有原厂源文件路径mipns/thirdparty/distrib_wakeup.c；libmdspeech里还有设备唤醒信息、仲裁决策、唤醒抑制等实现线索。main通过另一初始化路径传入wakeup_2回调；它也执行状态检查，再转入本地唤醒事件。

需要纠正上一轮架构图中过于简化的表述：libmdspeech有明显的多设备唤醒/协同职责；本地speech Protobuf的组包、收发调用则直接位于mipns/aivs_agent.c对应的可执行代码中，不能把二者统称为一个简单的“会话转发库”。

这些证据支持继续验证“原始唤醒 -> 仲裁 -> 确认唤醒 -> 建立语音会话”的链路，但完整时序仍未通过一次受控实际唤醒抓取证明。不要把_1/_2直接当成两种不同唤醒词。

对于保留全部原有功能，应研究在确认这台设备获选之后、建立小米语音会话之前分流。直接在最早回调处吞掉原始事件，可能破坏多台设备的唤醒抑制与其他原生状态。

## 4. 本地消息8个上行type

type值从组包指令存储位置结合原厂protobuf-c字段偏移确认；不是按名称猜数字。

| upward.type | 含义 | pack调用点 |
|---:|---|---:|
| 0 | register_request | 0x149c8 |
| 1 | stream_prepare_request | 0x14c58 |
| 2 | stream_cancel_request | 0x14ec4 |
| 3 | stream_transmitting | 0x1515c |
| 4 | wakeup_latency | 0x1548c |
| 5 | voip | 0x15714 |
| 6 | tts_finish | 0x1597c |
| 7 | multi_channel_upload_request | 0x15bec |

这些发送路径中外层speech_message.type都存为0，upward指针指向对应消息。
注册子消息含speech_vendor和可选speech_codec；音频子消息含type和bytes stream。

0x14768发送辅助函数在0x148c4调用sendto，r1/r2直接来自传入的组包缓冲区/长度；本地socket创建指令为AF_UNIX=1、SOCK_DGRAM=2。已检查的这个发送路径未看到另加长度前缀或帧头，因此不应预设一个社区WebSocket帧格式。此结论只适用于该路径，不代表其他小米协议也如此。

## 5. 本地消息11个下行分支

接收代码在unpack后读取downward指针和其type，通过0x140dc的11项跳转表分发。各case引用明确的原厂日志字符串，已结合跳转目标复核：

| downward.type | 原厂行为标签 | 分支地址 |
|---:|---|---:|
| 0 | register response | 0x14108 |
| 1 | prepare response | 0x14168 |
| 2 | stop capture | 0x141d0 |
| 3 | expect speech | 0x14220 |
| 4 | truncation notification | 0x14314 |
| 5 | dialog finish | 0x14364 |
| 6 | asr timeout | 0x143dc |
| 7 | asr partial | 0x1442c |
| 8 | disable voice wakeup | 0x14564 |
| 9 | enable voice wakeup | 0x145b4 |
| 10 | disconnected | 0x14604 |

这些是后续适配生命周期所需的具体控制事件，但存在接收分支并不等于发送任意事件都安全。本轮未发送任何这些控制包，也没有假定可以通过伪造来关闭/恢复服务。

### asr_partial的重要区别

该子消息只有：
- final: bool
- length: uint32

没有string text。日志asr partial: %s, %d中的%s不能当作识别文字字段的证据。期待直接读取这条本地消息得到整句文字的方案不成立。

它仍可用于判停/同步状态。要取得文字，应在更上层查找实际识别结果；或在豆包路线中直接把音频交给豆包识别。

## 6. 对机内豆包分流的具体方案

优先研究边界：确认唤醒后的原生会话准备请求，以及紧随其后的音频流，而不是事后读取聊天日志再杀进程。

预期状态机：
1. IDLE：保留原厂唤醒、静音、网关与按键。
2. WAKE_CANDIDATE：走原厂已有的唤醒和多设备检查。
3. ROUTE_SELECTED：确定本轮归属小爱或豆包。第二关键词能力仍需独立验证；目前还没有给真实唤醒事件增加“豆包”来源。
4. NATIVE_SESSION：原生注册/prepare/stream/cancel/finish链保持原样。
5. DOUBAO_SESSION：机内适配层消费合适格式的音频、连接豆包并管理输出；通过受限工具调用原生设备能力。
6. EXIT/INTERRUPT：取消本轮云端与本地输出，正确恢复唤醒、音乐及原生状态。

不可直接照做的捷径：
- 仅把socket重定向到一个新进程：注册、准备响应、dialog id、期望继续说话和结束标志都需正确处理，否则可能卡住原厂拾音。
- 把现有音频bytes当16k PCM上传：原厂运行参数含opus32，编码、帧边界和原始/处理后音频来源仍需核对。
- 关闭小米语音进程：会影响原有回退、技能、通话和状态管理。
- 改一个回调指针就宣布完成：需要线程、ABI、状态和故障恢复验证，本轮未进行这种修改。

“两个助手共存”的机内会话适配边界已经更具体；“分别喊小爱和豆包”的第二关键词问题尚未解决，不能把两者合并宣称完成。

## 7. 工程产物与证据

- temp/analysis/live-callback-verification.json：实际回调指针与静态地址匹配。
- live-engine-handle.bin、live-wakeup-context.bin、live-wakeup-callback.bin：仅16字节读取快照。
- callback-abi-excerpts.txt：main传参、唤醒/数据回调和包装函数的精确指令片段。
- wire-builder-excerpts.txt：上行组包与sendto调用片段。
- upward-types.json、downward-dispatch-table.json：消息值与证据。
- native-speech-reconstructed.proto：从原厂描述符重建的proto2结构，属于分析产物，不是小米源代码；尚未与实收报文端到端互通验证。
- logs/native-session-audio-map.txt：本次装载映射。
- logs/native-session-health.txt：进程和Mesh健康快照。
- tools/native-session-evidence.py、native-downward-types.py、native-session-artifacts.py、dump-native-functions.py：可复现分析脚本。

备注：.ARM.exidx粒度不足以自动恢复这些C函数边界；相关尝试已记录为限制，不把粗粒度范围当成真实函数。报告中的函数名依赖局部原厂日志引用、控制流及调用点交叉判断。

## 8. 当前仍需验证的项目

- 第二唤醒词：原模型标签/解码器可扩展性，以及是否应选独立轻量检测器。
- 实际唤醒期间，仲裁、prepare、数据流、stop/finish的时序和对应数据格式。
- 处理后音频是否能直接复用，及其AEC/远场质量。
- 豆包鉴权、实时语音调用和工具调用权限，未使用用户凭据。
- 双助手切换、米家动作去重、闹钟/通话抢占和断网恢复。

本轮的成功是回调定位与会话控制协议恢复，不是豆包已接管。所有改造决策应以不破坏原厂链为前提逐步验证。
