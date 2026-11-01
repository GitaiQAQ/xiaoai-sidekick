# Reference: Claude 接续原厂唤醒逆向

## 当前任务

深入分析并解决音乐播放时无人呼叫却反复激活的问题。用户明确保留“豆包豆包”，优先还原和复用原厂方案。最终 Agent 在音箱内运行，保留原厂小爱、米家与网关，不依赖外部常开服务器。

**问题尚未修复。** 两次日志判醒、静音零触发、探针返回成功，都不能当成真人召回或音乐误触发验收。

## 先读这些文件

先切到克隆后的项目根目录，阅读根 `AGENTS.md`，然后读：

1. [项目上下文](context.md)、[操作日志](../../../logs/operations.md)、[来源](sources.md)。
2. [第二十三轮：自激活证据](../reports/音乐自激活与原厂判醒复核-第二十三轮.md)。
3. [第二十四轮：原厂两级判醒与独立调用](../reports/原厂两级判醒与独立引擎调用-第二十四轮.md)。
4. [第五轮：模型解包与识别图](../reports/真实会话与唤醒模型-第五轮.md)。
5. [第九轮：双平面 S16 帧验证](../reports/音频帧结构与认证控制流-第九轮.md)。

改笔记前读 [Notes 规范](../README.md)；编辑 implemented 内容还需遵循其 `AGENTS.md`。历史报告的结论不代表当前状态。

## 已确认的边界

- 现有 MFCC+DTW 模板会将音乐误当唤醒。1.6 秒窗每 0.2 秒重评分，候选暂停不清旧特征/命中/缓存，能重复确认同一段音乐；冷却期间也可能直接判醒。
- 原厂 Rice/Paddy 使用声学网络、MFCC/CMVN、Viterbi/FST 与独立的两级复核。内置模型目前对应“小爱同学”，不等于已支持“豆包豆包”。
- `PaddyEngineImpl::LoadModel` 在本机是空实现；标准初始化会将两级 state+0x68 清零，按 type1/2 选内置模型。底层模型缓冲入口存在，但模型格式和目标词权重尚未解决。
- 固定字段探针只读原厂进程 108 B，两级 chunk5、self-loop1/80、acoustic-scale0.1、wait60、frames-forward30。score-threshold1 的完整用途仍未追清。
- 原厂模型初始化与推理已在隔离进程直接调用成功；需显式预加载原有 pthread、stdC++、ALSA。厂商 ALSA 还导出 WebRTC 重采样符号，不能换成普通桌面库。
- 10 秒生成静音批量推理 622 ms、峰值 RSS7040 kB、非零结果0。这不是音乐负载的稳态 CPU 或准确率。
- 离线合成“小爱同学”触发一级候选 score8.114393，二级未确认，随后 flag2 超时。它证明拒绝路径可达，不是真人正例通过。
- 备选 Sherpa 在 PC 上能识别合成豆包，却未识别现有真人录音；检查的 ARM 预编译库要求 glibc2.28/2.29，而设备为2.25。未部署。

## 接下来具体做什么

1. 从 `src/native_wake_engine_probe.c` 及第二十四轮列出的函数地址继续，核对两级输入缓存、确认/超时状态、score-threshold 的有效路径和 suspect/VAD 分支。
2. 继续还原模型容器中的声学网络、TransitionModel、FST、prior 及标签语义，评估在隔离进程使用自定义模型缓冲。不要猜测修改标签3/4就能改词。
3. 建立原厂实现的可信正负对照；当前仅有静音负例和被二级拒绝的合成词，尚缺真人正例验证。
4. 找到支持“豆包豆包”的可靠模型/确认实现后，再评估生产接入。不要先在生产进程 hook，也不要只靠放宽阈值或增加重叠窗口数。
5. 音乐无人说话测试和真人召回测试分开，记录音量、站位、时长、误暂停及云端误建连。每次实质进展同步上下文、操作日志和报告。

## 构建与检查

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .agents/notes/scripts/verify-agent-notes.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tools/Build-WakeProbes.ps1
vendor/python-analysis/python.exe tools/native-wake-dependencies.py
powershell -NoProfile -ExecutionPolicy Bypass -File tools/Build-NativeGlibc.ps1 -Duplex
```

第二十四轮两探针构建、实机实验和文档门禁已通过。通用 `dump-native-functions.py` 会覆盖 `focused.txt`，应保留命名副本。不要执行 `Manage-LX06Service -Start` 当作状态检查。

## GitHub 包含什么、缺什么

仓库保存源码、脚本、报告、配置说明和公共资料固定点。`private/`、`tools/local/`、录音、设备分区备份、固件二进制、`vendor/`、`temp/`、`build/` 与原始运行日志不发布。

同一台电脑可继续使用已有材料；换机器克隆后，按 [ROM 说明](../../../rom/README.md)、[工具说明](../../../tools/README.md)、[来源](sources.md)恢复公共输入与工具链。只有 GitHub 仓库不能凭空恢复私有实机快照和录音，缺失时明确说明，不能伪造证据。

原机器工作区为 `C:\Users\gitai\Desktop\lx06-agent`。原始日志和反汇编路径均在逐轮报告中；它们是本地忽略资料，不是坏链接或漏提交源码。

## 设备状态与约束

交接检查时原厂 PID1282/2527 存活、TracerPid0；Web 存活；自有语音进程不存在，pid 文件过期。正式 Duplex 哈希仍为 `cf231f7b5a6bb9ff374d51feef5e1a71c7a87a236916c40105052550332e4bcf`。这些是当时快照，接手须重新只读核验。

不重复安装驱动，不输出或提交账号/密钥/设备认证数据。backups/ 不改，镜像修改输出 firmware/；设备写入前准备具体产物和恢复方案。没有改固件、模型或原厂服务的隐含授权。

CLI 拆分只是[待实施提案](../proposed/architecture/2026-11-01-capability-boundaries-and-binary-packaging.md)，当前不要顺手重构。
