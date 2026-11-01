# Reference: LX06 控制协议 v1

采用 HTTP + JSON；浏览器状态订阅用 SSE。页面、豆包 tools、未来 NAS 适配器共用方法名称和参数定义。无需在 LX06 上安装 Node/Python 服务。现阶段不加 Protobuf/MessagePack；将来只有确有二进制音频或高频数据需求时再加专用通道，控制语义保持不变。

## 命令

`POST /api/v1/command`，`Content-Type: application/json`。请求头必须带本页面获取的 `X-LX06-Token` 和与设备实际 IP/端口匹配的 Origin；跨域和任意 Host 不接受。

```json
{"v":1,"id":"web-123","method":"music.pause","params":{}}
```

```json
{"v":1,"id":"web-123","result":{"ok":1,"action":"music_pause"}}
```

错误 result 包含 `ok:0,error:<稳定错误代码>`，HTTP 400=参数/未知方法，403=来源/token，409=动作未确认；提供商请求失败可返回 HTTP200、result.ok0 和 api_code。客户端必须同时核对传输和业务结果，不能把HTTP200当成功。

`id` 必须是1–63字节的字母、数字或`-_.:`，响应原样回显；当前没有自动重试或幂等去重。变更类命令失败后先读状态，不盲目重试。单线程服务串行执行，每个请求有长度/超时上限，音源请求最多8秒；不是无界任务调度器。

## 状态与事件

- `GET /api/v1/state`：真实运行快照，含设备/资源、语音阶段、灯效、播放器、网易云最小账号状态、最近操作。
- `GET /api/v1/events`：标准 `text/event-stream`，事件名 `state`。v1发送单份快照后关闭，设置retry4000由EventSource重连；不是保持大量长连接。页面隐藏时暂停订阅。
- `voice` 通过PID及/proc/cmdline核实自有程序，再读取其原子状态文件；进程不存在优先显示stopped。不会把灯亮当作云端会话在线。
- 不返回Cookie/Key/原始系统日志；播放URL按用户要求允许进入原厂日志。

## 当前能力

|方法|参数|实现|
|---|---|---|
|voice.start / voice.stop|空|启停自有助手，凭据只在Web内存/匿名管道传递|
|music.pause / resume / stop|空|原厂播放器状态核对；stop保留原厂队列|
|music.stream.play|url:HTTP/HTTPS|原厂音乐流，不增加代理|
|device.volume.set|value:0–100整数|设置后读回核对|
|device.lamp.clear|空|助手未运行时撤销会话相关1/2/3/8层，不用shut_all|
|accounts.netease.login.start / check|空|机内请求二维码与扫码状态|
|accounts.netease.refresh / logout|空|验证账号或清除本设备会话|
|music.netease.search|query:≤256字节|搜索12条、最小元数据|
|music.netease.play|id:正整数字符串、quality|账号音源、拒绝试听、原生播放|

网络能力/音质兼容与实际会员验证状态以报告为准。豆包 tools 已接入本地分发器，工具目录由执行器统一下发。

## 扩展约定

新增`mijia.devices.list`、`mijia.properties.get/set`、`mijia.scenes.run`、`alarms.*`、`bluetooth.*`、`system.*`等命名空间。每个方法都需要类型化参数、权限范围、超时、可撤回方案及动作结果读取，不能包装成任意shell或不受约束的原始ubus透传。模型只能使用审核过的具体能力。

短期内部是本地函数调用；需要进程隔离时使用Unix Socket传同一JSON封装。不要为未来可能的流量提前引入多种序列化。






## Revision 18: Netease queue methods

Added `music.netease.charts`, `music.netease.chart.tracks`, `music.netease.chart.play`, `music.netease.playlists`, `music.netease.playlist.tracks`, `music.netease.playlist.play`, `music.netease.chart.prepare`, `music.netease.playlist.prepare`, `music.netease.prepare`, `music.netease.commit`, `music.next`, and `music.previous`. Doubao accesses these through the fixed tool allowlist; voice playback uses prepare/commit and starts after the spoken confirmation. Each playlist load is bounded to its first 20 tracks. `music.title`, `artist`, `playlist`, `queue_index`, and `queue_total` are derived by matching the factory current `audio_id` to Sidekick in-memory metadata.

The user-playlist endpoint, native queue index semantics, and real device audio ID mapping have not yet been field-verified; see the revision 18 player-queue report.

## 本地工具接口（模型工具，仅本机 socket）

`/data/lx06-agent/control.sock`（0600，root）上的两个路径不对外开放，网络侧访问返回 403：

- `POST /api/v1/tools`：返回模型可见目录 `{"ok":1,"version":1,"tools":[{"type":"function","name":...,"description":...,"parameters":{...}}]}`。
- `POST /api/v1/tool`：请求 `{"name":"music_charts","arguments":"{\"id\":\"...\"}"}`，`arguments` 是 JSON 对象字符串（≤3000 字节），响应就是该动作的结果对象；未知工具返回 `{"ok":0,"error":"unknown_tool"}`，参数非法返回 `invalid_tool_arguments`。

工具定义唯一来源是 `src/native_tools.h`：同一张表生成模型目录、工具名到动作的映射；执行仍走 `action_execute()`。新增能力＝一行工具定义＋一个动作分支，语音端无需改动。

## 会话预算与上行控制

- 会话上限 60 秒，首轮无语音 6 秒，追问 8 秒。
- 会话开始 10 秒内没有任何回答开始（无文本、无音频、无 `response.done`）时主动关闭，日志为 `session_stop=no_response`；唤醒后无人说话不再持续上行到 60 秒。
- 播报回答和执行工具期间本机停止上行，同时发送 `input_audio_mute.commit`。
- 本地“有人说话”能量门限默认 2500；`LX_NO_RESPONSE`（1–60 秒）和 `LX_VOICE_GATE`（100–100000）可覆盖。
