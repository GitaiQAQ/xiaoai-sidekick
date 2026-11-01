# XiaoAI Sidekick

Current deployment (revision 18, with rounds 19–21): Netease chart and account playlists can queue their first 20 songs in the factory player; the page shows track metadata and queue position. Tools come from one registry in the console; the voice client fetches that catalog and forwards calls. The wake acknowledgement uses the Doubao voice, answers are level-matched, and saying the wake word interrupts a reply. A session with no answer closes after 10 seconds. Audio capture and echo-cancellation findings (6 microphone channels plus 2 hardware-aligned reference channels, factory Speex AEC) are in `.agents/notes/reference/audio-pipeline.md`; the AEC prototype measures about 19 dB suppression but is not yet in the live path. Console: http://192.168.100.210:18080/.

**大模型驱动的小爱音箱副驾**

以 Sidecar 方式在音箱内运行，保留原厂小爱，扩展多模型对话、聚合音乐与米家控制。

仓库名：`xiaoai-sidekick`。

## 当前能力

- **已验证**：LX06 机内豆包实时语音、本地唤醒、灯环反馈、连续追问，以及 Web 控制台。
- **已验证**：网易云扫码登录、会话保存与恢复、搜索及账号授权单曲播放。
- **开发中**：更多模型和音乐来源、歌单与自动续播、模型工具调用、米家设备控制。

原厂小爱与副驾并存是项目设计目标。当前 SSH 补丁关闭了 OTA；完整原厂功能回归及固件恢复尚未验证。

[控制协议](.agents/notes/reference/control-protocol.md) · [项目上下文](.agents/notes/reference/context.md) · [知识索引](.agents/notes/reference/knowledge.md) · [记录规范](.agents/notes/README.md) · [音频与回声消除](.agents/notes/reference/audio-pipeline.md) · [操作日志](logs/operations.md)

最新第十六轮：音箱内 [Web控制台](http://192.168.100.210:18080/) 已上线，网易云扫码登录和会话恢复通过，用户确认《左手指月》正常播放。详见[报告](.agents/notes/reports/Web控制台与网易云登录-第十六轮.md)及[控制协议](.agents/notes/reference/control-protocol.md)。豆包tools和米家控制尚未接入。以下内容含历史状态。

## 历史开发记录

最新（第十四轮）：已接入本地“豆包豆包”唤醒、原厂“我在”与收音/思考/播报灯效；真人两轮连续追问获用户确认。修复模板偏移缺陷，仍待补录和广泛误唤醒验证。详见[第十四轮报告](.agents/notes/reports/唤醒反馈与连续对话-第十四轮.md)和[当前操作说明](.agents/notes/reference/kws.md)。最新追问超时小修已安装，未再真人验证；测试后服务已停止，无开机自启。

以下为各轮历史记录，旧轮“尚未接入”等描述不代表最新状态。

目标：保留原厂功能，在音箱内运行轻量适配程序，直连豆包云端，不依赖额外常开电脑。

## 当前入口

- [第一轮分析报告](.agents/notes/reports/LX06-豆包接入分析报告.md)
- [项目上下文与接续步骤](.agents/notes/reference/context.md)
- [操作记录](logs/operations.md)
- [SSH连接](.agents/notes/reference/ssh.md)
- [ROM 与复现](rom/README.md)
- [工具脚本](tools/README.md)
- [研究来源](.agents/notes/reference/sources.md)
- [机内唤醒词检测（第十二轮）](.agents/notes/reports/机内唤醒词检测-第十二轮.md)，操作见 [KWS 操作说明](.agents/notes/reference/kws.md)。

SSH补丁已写system0并读回核验，root@192.168.100.210登录成功。原厂boot0/boot1/system1保留。尚未接入豆包或部署新Agent。

关键实测：内核可见128MiB RAM、用户态ELF32 ARM、/data空闲约8.2MiB。共享录音短测成功；原生播放/Mesh状态可读。补丁关闭OTA，不能声称当前所有功能与原厂完全一致。

备份：backups/factory-1.94.13 为采集批次名，不代表所有槽位都是1.94.13。system1实际1.88.221，尾部读取异常，完整恢复仍未验证。不要覆盖原始资料。

目录：backups=原始镜像；firmware=固件；tools=本地工具；research=源码/解包/差异；logs=原始采集和操作记录；reports=报告；docs=上下文；src=后续程序。

rootfs解包有Windows符号链接限制，不能直接从解包目录重新制作固件。详细证据和限制见报告。

最新深入报告：[原厂唤醒链路直接分析](.agents/notes/reports/原厂唤醒链路-直接固件分析.md)，含ARM反汇编、内置模型、原生Protobuf协议证据。

最新进展：[第三轮：会话协议与实机回调验证](.agents/notes/reports/原厂会话与回调-第三轮分析.md)。已只读核对真实唤醒回调，恢复本地会话消息类型；未接管设备行为。

最新实证：[第四轮：编码边界与机内程序](.agents/notes/reports/编码边界与机内程序-第四轮.md)。31KB ARM 原生探针及隔离 socket 转发/撤回测试已在音箱通过；[源码与操作范围](.agents/notes/reference/native-probe.md)。未接管真实会话，豆包和第二唤醒词尚未实现。

当前最新：[第五轮：真实会话与唤醒模型](.agents/notes/reports/真实会话与唤醒模型-第五轮.md)。用户配合的真实唤醒上行65条消息解析全通过，用户确认正常回答；两份内置模型完整解包并恢复识别图。观察器已清理，仍未接管会话或接入豆包。

最新实现：[第六轮：原厂动态库与云端传输](.agents/notes/reports/原厂动态库与云端传输-第六轮.md)。机内C程序复用原厂curl/TLS完成证书校验，TLS探针RSS约2MiB；尚未鉴权调用模型。[本地凭据说明](.agents/notes/reference/credentials.md)，私有文件不进入报告或Git。

最新进展：[第七轮：本机认证客户端与实时传输](.agents/notes/reports/本机认证客户端与实时传输-第七轮.md)。原厂JSON解析和SSH stdin已实测；实时端点原始TLS收发通过，尚未验证认证/101升级/语音。

最新深入进展：[第八轮：实时握手与音频分支](.agents/notes/reports/实时握手与音频分支-第八轮.md)，操作见[实时客户端](.agents/notes/reference/realtime-client.md)。仍待真实凭据认证与音频接入。
最新进展：[机内唤醒词检测（第十二轮）](.agents/notes/reports/机内唤醒词检测-第十二轮.md)。纯 C 离线 MFCC+DTW 检测"豆包豆包"，模板来自 /tmp/doubao_enroll.wav，设备实测阈值 14.7784、现场 30 s 待机 0 次误唤醒；唤醒前不联网。唤醒后真实会话闭环尚未实测。

## Current status (revision 18)

The deployed player card can show title, artist, playlist and queue position. The Web page can browse Netease charts and the logged-in account playlists, and queue their first 20 tracks in the factory player. Previous/next and Doubao playlist tools are included. Web: http://192.168.100.210:18080/. Account playlist listing failed after round 18 because the provider returned a ~728 KiB response; the receive ceiling is now 1 MiB (round 19), and the listing, queue metadata, previous/next, pause/resume and search were verified live. Longer playlists do not auto-refill. Details: the revision 18 player-queue report and the round-19 playlist-listing fix report.
