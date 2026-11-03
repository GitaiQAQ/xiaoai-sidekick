# Report: Wake Handoff and General Dialogue Fix (Round 35)

2026-11-03。用户反馈紧接唤醒词问身份不回答、数学问题被当音乐。当前现场日志两次flag1/ack played后HTTP101/session_ready1，但transcript0、audio0、no_speech回待机，证明这些事件连接成功而未形成发言；不能把更早成功会话当本次验证。

代码发现：工具分支覆盖中文指令为英文音乐长引导，含歧义Point requests。按用户要求改为唯一极简通用指令：“你是豆包，用用户的语言简短回答。需要操作音箱时调用对应工具；工具未成功，不要声称成功。” 工具说明只保留各自用途/参数结果，音乐搜索明确搜索歌曲，不补身份数学规则或全局路由。

时序缺陷：原preroll300ms，ACK及TLS/session创建期间无capture读取者，紧接话可能截断/丢失。新preroll2秒；确认后立即启动唯一采集线程（同时拥有ALSA和AEC），持续到会话结束；16秒/512KiB内存环形队列，主会话只读队列，不并行调用capture。20ms节奏发送；桥接模式网络poll不再额外等20ms，避免两倍节奏导致堆积。退出/错误join后归还KWS，溢出失败关闭，不写音频文件。2500门限只控制no_speech计时，并不过滤PCM，本轮不盲改增益/门限。

合成测试tools/test-wake-handoff.c验证前滚及后续样本顺序、单一读取者、界限溢出、join停止读和重启；通过。session policy21项通过，ARM-Werror编译通过。新增--voice-fixture仅限显式合成WAV输入、8秒以内源音频、15秒会话，不开麦；只在该模拟模式收集模型回答文本。正式模式只在有界内存缓冲音频，不落录音文件，也不记录用户文字。

可审查候选：Duplex SHA256 06908fb5426451aea58732cce9ccfa9bda39f2c28723f8c7c7f9d211059e17fc。Web构建使用当前完整音乐代码，仅工具说明更新。恢复副本已收Mac private/deploy-recovery/r35/，旧Duplex a4553ad732769d6ecd8510caffc8d7ebb3be7d8e2be4830ffaf1872e10720f6e、旧Web 8c2ee543047adba9abbed7026d8c3b6038c40b5815e2f4ad0a8dcc2e93a05d8f。部署恢复仅用短暂/tmp候选，停止对应daemon后替换校验；恢复亦从Mac上传旧文件并安全stdin启动，不在设备留revision历史目录。本轮不改唤醒模型/本地回应/网易云账户或播放器队列。


## 实测结果与最终部署

两次固定合成语音用Mac系统Tingting生成（不录麦）：豆包豆包你是谁、豆包豆包一加一等于几。显式fixture模式均HTTP101/session_ready1，transcript分别13/15、audio_done1、answer_complete turns1、rc0，没有music工具调用。模型回答分别确认自己是豆包、回答1+1等于2（并额外提到二进制），证实极简提示词下这些固定输入能够形成正常回答。它不是现场弱音量或唤醒衔接的端到端验收，不能据此保证所有真人问题。

完成后再修正了退出阶段的采集所有权：会话循环结束后采集线程继续维护AEC但丢弃不再需要的样本，覆盖session.close/播放器清理期间；清理全部完成才join并归还KWS，避免结束阶段新的采集空档。发送节奏改为单调时钟的绝对20ms时间表，计入网络和处理耗时，不叠加20ms睡眠；桥接模式网络poll非阻塞。更新后的合成队列测试（包括discard维护、join、重启和溢出）再次通过，ARM-Werror通过。未重复开云模拟会话，因为该后续修改不改模拟输入/提示词/工具语义。

最终Duplex SHA256 4b03d99e04cf15af6b1d48292800f82cf04005784e586dd5858012aaf15c5ef6（最早06908…仅对应中间候选）；Web SHA256 e1e0230d624161e88f9e9138c2be6c91a1edc8785f0719af954f9810fc51f206。设备正式文件哈希已核对。最后启动PID2935，key_loaded1、AEC Capture就绪、standby3600；Web PID2359。此前一次启动SSH返回255，但随后独立查询证实PID2401已ready，因此未把传输返回码当设备状态；最终更新后启动返回0并再次查询实际PID/文件哈希。

恢复副本仍是Mac private/deploy-recovery/r35/的两份原二进制（完整旧哈希见前文）。回滚需先停止对应服务，再从Mac短暂staging上传旧二进制，核对哈希、覆盖/chmod700，Web与Duplex分别通过受保护stdin载入当前凭据启动，核对进程/ready；最后清理staging，不在设备留下历史目录。唤醒模型、local ack、页面和歌曲队列没有被该回滚更改。本轮没有实际回滚验收。

测试命令：clang -std=c11 -Wall -Wextra -Werror -pthread tools/test-wake-handoff.c；src/native_session_policy_test.c（21项）；sh tools/build-factory-wake.sh；设备固定模拟使用 /lib/ld-linux-armhf.so.3 candidate --voice-fixture fixed.wav（Key仅受保护stdin，超时25秒、内部会话15秒）。不让模拟模式读取实际麦克风。

收尾只读验证：正式语音PID2935与Web2359；Web /api/v1/state 返回voice.running1、credentials_loaded1、state standby，music.status1、loop_type1。原厂/米家/Mesh/播放器PID仍3576/3667/1585/2440/3518/3214，未进行完整功能验收。本轮/tmp/voice-fix-r35已清理（staging_removed1），MemAvailable24116KiB，/data可用5496KiB，/tmp已用9504KiB。修复后当前待机不保持云端连接；云接口连通/回答证据来自本轮两次有界固定模拟，真人紧接话仍待现场确认。
