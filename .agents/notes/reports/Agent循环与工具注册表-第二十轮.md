# Report: Agent 循环与工具注册表：第二十轮

## 结果

用户提出需要一个可复用、便于接入新能力的 agent loop。本轮把工具定义收敛到一个注册表，把会话循环从豆包客户端里抽出来，并补了两条与空转计费相关的截止规则。现有音乐工具经新路径实测可用，语音端不再保存任何工具定义。

## 工具注册表（单一真源）

新增 `src/native_tools.h`：

- 每条工具记录包含 `name`、`description`、`parameters`（JSON Schema）、`action`（执行动作名）和 `scope`。
- `lx_tools_json()` 生成模型可见目录；`lx_tool_find()` 解析工具名并校验范围。
- 工具名到动作的映射也在这张表里，不再有独立的 `tb_method`。

Web 控制台：

- `action_reply()` 的执行逻辑抽成 `action_execute()`，页面 RPC、`/api/action`、模型工具三条路径共用。
- 新增仅本机 socket 可访问的 `POST /api/v1/tools`（工具目录）与 `POST /api/v1/tool`（单次调用）。网络侧访问返回 403。
- 参数上限 3000 字节；未知工具返回 `{"ok":0,"error":"unknown_tool"}`；解析失败返回 `invalid_tool_arguments`。

语音端（`src/native_tool_bridge.h`）：

- 删除 `tb_tools` 与 `tb_method`。
- 会话建立时 `tb_catalog()` 拉取目录并放进 `session.create`；失败时打印 `tool_catalog=unavailable`，会话仍可用于对话。
- 模型调用经 `tb_call()` 原样转发 `name` 与 `arguments`，结果回填给模型；`play_ticket` 仍由桥接层剥离并触发延迟提交。

## Agent 循环（provider 与 host 分离）

新增 `src/native_agent_loop.h`：

- provider 接口：`stats`、`send_audio`、`send_json`、`pump`。豆包实时是第一个实现，连接、握手和 `session.create` 仍在 `native_duplex_client.c`。
- host 接口：`capture`、`capture_recover`、`play_delta`、`playback_idle`、`led`、`publish`、`tool_start`、`tool_busy`、`tool_take`、`tool_has_ticket`、`stop`。
- 循环本体负责：上行音频泵、轮次策略、工具轮的静音/解除静音、追问窗口、会话上限、日志与停止原因。
- 行为保持：工具轮期间继续采集但不上行；回答播放时静音；延迟提交音乐；追问结束回待机。

## 空转与计费修正

现场日志显示一次唤醒后 49 秒空转（`session_stop=max_duration`，无回答）。原因有两个：本地能量门限 900 会被环境噪声反复刷新“有人说话”时间；会话没有“从未开始回答”的独立截止。

- 新增 `no_response_ms`（默认 10000）：会话开始后若没有文本、音频或 `response.done`，关闭会话，日志 `session_stop=no_response`。工具执行期间不触发。
- 本地语音门限 900 → 2500（`LX_VOICE_GATE` 可调）。

## 实测（2026-11-01）

- `--transport-self-test`：9 项通过，无网络连接。
- `--tool-catalog`：返回 12 个工具，字段与旧静态定义一致。
- `--tool-test` 三次：`music_charts` 返回完整榜单列表；`music_search`（Jay）返回 12 条；`music_playlist_tracks` 返回「@Gitai的十年精选辑」19 首；`made_up_tool` 返回 `unknown_tool`。
- Web RPC 回归：`music.netease.search` 12 条、`music.netease.playlists` 50 条。
- 唤醒回放（真实 Key）：`tool_catalog=loaded`、`session_ready=1`、无后续语音时 `session_stop=no_response`，415 帧（约 8.3 秒上行）后结束，`session=pass`；临时二进制已清理，原厂 1282/2527 与 Mesh 正常。
- 真人会话（部署后、加截止规则前）：一次唤醒调用 `music_search`，回答播放期间 `uplink_paused_for_playback`，追问超时结束，`session=pass`。

## 部署与恢复

- Web SHA256 `4A559F5FE5C7A19B93F5D2B5EBBDE937D552F01A7E652B8F1322087FE1489BE5`，Duplex SHA256 `0D77AFE0299C0D1648D6396616E4B90701FAC4C23A083FCADFD8713AD36F4A8F`，Web PID 2854。
- 部署前状态在 `/data/lx06-agent/revision20-original/`（Web `e45b9cb9…`，Duplex `3d561922…`）。
- 回滚：停止语音与 Web，从该目录恢复同名文件后按需启动。

## 限制

- 10 秒截止以“回答是否开始”为准；用户连续说超过 10 秒且模型尚未开始输出时，可能被提前关闭。
- 本地门限是固定能量值，安静房间与嘈杂客厅需要不同的最优值；尚未按底噪自适应。
- 尚未做本地 VAD 门控上行；服务端断句仍依赖持续音频。
- 真人完整点歌（`music_prepare` → 回答 → `netease_commit` 入队）尚未在最新二进制上复测；上一轮页面路径的入队验证不受本轮改动影响。
- 新循环尚未接入第二个 provider；方舟文本通道仍是一次性调用。
