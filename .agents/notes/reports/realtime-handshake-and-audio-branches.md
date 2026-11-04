# Report: Realtime Handshake and Audio Branches (Round 8)

本轮继续推进机内实现。尚未配置实际API凭据，没有云端认证或语音会话成功记录，没有修改原厂回调/socket/模型。

## 结论与范围

本轮聚焦 WebSocket 握手、帧处理和音频分支的本地实现与回环验证。回环成功只证明客户端处理这些协议边界的能力；真实服务鉴权、会话事件和音频收发以之后的实机报告为准。未验证项保留在本报告末尾。

## 实时客户端

新增增量接收器与握手校验，位于src/native_websocket.h。无动态分配；能跨网络读取边界解析帧，重组分片、处理期间插入的控制帧、验证文本UTF8和Close状态/原因，拒绝超长累计消息。101校验要求正确Upgrade/Connection/唯一Accept，不接受未请求的扩展或子协议。

实机 `logs/native-ws-test-runtime.txt`：基础帧向量通过，新增流/握手33项检查通过，原厂libcrypto SHA1/Base64生成的Accept与RFC6455固定示例一致。检查覆盖中文跨WS分片、插入Ping、各二段网络切分、非法续帧/数据交错、UTF8过长编码/代理区/越界/截断、非法关闭码/原因和累计消息上限。它不是完整协议符合性测试。

`native_library_probe --ws-stdin`新增实际认证升级后关闭代码；从stdin读取原始语音Key，固定HTTPS端点、证书/主机名校验，发X-Api-Key，验证101及Accept后立即随机掩码Close1000，最多等3秒对端关闭。不发送session.create，不访问麦克风。**认证分支尚未实测**，不能把隔离测试等同云端验证。

`tools/Invoke-DoubaoSmoke.sh` 统一文本与实时传输：实时模式只使用 realtime 字段，默认文本模式只使用 ark 字段。公开假 Key 的 stdin 检查已在音箱验证，无网络调用；日志为 `realtime-stdin-transport-test.txt`。真实凭据检查仍未完成。

加入认证分支后再次实测不带认证Upgrade：HTTP400、TLS verify0、头1224B、RSS2004KiB；没有会话创建或音频。日志native-ws-transport-runtime.txt。不能把400确定归因为某项权限或认为已经完成101握手。

最终构建SHA256 d34447d5b3741e95cfcc3a788261fb904d61404369dae9da9a59685b718a56ea。原厂PID1282/2527在各次测试前后相同，临时程序清除；stdin测试后Mesh支持/启用/运行均true。全功能回归尚未完成。

## 直接音频分析的新证据

新增tools/native-asr-boundary.py及temp/analysis/native-asr-boundary-disassembly.txt，只读原厂ELF，集中列出7个函数，491行，未执行这些原厂音频接口。

- `xaudio_wrapper_dnn`0x16120中，0x16148调用`AFE_DnnProcessPostAgc`。对应实现0x51d90也已导出反汇编。能确认这条包装调用链，不能仅凭函数名推断最终输出所有格式参数。
- `xaudio_wrapper_wakeup`0x2009c在0x200bc读取handle内第二个指针，再在0x200d0调用WakeupProcess；该实现0x29368已集中保存。
- 原有native-audio-format-disassembly.txt中的0x1ef44～0x1ef7c：原唤醒线程在分支条件满足时锁定缓存、读取`[engine+0x44]`指向结构的buffer/length，将length有符号除2，条件性将buffer前移半长，随后调用cache_data_save。说明ASR缓存此处选择整个缓冲的一半；选择条件来自栈上的分支标志，具体每半语义尚未完全证明，暂不直接命名声道。

结合先前实读AudioRecord的16000Hz/7ch/S16中间输出，这些证据要求继续追踪DNN输出布局与缓存选择，而非直接把7ch送入要求16k单声道的豆包。最终ASR采样率/位宽/布局仍待交叉验证。本轮没有读取音频样本或改变录音路径。

## 后续

继续准备官方session.create/close事件与音色配置；对认证升级分支做隔离连接验证；继续追踪DNN/ASR缓存格式。凭据具备后，先各做一次受限文本和实时升级测试，再进入无麦克风会话测试。第二唤醒词、真正音频接入、播放仲裁/打断、米家工具调用和全功能回归仍是完整目标的一部分。

参考：[RFC6455](https://datatracker.ietf.org/doc/html/rfc6455)、[OpenSSL1.0.2 SHA ABI](https://docs.openssl.org/1.0.2/man3/sha/)。音频结论以本地原厂指令为依据。
