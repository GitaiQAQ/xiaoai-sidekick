# Report: Wake Handoff and General Dialogue Fix (Round 35)

2026-11-03。用户反馈紧接唤醒词问身份不回答、数学问题被当音乐。当前现场日志两次flag1/ack played后HTTP101/session_ready1，但transcript0、audio0、no_speech回待机，证明这些事件连接成功而未形成发言；不能把更早成功会话当本次验证。

代码发现：工具分支覆盖中文指令为英文音乐长引导，含歧义Point requests。按用户要求改为唯一极简通用指令：“你是豆包，用用户的语言简短回答。需要操作音箱时调用对应工具；工具未成功，不要声称成功。” 工具说明只保留各自用途/参数结果，音乐搜索明确搜索歌曲，不补身份数学规则或全局路由。

时序缺陷：原preroll300ms，ACK及TLS/session创建期间无capture读取者，紧接话可能截断/丢失。新preroll2秒；确认后立即启动唯一采集线程（同时拥有ALSA和AEC），持续到会话结束；16秒/512KiB内存环形队列，主会话只读队列，不并行调用capture。20ms节奏发送；桥接模式网络poll不再额外等20ms，避免两倍节奏导致堆积。退出/错误join后归还KWS，溢出失败关闭，不写音频文件。2500门限只控制no_speech计时，并不过滤PCM，本轮不盲改增益/门限。

合成测试tools/test-wake-handoff.c验证前滚及后续样本顺序、单一读取者、界限溢出、join停止读和重启；通过。session policy21项通过，ARM-Werror编译通过。新增--voice-fixture仅限显式合成WAV输入、8秒以内源音频、15秒会话，不开麦；只在该模拟模式收集模型回答文本。正式模式不记录用户语音/文字。

可审查候选：Duplex SHA256 06908fb5426451aea58732cce9ccfa9bda39f2c28723f8c7c7f9d211059e17fc。Web构建使用当前完整音乐代码，仅工具说明更新。恢复副本已收Mac private/deploy-recovery/r35/，旧Duplex a4553ad732769d6ecd8510caffc8d7ebb3be7d8e2be4830ffaf1872e10720f6e、旧Web 8c2ee543047adba9abbed7026d8c3b6038c40b5815e2f4ad0a8dcc2e93a05d8f。部署恢复仅用短暂/tmp候选，停止对应daemon后替换校验；恢复亦从Mac上传旧文件并安全stdin启动，不在设备留revision历史目录。本轮不改唤醒模型/本地回应/网易云账户或播放器队列。
