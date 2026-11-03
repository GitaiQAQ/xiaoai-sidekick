# Report: Factory Runtime Interface Inventory

## Scope and evidence

2026-11-03，针对运行 LX06 固件 1.94.13 的设备，通过 SSH 只读枚举 `ubus list` 和各对象的 `ubus -v list`。本次得到 26 个对象、260 条方法签名。只读取了方法名称和参数类型，没有调用这些方法，也没有读取参数值、账号凭据或 Mesh 密钥。

完整签名快照见[运行时 ubus 签名报告](factory-runtime-ubus-signatures.md)，静态实现路径和协议分层见[协议逆向报告](factory-protocol-reverse-engineering.md)。

本报告将证据分为三类：

- **运行时签名**：设备当前 ubus 服务公布的对象、方法和参数类型；不表示每个方法已成功运行，也不代表调用者权限相同。
- **仓库封装**：`native_tools.h` 当前发布的 35 个模型工具；部署报告记录了这些工具的有限实机验收。
- **固件静态线索**：固件二进制中的字符串、符号或配置；只能指出可能的协议路径，不能证明运行时可调用或操作成功。

260 是方法签名数量，不是 260 个不同的用户功能。不同对象可能重复暴露同一方法，底层服务可能只接受特定调用者，部分接口依赖账号、设备 ID、认证信息或特定状态。此清单只覆盖当前设备公开的 ubus 表面，不覆盖私有 Unix socket、未公开 IPC、云端功能、设备 UI 或不同固件版本。

## 与当前模型工具的覆盖差异

当前 35 项模型工具覆盖音乐查询和播放、设备与音量、播放状态和部分控制、睡眠计时/音效读取、单次闹钟、灯反馈，以及蓝牙、Mesh、夜间模式、Line-in 状态。它们并没有覆盖设备全部 ubus 接口。

运行时清单里还有大量可能有用户价值、但尚未经过语义与风险审查的功能：

| 领域 | 当前签名示例 | 当前判断 |
|---|---|---|
| 闹钟扩展 | `alarm.chime_hourly_get/set`、`alarm.alarm_set_work_mode`、`alarm.micmute`、`alarm.set_store_show_status` | 可能对应整点报时、闹钟工作模式、定时静音或展示策略；参数虽已知，用户语义、枚举与恢复行为未确认 |
| 红外 | `ir_agent.learn/read/codec/play/play_batch` | 可能支持学习和发送红外码；会读取遥控器信号或控制外部家电，不能直接提供模型调用 |
| 儿童模式与通知 | `path_child_mode.status/set`、`notify.status/create/query/set/play/stop` | 存在状态和配置接口；消息文本、级别、自动播放与权限语义仍需检查 |
| 投播和多房间 | `mdplay.casting_status/pause/resume/quit/do`、`device_detect`、`query_device_groupInfo`、`change_speaker_group` | 可能支持 DLNA/MiPlay 投播和音箱分组；`do`、分组变更会改变播放目标或其他音箱状态 |
| 媒体播放器扩展 | `mediaplayer.player_play_private_fm`、`player_get_latest_playlist`、`player_set_positon`、`player_set_shutdown_timer`、`set_playrate`、`player_aux_operation` | 当前工具只覆盖其中部分读取和播放操作；此前 `player_set_shutdown_timer` 的 set/cancel 请求均返回 code 162，读回仍为 type 0。其它 URL、播放源和辅助操作语义需单独核对 |
| 麦克风与声纹 | `alarm.micmute/micmute_remove`、`voice_print.get_status`、`register_voice_recognize/payment` 及取消方法 | 涉及麦克风隐私或声纹/支付授权；不能按一般家居开关处理 |
| 环境检测 | `pnshelper.oneshot_get`、`environ_detect_get`、`db_detect_get` 及相应 setter | 有一次性环境检测、配置检测方法；字段含义和对原厂行为的影响未知 |
| 线路输入 | `linein.start/stop/set_soft_vol/get_soft_vol` | 当前目录只公开状态读取；启动/停止输入源和音量设置尚未纳入 |
| 蓝牙与 Mesh | `mibt.connect/disconnect/unpair/enable/volume`、`mibt_mesh.mesh_scan/mesh_bind` | 状态工具只读取信息；配对、断连、扫描和绑定会改变连接或网络状态 |
| 设备与服务管理 | `system.reboot/sysupgrade`、`service.add/set/delete/signal`、`miio.register/connect/drop/token_write` | 系统级或凭据级接口；签名显示可重启/升级、增删改服务定义或修改账号连接状态，禁止包装成一般模型工具 |
| 通用云与 MIoT | `miio.miot_rpc`、`messagingagent.miot_push`、`mibrain.aivs_event_post` 等 | 泛化请求可能绕过参数和权限边界；不暴露原始文本、RPC 或凭据透传 |

表中“可能”表示由方法名推测用途，不是已验证的功能说明。要把候选变成可用工具，仍需核实输入格式、状态变化、结果语义、错误返回、访问控制和可恢复性。

## 当前结论

此前工作直接从用户指出的睡眠模式问题切入，随后围绕已选出的 35 项工具验收，没有先从整台设备的运行时接口建立覆盖清单。因此能确认 35 项工具的有限验收，却不能据此声称已经盘点原厂功能；这属于调查范围不足，不是设备没有暴露其他功能。

本次只读枚举确认确实存在很多未封装的接口候选；后续静态逆向还发现运行中的 Mesh 二进制维护子设备树、产品规则与 MIoT 属性映射。详见[协议逆向报告](factory-protocol-reverse-engineering.md)。但“接口可见”不等于“功能已理解、可安全控制或适合交给语音模型”。尤其服务管理、系统升级/重启、账号注册、声纹/支付、红外、Mesh 绑定、通用 MIoT/RPC 都应从模型工具候选中排除。

建议后续按下列顺序补审，而不是按 ubus 方法数量逐个开放：

1. **低副作用状态查询**：儿童模式状态、通知状态、整点报时状态、投播状态、声纹注册状态、环境检测状态；先确认是否泄露隐私或账号信息。
2. **用户可见且可恢复的本地设置**：夜间/睡眠设置、闹钟扩展、通知音量、Line-in、播放速度和关闭计时；逐项查明值域、执行后读回和恢复步骤。
3. **会影响外设或其他设备的动作**：红外发送、蓝牙控制、Mesh 发现/绑定、投播与音箱分组；先设计目标确认、权限校验和撤销流程。
4. **系统、账号和通用执行接口**：重启、升级、service 管理、miio 凭据、任意 MIoT/RPC、原厂云端文本；不作为模型工具开放。

本报告没有调用候选接口、修改设备、部署程序或测试未覆盖功能。下一步若要扩展工具，应以具体用户场景为单位审查完整契约，而不是把 ubus 方法直接映射给模型。
