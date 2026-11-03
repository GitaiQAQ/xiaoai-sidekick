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

`id` 必须是1–63字节的字母、数字或`-_.:`，响应原样回显；HTTP命令入口不自动重试，模型桥另按call_id缓存去重。变更类命令失败后先读状态，不盲目重试。单线程服务串行执行；35项工具候选为每次action设置20秒预算，提供商和ubus等待受剩余预算约束。实际安装与回归结果以当前部署报告为准。

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

35项模型目录增加以下日常控制方法。参数的完整schema以`src/native_tools.h`为准；目录可见不等于每项都已完成实机动作验收，见[本地接入报告](../reports/speaker-capability-tools-integration.md)和最新部署记录。

|方法|参数/返回重点|边界|
|---|---|---|
|device.status / device.volume.get|设备时间、资源、音量|不返回认证或网络身份|
|music.status / music.seek|播放状态；position_ms|定位需同曲目读回|
|music.mode.get / music.mode.set|repeat_all / sequential|只开放已定义的模式|
|alarm.volume.get / alarm.volume.set|value 0–100|设置后读回|
|alarms.list / create / enabled.set / delete|分页、单次时间、ID、enabled|创建时间三选一：timestamp、local_date+local_time、after_minutes；不开放未验证的重复闹钟修改|
|device.lamp.status / device.lamp.feedback|enabled|仅控制Sidekick反馈，不清原厂指示层|
|bluetooth.status / mesh.status / line_in.status|连接/网关/输入状态|只读|
|night_mode.status / sound_effect.status / music.timer.status|配置和状态|未验证的写入不在目录|

网络能力/音质兼容与实际会员验证状态以报告为准。豆包 tools 已接入本地分发器，工具目录由执行器统一下发。

## 扩展约定

新增`mijia.devices.list`、`mijia.properties.get/set`、`mijia.scenes.run`、`alarms.*`、`bluetooth.*`、`system.*`等命名空间。每个方法都需要类型化参数、权限范围、超时、可撤回方案及动作结果读取，不能包装成任意shell或不受约束的原始ubus透传。模型只能使用审核过的具体能力。

短期内部是本地函数调用；需要进程隔离时使用Unix Socket传同一JSON封装。不要为未来可能的流量提前引入多种序列化。






## 网易云队列方法与元数据

队列方法包括`music.netease.charts`、`chart.tracks`、`chart.play`、`playlists`、`playlist.tracks`、`playlist.play`、`chart.prepare`、`playlist.prepare`、`prepare`、`commit`以及`music.next`和`music.previous`。豆包通过固定工具表访问；语音播放先prepare返回内部票据，再在回答完成或有界确认截止后commit。实际播放成功核对原厂status和目标audio_id。

队列上限600首，第三十七轮完整538首歌单prepare与单首播放已验证。歌单详情按32首分批获取；单首、榜单和歌单均使用本地短地址，在原厂播放器取流时刷新音源。35项工具候选的模型曲目列表分页默认20、最多20，这不缩小完整播放队列上限。title、artist、playlist、queue_index与queue_total来自当前audio_id匹配Sidekick内存元数据；应用重启后旧原厂队列是否有对应元数据需单独核实，不能把原厂队列存在当成内存映射已恢复。

## 本地工具接口（模型工具，仅本机 socket）

`/data/lx06-agent/control.sock`（0600，root）上的两个路径不对外开放，网络侧访问返回 403：

- `POST /api/v1/tools`：35项工具版本返回目录 `{"ok":1,"version":2,"tools":[{"type":"function","name":...,"description":...,"parameters":{...}}]}`。命令envelope仍是v1，目录版本与协议版本分开。
- `POST /api/v1/tool`：请求 `{"name":"music_charts","arguments":"{\"id\":\"...\"}"}`，`arguments` 是 JSON 对象字符串（≤3000 字节），响应就是该动作的结果对象；未知工具返回 `{"ok":0,"error":"unknown_tool"}`，参数非法返回 `invalid_tool_arguments`。

工具定义唯一来源是 `src/native_tools.h`：同一张表生成模型目录、工具名到动作的映射；执行仍走 `action_execute()`。新增能力＝一行工具定义＋一个动作分支，语音端无需改动。

模型事件必须显式提供arguments字符串，无参数也传`"{}"`。bridge拒绝缺失、非字符串、超过3000字节、嵌入NUL、非对象或坏JSON，不能替换成空对象执行。每会话最多64个新调用，每批最多8个；重复call_id返回缓存。缓存上限96KiB，单结果上限8192字节，过大结果和响应摘要都有明确错误/截断标记。

## 会话预算与上行控制

- 待机只运行本地唤醒，没有云端连接；唤醒后建立双向会话并在对话中复用。
- 默认20秒首轮无语音/无进展与追问等待，正常结束依据静默和进展；30分钟总时长仅作异常安全上限。
- 默认full双工，模型说话和执行工具期间继续上传AEC音频；服务端ASR开始可取消回答，实现普通语音打断。half仅供诊断对照。
- 上传麦克风音频必须经过AEC；不可用时拒绝云上传，不回退原始音频。正常有凭据唤醒使用短提示音，取消本地语音“收到”。
- 启动参数与环境变量定义以`deploy/start.sh`、`src/native_duplex_client.c`为准。已有播放票据的确认截止15秒不限制普通对话。详见[双工与超时报告](../reports/full-duplex-upload-and-wake-response-policy.md)。
