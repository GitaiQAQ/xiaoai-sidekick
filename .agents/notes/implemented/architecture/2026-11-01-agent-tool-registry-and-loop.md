# Agent Note: Agent tool registry and provider/host loop

Status: implemented

## Problem

工具定义分散在三处：`native_tool_bridge.h` 的 `tb_tools[]`（给模型的 schema）、同文件的 `tb_method()`（工具名 → RPC 方法）、`native_web_console.c` 的 `map[][]`（RPC 方法 → 动作）。新增能力要改三处，模型描述和实际执行容易漂移。

会话循环整个写在 `native_duplex_client.c` 的 `run_session()` 里：麦克风泵、WebSocket 事件、工具线程、轮次策略、播放环、灯效、状态发布全部内联，与豆包实时协议绑死。换一个提供方（方舟文本、将来的其他通道）只能复制这一大段。

## Decision

工具定义收敛到执行器里的唯一注册表 `src/native_tools.h`：每条记录包含名字、描述、JSON Schema、动作名和可见范围。模型目录、工具名映射、执行分派都从这张表生成。

Web 控制台新增两个只在本机 Unix socket 可用的端点：

- `POST /api/v1/tools` 返回模型可见目录；
- `POST /api/v1/tool` 接收 `{name, arguments}`，交给统一的 `action_execute()` 执行。

页面 RPC、`/api/action`、模型工具共用同一个执行函数。

语音端删除硬编码的 schema 和名字映射：会话建立时拉取目录，失败则打印 `tool_catalog=unavailable` 并继续无工具会话；模型调用原样转发到 `/api/v1/tool`。

循环拆成 provider 与 host：provider 只负责 `stats`、`send_audio`、`send_json`、`pump`；host 负责采集、回放、灯效、状态和工具线程；`src/native_agent_loop.h` 持有轮次策略、静音/解除静音、工具轮、追问窗口与会话预算。连接和 `session.create` 留在豆包实现里，因为那段与协议、音色和工具目录强相关。

同时补了两条预算：会话上限 60 秒、首轮无语音 6 秒、追问 8 秒；新增“10 秒内没有任何回答开始就关闭”，本地语音门限从 900 提到 2500。

## Consequences

- 新增一个工具只需改注册表一行加一个动作分支，语音端零改动。
- 工具执行仍然依赖 Web 进程；语音会话在目录不可用时降级为无工具对话。
- 空转会话从 49 秒降到约 8.3 秒（415 帧）后关闭。
- 调试中修掉了两个真实缺陷：工具名在 JSON 释放后继续使用导致 `unknown_tool`；`action_execute()` 与调用方重复释放请求对象。
- 已验证：目录 13 项、工具分发（榜单/搜索/歌单曲目/未知工具/`session_end`）、页面 RPC 回归、传输自检。
- 未验证：完整点歌入队在新循环上的真人回归；方舟文本 provider 未接入；米家/闹钟/红外只有接入位置。

## Alternatives considered

- **把注册表放在语音端**：语音端只跑一个会话，目录仍需与执行器同步；放在执行器可以让页面、语音和未来的定时任务共用同一张表。
- **工具在执行器进程内直接调用**：语音端与执行器本来就是两个进程，凭据和网络客户端都在执行器；进程内调用需要把凭据搬进语音端，扩大暴露面。
- **保留静态 schema、只统一执行**：仍然要维护两套定义，模型描述和实现继续漂移，没有解决最初的问题。
- **每个工具一个 HTTP 路径**：路径数量随工具增长，鉴权和参数校验要复制；单一 `/api/v1/tool` 加注册表更小。
- **把整个 `run_session` 一次性重写**：风险集中在已工作的音频路径上；先抽 provider/host 边界、保留握手在豆包实现里，行为可对照日志逐段回归。
