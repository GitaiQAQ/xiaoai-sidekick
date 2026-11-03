# Report: Speaker Tools Deployment and Device Validation

2026-11-03。用户明确重新授权“部署”后，35项工具应用已部署并完成有界实机验收。只替换Web/Duplex，没有更换未达35/35的唤醒候选、刷ROM、重启整机或停止原厂服务。用户原有暂停队列保持status2、volume30、loop0、position112883，当前audio_id前后相同；没有为验收恢复播放、seek或换队列。

## 当前版本与恢复材料

|应用|实际设备SHA256|PID|本机候选大小|
|---|---|---:|---:|
|Web|ea787bb8ff5511cb0791c4cf67dad6dd5b40cdbea533f3aa2b128ed24cecb60a|2353|94024B|
|Duplex|2c575af55c9f20ae064162db2ba6d80a79725a5df5536340a7e2f7ea65dfe677|2377|117476B|

原版本在任何设备写入前从设备只读收至Mac `private/deploy-recovery/tools35-20261103/`，目录0700、文件0600，逐项本地字节SHA与远端sha256sum一致。`plan.txt`和`manifest.json`记录具体恢复方案、当时自有argv、用户播放状态、模型SHA及灯策略原状态；不含密码、API key、Cookie或设备认证数据。

|恢复文件|原设备SHA256|
|---|---|
|lx06-web-armhf|55aa0dcf928a5244777fe10631b36196f6655630f508c810c633695a354f5c98|
|lx06-duplex-armhf|435477cfb919e2a72df931cfad268317771cd751060e3cd8b12ba4c418f4f12e|
|start.sh|9393a0ea1752e033e110714ecae5222ea97e22f8df0083b017a10d265f0185b8|
|stop.sh|7c305fdcfae4682593707a84e0d8cca83072f068afa850f9aa957e4236940189|

灯策略文件原本不存在，验收后也不存在。start/stop未替换。新旧自有argv均核实为factory loader+准确应用路径，替换前voice状态standby，无活动云会话要中断。原停止脚本只处理自有Duplex/tagged writer；Web按已核对argv的PID停止，不匹配原厂进程。

每份小文件独立staging并核SHA后替换。账号凭据从Mac private JSON内存读取，通过SSH stdin/FIFO启动，未放argv或工具输出。设备没有历史恢复副本，web.tools35-stage、duplex.tools35-stage、tools35-abi-stage均已清理/消耗；未清扫他人的未知文件。

## ARM ABI与35项实际目录

先部署11252B只读ABI checker并运行，实际factory libjson-c动态加载与函数调用通过：object_length1，嵌入NUL字符串string_length3而strlen1，真实C目录解析35项/9201B。该checker不打开音频、不连接云，完成后移除。新Web启动本身也加载了object_length/get_string_len/get_int64等实际依赖，Duplex --tool-test实际读取string_length运行通过。

设备curl CLI没有--unix-socket支持，返回exit48，没有业务动作；改用现有Duplex --tool-catalog/--tool-test的原生Unix接口，没有安装依赖或修改已审查候选。

实际目录35项：music_search、music_charts、music_chart_tracks、music_playlists、music_playlist_tracks、music_play、music_play_chart、music_play_playlist、music_next、music_previous、music_pause、music_stop、speaker_status、speaker_volume_get、speaker_volume_set、music_status、music_resume、music_seek、music_mode_get、music_mode_set、sleep_timer_get、sound_effect_get、alarm_volume_get、alarm_volume_set、alarm_list、alarm_create、alarm_enabled_set、alarm_delete、lamp_status、lamp_feedback_set、bluetooth_status、mesh_status、night_mode_status、line_in_status、session_end。

新增只读实际检查13项全部ok1：speaker_status、speaker_volume_get、music_status、music_mode_get、sleep_timer_get、sound_effect_get、alarm_volume_get、alarm_list、lamp_status、bluetooth_status、mesh_status、night_mode_status、line_in_status。不是只看到HTTP200：每项核对业务结果，必要字段经过设备上相同执行器校验。

speaker_status实测2026-11-03T08:45:10+0800/CST/+0800，后续08:47:59同格式；设备date实测2026-11-03T08:49:02+0800 timezoneCST，同日期与时区，未改系统TZ。Python3.9测试脚本不接受ISO basic offset的+0800，改用strptime后验收，未改设备格式。

## 错误参数无执行

