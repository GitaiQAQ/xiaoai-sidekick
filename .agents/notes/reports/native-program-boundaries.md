# Report: Native Program and Audio Encoding Boundaries (Round 4)

时间：2026-11-01，设备最后健康检查为北京时间 10:25。对象仍为用户自有 LX06 / 1.94.13。本轮主线为原厂 ELF、实机运行与自行编写的隔离探针。

## 结论与范围

本轮在 LX06 上验证了两个隔离的 ARM32 hard-float 程序：协议合成检查通过，Unix socket 代理实验完成双向转发与恢复直连。原厂本地音频是否已经编码、采样格式和真实应用是否允许在线 socket 移位，仍没有充分证据。以下分开记录 ELF/协议实测、原厂静态分析和未验证推断。

## 本轮结论

已经完成第一份可在本机执行的 ARM32 hard-float 静态程序，大小 **31,056 字节**，在音箱上通过 20 项合成协议检查。独立的 **24,156 字节** socket 实验程序也在音箱上通过双向转发、发送者路径保持和恢复直连检查。这证明后续无需在音箱部署 Python、Node 或完整运行时，就能实现小型原生适配组件；尚未证明完整豆包接入的内存、CPU和延迟预算。

本轮没有接管真实唤醒或原厂音频，没有修改两个原厂 socket，没有停止服务、写入闪存或安装启动项。两个临时二进制和隔离测试目录均已移除。最后原厂 mipns-xiaomi=1282、mico_aivs_lab=2527，Mesh 查询仍为支持、启用、运行，两个 speech socket 均存在。这些状态检查不能代替全部原厂功能回归。

## 音频编码边界

已恢复以下原厂代码证据，地址均为本地 ELF 虚拟地址：

| 位置 | 实际证据 | 含义 |
| --- | --- | --- |
| libaivs_sdk.so 导出 OPUS_BITRATE_32K / 64K | 常量值 32000 / 64000 | SDK 中的 32K/64K 是码率常量，不能把运行参数 opus32 直接解释为采样率 |
| mico_aivs_lab 0x229a4～0x229c0 | 比较 soundai；选择 CODEC_PCM 或 CODEC_OPUS 后调用 putString | 原厂应用配置云端编码类型，不等于本地 socket 已传 Opus |
| SDK 0x10c8d0～0x10c8e0 | ENCODED_BY_CLIENT 对应 GOT 偏移 0x12a8；putBoolean 的值为 0 | SDK 默认设置“客户端已经编码”为 false |
| SDK 0x18e094～0x18e20c | getBoolean 读取该配置；为 false 则进入 0x18ef58 分支 | 编码选择真实受此配置影响 |
| SDK 0x18ef58～0x18f118 | 比较 BV32/OPUS，构造编码器；PCM/SoundAI 跳至 no need encoder | 编码动作可以发生在 AIVS SDK 内，而非 mipns 音频回调内部 |

GOT 基址通过 ELF 的 DT_PLTGOT 核对为 0x333000；引用分别由重定位表确认，避免只凭反汇编脚本的启发式常量注释判断。证据在 `temp/analysis/sdk-codec-got-references.json`、`sdk-codec-got-xrefs.json`、`sdk-encoder-config-xrefs.txt` 和 `codec-boundary-excerpts.txt`。

**当前判断：原厂本地会话流可能仍是供 SDK 编码的原始音频，这比“opus32 所以本地必为 Opus”的判断更有证据。** 但尚未从一次真实唤醒会话核对注册参数、实际编码分支和音频采样参数，因此不把 16kHz / 单声道 / S16LE 写成已验证事实。下一步仍需结合原生音频回调或有界会话元数据确认。

本轮还从 mico_aivs_lab 的下行组包核对外层 type=1：0x35adc 将 1 写入外层消息偏移 0x0c（栈基址 0x2c、字段位置 0x38），然后 0x35ae8 打包。下行发送助手 0x355cc～0x355f0 通过显式 sockaddr 调用 sendto，没有观察到额外帧头。

## 原生程序与隔离实验

源码在 `src/native_protocol.h`、`src/native_protocol.c`、`src/lx06_probe.c`、`src/unix_handover_test.c`。构建与临时运行脚本在 `tools/Build-NativeProbe.ps1`、`tools/Run-NativeProbe.ps1`；本报告记录其对应实验范围与结果。

协议解析器只输出方向、类型、流类型和音频长度，不复制音频、文字、dialog_id 或供应方字符串，不分配堆内存。它检查关键字段、长度与 varint 边界，但不是完整 protobuf schema 校验器；未解析子消息的全部必填字段仍未验证。测试输入均为合成数据，20 项检查包括合法上下行样本、逐字节截断、重复字段、方向冲突和整数溢出。

实机输出确认 Linux 4.9.61、uname=aarch64、程序指针宽度 32、ARM hard-float。ELF 离线检查为 ELF32 / EM_ARM / flags 0x5000400，且没有 PT_INTERP。静态 musl 程序不依赖修改设备 glibc。探针执行时 MemAvailable=45576kB；这不是整个 Agent 的内存实测。

socket 实验只使用随机新建的 /tmp/lx06-socket-test-XXXXXX 目录。原生两个端点模拟程序在路径被移位后，代理可双向转发并保持应用看到的发送者路径；关闭代理、恢复原路径后直连仍通过。**这只是原设备内核机制验证，不证明原厂应用一定允许在线移位。** 特别是已 connect 的 Unix socket、发送者验证、自动重建、退出清理和代理异常恢复，需要在实际接入前继续处理。

## 遇到的问题与处理

Windows 主机版编译因 Zig 生成 user32 导入库失败而终止，未称其测试成功。改为直接编译 Linux ARM 程序并在音箱执行，成功。Linux 构建曾输出 Zig 自带 musl CRT 的兼容警告，最终退出码为 0；本项目 C 源码以 -Wall -Wextra -Werror 编译。

首轮临时传输发现设备没有 base64 命令，退出清理移除了不完整临时文件。随后改用 shell printf 的八进制转义传输，远端 SHA256 与本地一致才执行；没有额外安装工具。Zig 仅解包到 PC 项目 tools，官方压缩包按官方下载元数据校验 SHA256。

45 秒被动 strace 只捕获到心跳、多播等元数据，未捕获真实唤醒会话，不能据此描述实际对话时序。观察器已经退出，没有后台录音或监听程序。

## 继续推进的工作

下一步准备有异常恢复的透明会话观察器，先在隔离端点验证正常退出、故障退出和消息完整性，再考虑原厂 socket。默认仍全部转发原厂消息，仅记录类型与长度；不能因为解析不了一个消息就丢弃它。

在此基础上核对真实唤醒时序与音频格式，再实现按会话选择豆包。第二唤醒词目前仍未实现，原厂模型的关键词 ID 也未映射。豆包接口鉴权、语音收发与打断、米家工具执行、音乐/闹钟/网关/通话完整回归均未完成。当前 SSH 补丁禁用 OTA 这一已知原功能变化也仍需在最终方案中处理。

运行原始结果：`logs/native-probe-runtime.txt`、`logs/native-socket-test-runtime.txt`、`logs/native-fourth-round-health.txt`。本轮阶段完成不等于整体目标完成，持续任务保持进行。
