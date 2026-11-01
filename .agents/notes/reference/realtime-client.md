# Reference: 机内实时传输客户端

`native_library_probe.c --ws-stdin`现包含受限的实时接口认证升级/关闭流程，复用原厂curl和crypto库。它从stdin读原始语音API Key（最多4096B可见ASCII），只连接固定官方域名/路径，在证书校验通过后发X-Api-Key，验证HTTP101、Upgrade/Connection及Sec-WebSocket-Accept。未协商扩展或子协议。

101成功后立即发送随机掩码Close1000，最多等3秒对端关闭；可在等待中响应Ping，忽略但不打印其他消息。成功需要有效Close1000/1001。**默认不发送session.create，不开启麦克风**。2026-11-01 已用真实Key实测升级与关闭：101、TLS verify 0、Accept 校验、Close 1000 全部通过。

无密钥可验证：

```powershell
$p='C:\Users\gitai\Desktop\lx06-agent'
& ([scriptblock]::Create((Get-Content -Raw "$p\tools\Build-NativeGlibc.ps1"))) -ProjectDir $p
& ([scriptblock]::Create((Get-Content -Raw "$p\tools\Run-NativeProbe.ps1"))) -ProjectDir $p -Kind ws-test
& ([scriptblock]::Create((Get-Content -Raw "$p\tools\Run-NativeProbe.ps1"))) -ProjectDir $p -Kind ws-loopback
& ([scriptblock]::Create((Get-Content -Raw "$p\tools\Invoke-DoubaoSmoke.ps1"))) -ProjectDir $p -Realtime -TransportTest
```

`ws-test`包含RFC固定掩码向量、增量解析/UTF8/分片/控制帧、握手头校验以及原厂SHA1/Base64的RFC Accept固定向量。`ws-loopback`在设备上用 `--ws-test-server` 起 127.0.0.1 场景对端，再用 `--ws-local` 走同一条认证分支，覆盖正常101+关闭、错误Accept、HTTP400、不发Close四种情况；它只连回环，不代表云端可用。`-TransportTest`只传公开假Key，设备检查字段，不建立网络连接。三个测试都使用/tmp临时二进制、远端SHA校验和清理。

回环模式只用于控制流验证：`ws_test_server` 按请求里的 `Sec-WebSocket-Key` 用原厂 libcrypto 计算 Accept，`--ws-local` 发送固定假 Key `loopback-test-key`，不读取凭据。

凭据填入private/credentials.json的realtime.api_key后，`Invoke-DoubaoSmoke.ps1 -Realtime`才执行一次认证升级/关闭。该入口与Ark共用安全传输包装，但只取相应服务的字段；不把语音Key发给文本接口。旧Invoke-ArkSmoke.ps1保留文本模式入口。

帧模块使用调用方提供的缓存，当前消息上限64KiB，累计分片不能超过上限；超界关闭并报错，不截断或无限扩容。这个上限尚未与服务端音频delta最大尺寸交叉验证，音频集成前必须验证，不可仅凭测试推断兼容。`--ws-session` 会在101之后发送JSON `session.create`（model 1.2.6.1、输入pcm 16000、输出pcm_s16le 24000、音色`zh_female_vv_uranus_bigtts`），等`session.created`，再发`session.close`等`session.closed`，全程不发送音频。2026-11-01 实测 `session_created=1 error_event=0 session_closed=1`，随后Close 1000。语音上行/下行仍未测试。

## 官方 Seeduplex 协议（2026-11-01 核对）

来源：`docs.volcengine.com/docs/doubao-voice/end-to-end-real-time-voice-full-duplex-version?lang=zh`（页面更新 2026-10-27），音色表 `.agents/notes/6561/1257544`。以下为官方正文，属公开资料，不是本项目实测。

- 端点 `wss://openspeech.bytedance.com/api/v3/duplex/realtime/dialogue`，请求头 `X-Api-Key`。旧版控制台鉴权仍兼容，后续会下线。
- 上行事件是 **JSON 文本帧**，音频要 Base64 后放进 `audio` 字段，不走二进制帧。
- `session.create`：`{"type":"session.create","session":{"model":"1.2.6.1","instructions":"...","audio":{"input":{"format":{"type":"pcm","rate":16000}},"output":{"format":{"type":"pcm_s16le","rate":24000},"voice":"<音色ID>","speed":0,"loudness":0}},"tools":[...]}}`。`model` 固定 `1.2.6.1`，`audio`、`voice` 必填；`session.id` 用于接续历史对话。
- 输入音频只支持 16000 Hz，`pcm` 或 `speech_opus`；建议 20 ms 一包，16k/int16 下 640 字节，并按实时节奏发送，过快过慢都会报错。
- 输出只支持 24000 Hz；`pcm` 是 32 位小端 PCM，`pcm_s16le` 是 16 位，另有 `ogg_opus`。播放侧要按所选格式解码。
- 关麦后必须发 `input_audio_mute.commit`，恢复发 `input_audio_unmute.commit`，否则服务端超时。强制判停用 `input_audio_buffer.commit`，打断播报用 `response.cancel`，结束会话用 `session.close`。

上行事件：`session.create`、`session.update`、`speech_text_buffer.commit`（打招呼）、`input_audio_buffer.append`、`input_audio_buffer.commit`、`speech_text_buffer.replacement.append/commit`、`conversation.item.create/update/retrieve/delete`、`response.cancel`、`session.close`。

下行事件：`session.created/updated/closed`、`input_audio_buffer.committed`、`conversation.item.input_audio_transcription.started/delta/completed/failed`、`response.output_text.delta/done`、`response.output_audio.started/delta/done`、`conversation.item.added/retrieved/deleted`、`response.function_call_arguments.done`、`response.done`、`response.canceled`、`error`。

S2S 主打音色（官方标注）：Vivi 2.0 `zh_female_vv_uranus_bigtts`、小何 2.0 `zh_female_xiaohe_uranus_bigtts`、云舟 2.0 `zh_male_m191_uranus_bigtts`、小天 2.0 `zh_male_taocheng_uranus_bigtts`。文档示例里出现的 `zh_female_vv_jupiter_bigtts` 与表中 Vivi 2.0 的 ID 不同，实际请求前要按控制台音色表确认，不要混用。

仍未实测：这段 JSON 在真实 101 之后的行为、`event_id` 是否必需、`response.output_audio.delta` 的最大帧尺寸（与本地 64KiB 上限相关）、错误码含义（见官方《接入必读》）。