通过实际ARM worker，分别对music_stop/session_end测试过长、非string、object、null、坏JSON、非object JSON、嵌入NUL及缺失arguments，共16例；全部返回invalid_tool_arguments，无tool_executed记录，end_session0。Web审计事件的action/ok序列未增加，播放器status/audio_id不变。age_seconds随时间自然增长，验收只比较有意义的事件内容。

另以结构合法的模型调用进入真正Web工具入口，测试music_stop未知属性、volume101与volume boolean三例，全部invalid_tool_arguments，不改变音量或播放状态。没有执行一次有效的停止/结束会话作为错误测试。

主代理此前已重跑全部Mac测试：cap146、bridge259、endpoint22、目录35及容量、灯所有权、full/half、timeout、policy26通过。实机与Mac测试范围不同，不能把本机mock覆盖当35种硬件动作全已执行。

## 可恢复设置与owned闹钟

- 媒体音量30→31→30，设置业务ok1与独立读取一致；beep0，无试播。
- 闹钟音量18→19→18，isPreview0，独立读取一致。
- Sidekick灯反馈enabled1→0→1，策略读回一致；最后策略文件恢复原本不存在。没有shut_all、任意原厂层或RGB控制，也没有活动语音灯要清除。
- 用设备时间契约local_date+local_time创建七天后的单次临时闹钟，返回timestamp与预期一致、owned ID不是原列表ID；关闭返回enabled0，开启enabled1；最后只删除owned ID，原一条闹钟的ID/时间/重复mask/音量/启用状态逐项不变。owned-alarm.json在Mac恢复目录记录deleted_verified，不输出闹钟私密文本或ID。

没有测试music_resume/music_seek/music_mode_set，以保留用户当前暂停队列和loop0。它们已经注册、Mac读回逻辑已测，但本轮不宣称这三个写动作已实机验证。

## 音乐20秒预算回归

原音乐路径实际读到65榜单/50歌单，账户/ID/私密歌单名仅在进程内使用，不输出Cookie、UID、音源URL或票据。

|原工具|实际prepare数量|耗时|确认|
|---|---:|---:|---|
|music_play_chart|100|1.725秒|ok1/queued1、queued=requested，无commit|
|music_play_playlist|538|7.620秒|ok1/queued1、queued=requested，无commit|

完整538条仍经32条provider批次取得，当前网络下没有被新20秒预算必然截断。验收是prepare，不启动这些队列。每次结束先核用户仍status2且ID/位置/模式/音量一致，再使用已暂停队列的幂等music_pause分支清pending（该分支先清票据，不需stop或resume），业务结果成功且前后用户状态逐项一致。

短本地/r/netease/149297/standard.mp3 HEAD实际返回302，provider Location存在、长度286；Location只在Mac内存解析，未输出或下载音频正文。这验证本地短地址/提供商解析，不声称本轮又播放了一首歌。

Web重启后Sidekick内存queue元数据未保留，当前旧factory队列的queue_total/index/title映射不可用；factory当前audio_id、暂停状态、位置仍保留。next/previous按既有实现仅适用于本项目已经加载的队列，不能把旧factory队列持续存在等同于Sidekick完整缓存已恢复。没有为验收重载/替换用户队列；此边界交主代理同步文档。

## 云schema（主代理独立实测）

主代理Mac以同35项目录、原极简prompt与相同音频配置发送session.create，真实云upgrade101/session.created通过，随后session.close/session.closed通过；0音频帧、0工具动作、未采麦克风，request9560B。安全结果在temp/analysis/speaker-capabilities/cloud-schema-result.json。部署代理没有重复云连接。这证明schema接收与最小会话创建，不是新工具真人语音回归。

## 原厂、资源、模型与待机

最终factory PID与部署前相同：mipns-xiaomi1093、mico_aivs_lab3667、miio_client1585、central_lite2440、mibt_mesh_rtl3518、mediaplayer3214。没有重启这些进程或整机，也没有把PID存在表述为米家/通话等全部端到端验收。

部署前MemAvailable23076KiB、/data可用5464KiB；最终24108KiB与5968KiB。Web RSS2496KiB/1线程、Duplex7040KiB/1线程。完整歌单prepare后Web仍存活，没有本轮新OOM/恢复操作。

现用模型SHA前后完全一致：d96-v5.binn=471dfb5ea0e8977e53550ad3ab392beb3b685609a08e51ddf84a8b5ccada9ba3；l2-d160.binn=96917162f1ec9bb589f098cc19283f676d2e73f777adbe37f7f55f3c9600c92c。没有晋级新的wake候选；召回未达35/35问题仍独立存在。

