# Report: On-Device Realtime Voice (Round 11)

时间：2026-11-01 14:00–15:00，Asia/Shanghai。
目标：在真实音箱上跑通「说话 → 豆包实时语音 → 音箱出声」，原厂小爱/米家进程不停。

## 结论与范围

本轮在 LX06 上完成“采集语音—连接实时语音服务—音箱播放回答”的最小闭环，同时保留原厂进程运行。报告区分已通过的端到端场景、调试中修复的问题和仍未覆盖的使用条件；单次闭环不能证明长期稳定性或完整原厂功能回归。

## 一、音频通道

- 采集：ALSA dsnoop PCM `mico_record`（`/etc/asound.conf` 已定义），16 kHz 单声道 S16。原厂进程独占 `hw:0,3`，dsnoop 允许第二个客户端共享，实测可同时录音。
- 播放：ALSA `default`（softvol → dmix → `hw:0,2`），24 kHz 单声道 S16，与原厂播放共存。2 秒方波回环测试确认可听。
- 未停任何原厂服务，未改 `/etc/asound.conf`。

## 二、客户端

新增 `src/native_duplex_client.c`（构建 `-Duplex`，运行 `-Duplex -Seconds N`）：

- 复用原厂 libcurl/OpenSSL/json-c/libasound，全部 dlopen，无新增依赖；
- 同一套 WebSocket 握手/帧代码，`https://` + CONNECT_ONLY（curl 7.55 不支持 `wss://`）；
- 20 ms/640 字节一包，Base64 放进 `input_audio_buffer.append`；
- 下行 `response.output_audio.delta` 的 `delta` 字段 Base64 解码后进 60 秒环形缓冲，播放线程写 ALSA；
- 半双工回声保护：播放队列非空时把麦克风帧置零，避免模型听到自己；
- 退出前把队列里没播完的音频播完（最多 30 秒）。

## 三、踩到的坑（都已修）

1. URL 用 `wss://` 导致 curl 直接失败，工厂 curl 只认 `https://` + 手动 Upgrade。
2. 音频 delta 字段是 `delta`，不是 `audio`。
3. 下行音频消息远大于 4 KB，临时缓冲截断导致 JSON 解析失败；改为 64 KB。
4. 环形缓冲结构体未初始化，`stop` 是栈垃圾，播放线程一启动就退出（`written_frames=0`）。
5. 播放队列只有 2 秒，服务端突发下发 20 多秒音频，大部分被丢弃；扩到 60 秒。
6. 不送音频时服务端约 18 秒断开；改为持续送流，触发词交给 `instructions` 判断。

## 四、实测

最终一轮（35 秒窗口，用户说「你好豆包，讲个笑话」）：

```text
session_ready=1
transcript=38 text=20 audio_delta=27 audio_done=2
imported_audio_bytes=488134 written_frames=244067 write_errors=1
audio_peak=11317
```

`written_frames` 与解码样本数一致，说明队列里的音频全部播完，没有再截断。用户确认听到完整回答（前一轮只有 8.6 秒被播出，因 2 秒队列丢包）。

## 五、当前形态与限制

- 启动即持续上传麦克风，靠会话指令「只有明确叫豆包时才回应」当触发词；空闲也计费。
- 半双工：它说话时麦克风置零，不能打断。
- 会话最长按 `-Seconds`，到点关闭；无唤醒常驻、无自动断线重连。
- 未接米家工具、无第二唤醒词、无播放仲裁。

## 六、产物

- `src/native_duplex_client.c`，构建 `Build-NativeGlibc.ps1 -Duplex`，运行 `Invoke-DoubaoSmoke.ps1 -Duplex -Seconds N`。
- 日志 `logs/duplex-runtime.txt`；测试音 `-Duplex -Tone`。
- 探针与工具链沿用第十轮，SHA 校验与临时文件清理不变。