最终voice standby；按Duplex自身FD socket inode与/proc/net映射，TCP/TCP6/UDP/UDP6各0、Unix1。没有主动上传麦克风或新录音，未做待机预连接；正常本地KWS/AEC服务继续运行。暂停播放状态最终为status2/volume30/loop0/position112883。

## 回滚方案与剩余边界

当前恢复包来自此次真实部署前版本，不是仅依据更早e1/efe副本：待活动语音会话正常结束，停止准确自有应用；从Mac逐个上传保存的原Web/Duplex到临时staging、核SHA/chmod700后替换，按原脚本受保护stdin启动。start/stop此次不变；灯策略原状态为不存在。恢复只处理应用与本轮策略，不动ROM、原厂服务、用户认证或队列。设备不存历史副本，临时staging验收后清。

未接入的IR、Mi Home家电执行、蓝牙连接、夜间模式/音效/睡眠计时setter条件见speaker-capability-tools-integration.md；其缺失不应混同于此次35项应用部署失败。后续新增写工具必须先有明确语义、目标映射与可审查恢复方案。应用部署已收尾；后续原厂接口的完整清单见[运行时接口盘点](factory-runtime-interface-inventory.md)。

## 睡眠模式与米家 Mesh 只读调查

2026-11-03 在同一台 LX06、固件 1.94.13 上通过 SSH 只读查询 `ubus -v list`、`nightmode status`、`nightmode current` 和 `mibt_mesh mesh_status`。没有调用 setter、扫描、绑定、MIoT RPC 或属性写接口。

### 夜间模式和睡眠模式

设备运行时的 `nightmode` 对象公开以下方法与参数：

| 方法 | 运行时参数 | 已确认行为 |
|---|---|---|
| `set` | `total`、`light`、`volume`、`start`、`stop`，均为 String | 方法存在；本次未调用，值域、持久化和生效时机未知 |
| `sleep_mode` | `value`、`from`，均为 String | 方法存在；名称不足以确认它是开关、状态设置还是带来源标记的请求，本次未调用 |
| `status` | 无 | 只读状态查询 |
| `current` | 无 | 只读当前状态查询 |

本次设备读回 `status` 的 `info` 为：`sleep_mode=normal`、`total=night`、`light=night`、`volume=night`、`start=22:00`、`stop=07:59`；`current` 为 `total=normal`、`light=normal`、`volume=normal`。固件 init 脚本只给出首次创建配置时的默认 `stop=06:00`，不能覆盖运行设备当前读到的 `07:59`。这些结果确认了当前字段值和只读查询；没有确认完整枚举、时间边界语义、睡眠模式触发规则或 setter 副作用。新增写工具前仍须确认这些契约并准备恢复办法。

### 米家与 Mesh 子设备

设备运行时 `mibt_mesh` 对象有 `mesh_status`、`mesh_scan(caller: String)`、`mesh_bind(is_cancel: Boolean)`、`mesh_enable(caller: String)`、`mesh_disable(is_need_reboot: Boolean)`、`wakeup_tv(tv_mac: String)` 和 `miconnect_enable(interval: Integer, action: String)`。本次 `mesh_status` 返回 `mesh_is_support=true`、`mesh_is_enable=true`、`mesh_is_running=true`、`mesh_status=BTMESH_IDLE`。这确认 Mesh 网关状态可读，不等于已取得子设备目录或设备属性。

静态逆向运行中的 `/data/miio_mpkg/bin/mibt_mesh_rtl` 后发现设备树、产品规则、DID/PDID/MAC/mesh地址、SIID/PIID/EIID映射及属性/场景 RPC 路径；固件确实实现了子设备发现和属性协议的组成部分。仍没有读取或验证当前设备清单、账号授权来源或完整请求格式。`mibt_mesh.mesh_scan` 会启动扫描，`mesh_bind`、`miio.miot_rpc` 与属性/场景请求可能改变设备状态或绑定关系，因此本次均未调用。`libxiaomi_miot.so` 的动态符号仍只显示 UDID 身份工具，不能作为家电控制 SDK 的证据。完整静态调用路径与剩余未知项见[协议逆向报告](factory-protocol-reverse-engineering.md)。

当前可确认的安全边界仍是读取音箱自身的 Mesh 网关状态。若要控制家电，需先取得并验证账号授权、稳定的设备身份到 `did/siid/piid` 映射、属性类型和值域，以及每类操作的权限和失败语义；现有材料尚不能建立这些条件。豆包语音 API 凭据不提供米家账号授权。
