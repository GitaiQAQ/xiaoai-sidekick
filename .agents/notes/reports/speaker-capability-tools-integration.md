# Report: Speaker Capability Tools Integration — Local Candidate

2026-11-03。**本轮最终状态为 LOCAL ONLY：35 项模型工具已接入本机代码目录与执行器，其中新增 22 项；候选仅留 Mac，未部署、未做新工具实机验收。** 用户准备休息后明确禁止设备访问、控制和部署，该限制优先于此前部署授权。此后没有 SSH、音箱 HTTP、ubus、播放、设置、重启、录音或云会话。主代理维护 context/operations。

本机源码变化不会自动改变音箱当前工具目录。设备仍以第三十七轮最后确认的 Web/Duplex 与原 13 项工具为基线；本报告没有在禁令后重新核查设备状态。

## 禁令前操作与最后已知状态

先读取 AGENTS、README、context、operations、sources，再按有限 ubus 对象读取方法签名和返回结构。调查覆盖 mediaplayer、led、alarm、mibt、mibt_mesh、nightmode、sound_effect、linein、ai_crontab、ir_agent、miio、messagingagent、mibrain。

禁令前有两组可撤回试验：播放器睡眠计时 action=set/cancel 均返回 code162，读回 type0，未建立计时；创建一条七天后的临时 alarm，query确认新增，close读回status1、open读回status0，最后仅删除该临时ID并确认不存在。没有修改用户现有闹钟。alarm-query仅输出数量、字段类型或临时对象元数据，未输出用户闹钟文本/ID、账号、MAC或认证数据。

最后只读播放状态为status1/loop_type1，说明用户正在播放；没有暂停或改队列。本次能力调查没有音量、灯光、模式、蓝牙、Mi Home/Mesh设置写入，也没有候选部署。中断时没有运行中的设备命令；中断前最后操作是本机源文件编辑。

## 能力盘点与接入分类

“有ubus签名”“底层已读回”“本地候选已接目录”“新工具已实机验收”是四种不同状态。此次只有前面三种证据，新工具实机验收全部待允许后进行。

|类别|禁令前证据|本地候选|仍未接的操作及条件|
|---|---|---|---|
|网易云/播放器|第三十七轮原工具搜索→prepare→commit、ID/进度和单首/完整歌单已验证|原音乐工具保留，增加完整状态、恢复、模式读写、当前曲目定位|定位单位和numeric loop setter须实机读回；没有新增原始URL模型工具|
|音量|现有Web设置后回读实现；本轮只读媒体音量|模型音量读取/设置，设置后返回已核对的值|新目录入口未实机验证；不在用户播放时做验证|
|本地闹钟|alarm query/create/open/close/delete临时对象实测，status0=启用/1=关闭|列表、单次创建、按ID启用/关闭/删除、闹钟音量读写|modify的默认字段/保留语义、重复星期mask、厂商timer/reminder类型未证明，未包装成模型工具|
|播放睡眠计时|getter返回type0；set/cancel试验code162、无变化|只接状态读取|action字符串/时间语义不可靠，未连接setter或宣称已支持倒计时|
|蓝牙|mibt.status code0，enable/playing/discover、设备数组结构可读|状态与已配对/连接计数，地址/名称不返回模型|enable/connect/disconnect/play等签名存在，但目标设备映射、参数语义、与Mesh共用无线电影响未验证|
|Line-in|line_status/play_status结构已读取|连接/播放状态读取|start/stop/音量等可能切换正在播放来源，未验证幂等、切回及状态语义，未接|
|夜间模式|status/current为字符串配置与schedule|带必要字段校验的状态读取|set/sleep_mode配置枚举和调度效果未验证，未连接写入|
|音效|sound_effect.get_mode返回mode0/effect1|读取mode/effect数字|没有可靠“低音/人声”等映射和恢复验收，不暴露任意numeric set_mode|
|灯光|led.status空info表示无效果；已有Sidekick 1/2/3层所有权|灯状态、Sidekick反馈开关；新Duplex应用该策略|不开放shut_all或任意层/RGB，避免清除原厂指示；不靠旧lamp_clear的409分支冒充支持|
|Mi Home/Mesh|Mesh support/enable/running可读；miio/messagingagent主要是绑定/认证/推送入口|仅接Mesh网关状态|缺已授权的设备清单、MIoT属性/动作映射和真实状态读取协议；不暴露miot_push、raw RPC或原厂自由文本执行|
|红外|LX06存在ir_agent play/play_batch/learn/read签名，原厂IR进程保留|未列入模型目录|缺品牌/型号/已绑定设备及可审查的命令码表；发送确认不等于家电状态改变，不能包装任意IR code|
|设备状态|本地/proc与已知LX06平台|当前Unix秒、系统本地时间/时区/offset、uptime、可用内存、助手是否运行|不返回网络身份、账户、设备认证、日志正文，不接系统写入|
|其它原厂能力|voip/声纹/儿童模式/投播/ai_crontab等对象存在|没有以“存在对象”计为支持|缺具体用户任务、类型化参数、失败/回读及恢复证据；保留原服务，不包装万能调用|

恢复出厂、刷写/分区、OTA、删除认证、绑定重置、系统重启/关机、任意shell、任意ubus、任意原厂云端文本执行均不作为模型无确认工具。没有修改原厂启动项、驱动、ROM、唤醒训练或probe。

## 35项最终本地目录

|分组|工具|
|---|---|
|原音乐与会话13项|music_search, music_charts, music_chart_tracks, music_playlists, music_playlist_tracks, music_play, music_play_chart, music_play_playlist, music_next, music_previous, music_pause, music_stop, session_end|
|设备与音量3项|speaker_status, speaker_volume_get, speaker_volume_set|
|播放扩展5项|music_status, music_resume, music_seek, music_mode_get, music_mode_set|
|睡眠/音效读取2项|sleep_timer_get, sound_effect_get|
|闹钟6项|alarm_volume_get, alarm_volume_set, alarm_list, alarm_create, alarm_enabled_set, alarm_delete|
|灯反馈2项|lamp_status, lamp_feedback_set|
|其它状态4项|bluetooth_status, mesh_status, night_mode_status, line_in_status|

目录唯一来源仍是native_tools.h。Web的tool endpoint与RPC共用action_execute/cap_execute；voice client取目录并转发。模型目录version2，命令envelope仍v1。极简system prompt原句未改变。

music_chart_tracks与music_playlist_tracks同映射netease_chart_tracks并非按名字误路由：两种ID都通过provider playlist/detail接口，第三十七轮已读回榜单100/用户歌单538。此次模型路径新增offset/limit分页，默认20、最多20；LAN完整列表与600首完整播放队列保持。超出响应能力不静默丢条目。

## 输入、时间、结果和所有权

- 工具参数验证类型、required、未知字段、enum、范围和字符串长度；拒绝嵌入NUL。损坏JSON/错误arguments类型不再被替换成空对象后执行music_stop等无参数动作。URL/shell/认证透传不在目录。
- speaker_status返回unix_seconds、local_datetime、timezone、utc_offset、timezone_source=system_localtime，模型可先查设备时钟。没有假定设备已经处于Asia/Shanghai，也未在禁令后读时区；未来验收须核对时钟与用户本地时区一致。
- alarm_create只创建一次性alarm，默认volume18。时间契约三选一：timestamp，local_date YYYY-MM-DD+local_time HH:MM，或after_minutes。设备端解析mktime/localtime并核对归一化结果，拒绝不存在日期、DST跳跃空档、过去/不足一分钟、超过365天、超出32位epoch或多种契约混用。不是让模型单凭自身日期计算epoch。DST回拨的重复本地时刻按系统libc选择，未做跨时区实机验收。
- 闹钟写入先读取现有ID，写入后query核对；创建只接受恰好一条新的目标时间对象，删除确认目标不存在，开关确认status。列表只返回ID/时间/重复mask/音量/启用状态，用户event/extra/ringtone文本不向模型或日志扩散。
- volume/模式/定位/闹钟音量必须最终读回；定位核对同一audio_id和非负position，不把旧曲目/未知position当成功。模式只提供已有历史映射的repeat_all1和sequential2，不开放未证明的数字。
- cap_call按已观察回复格式区分：大多数方法必须code0；仅sound_effect.get_mode、linein状态和led.status允许其实际无code格式，仍校验必需字段。{}、错误类型或不完整状态返回ok0。保留numeric backend_code，传输/无效回复有明确标记；写入不确定时state_may_have_changed提醒先读取状态，不盲目重试。
- 蓝牙paired_count返回真实总数；连接统计最多检查64项，明确devices_checked/connected_count_complete，不能把截断后的数当全部。LED效果列表也返回total/complete字段。
- 新灯工具管理Sidekick反馈策略文件；新Duplex记住requested/owned状态，下一循环只撤销自己拥有的层，重新启用时恢复所请求层。旧lamp_clear仍保留其保护，不清原厂层，也不开放RGB任意层控制。新Web和新Duplex必须配套验收。
- Web单次action有20秒预算；provider请求和ubus等待尊重剩余时间，普通GET在新连接重置预算。保留第三十七轮本地短/r、32首元数据分批、malloc_trim和实际播放ID确认；新增deadline可能使慢网络下的大列表明确超时，须后续实机回归。

## 目录/传输/内存预算

真实C目录生成35项、9201字节；加入原极简prompt和音频配置的session.create模拟为9542字节。Web目录buffer提升到32768，bridge本地response仍32768，WS消息上限65536；packet6000与arguments3000边界未放宽成任意大请求。

每会话缓存调用预算64，单批最多8，单结果最多8192字节，去重缓存上限98304字节。9–64项批次逐项明确tool_batch_too_large且不执行；超过64项拒整批并让会话返回工具错误，不截成“前4项已执行”。缓存不足/结果过大有明确错误。聚合响应接近60000字节时返回result_truncated摘要并保留原成功/失败状态，完整结果留去重缓存；不暗中把body送不出后仍报全量结果。重复call_id不会再执行。实际执行与缓存/拒绝日志分开，不打印参数或结果正文。

## Mac验证与候选

运行 `sh tools/test-speaker-tools-local.sh`：

|验证|本地结果|
|---|---|
|Foundation JSON-backed capability mocks|146 checks；必需字段、code、类型、overflow、时间三契约、无效日期/DST、原闹钟ID保护、读回失败、NUL、隐私过滤、灯策略均通过|
|真实tb_worker+假工具执行器|259 checks；64预算、8批、去重、缓存/响应边界、票据移除与pause取消通过|
|从真实Web源码提取tool_reply执行|22 checks；坏arguments/NUL/未知工具/越界及未知字段不执行，通过|
|真实C目录与独立JSON schema检查|35项唯一名、参数/schema合法、时间oneOf、容量及危险工具缺席，通过|
|从真实Duplex源码提取feedback_led执行|自有层撤销/请求记忆/重新启用/原厂层保留，通过|
|既有duplex/timeout/session policy|full/half、工具期间上行、打断、跨70秒、20秒timeout、音乐确认截止及26项策略检查通过|

所有测试设备接口均为fake；Mac只写临时测试目录，未连接音箱或云服务。它们证明本地逻辑，不证明硬件已支持所有候选工具。

ARM/glibc 2.25 application candidates passed `-Wall -Wextra -Werror`, ReleaseSmall, and strip build checks; this step did not execute or rebuild the wake probe, or upload binaries.

- Web：build/lx06-web-capability-candidate-armhf，94024字节，SHA256 `ea787bb8ff5511cb0791c4cf67dad6dd5b40cdbea533f3aa2b128ed24cecb60a`。
- Duplex：build/lx06-duplex-capability-candidate-armhf，117476字节，SHA256 `2c575af55c9f20ae064162db2ba6d80a79725a5df5536340a7e2f7ea65dfe677`。
- 本地目录与validation文件在temp/analysis/speaker-capabilities/，不含账号、票据、音源URL或录音。

候选没有在ARM上运行，也没有验证云端接受扩展35项/oneOf schema；须在用户重新允许并主动进行会话后验收，不能今晚主动开云。json_object_object_length在本机原厂libjson-c副本中有符号字符串，尚未在禁令后实机验证新增符号加载。此次不能宣称新目录已经由音箱或云模型加载，也不能宣称新控制已经读回成功。

## 以后获准验收时的恢复与应用方案（本轮不执行）

先重新确认当时实际部署binary/PID/hash和用户播放状态；在任何设备写入前只读收当前Web/Duplex/start/stop以及灯策略原状态到Mac private/deploy-recovery/新目录，0600并核SHA256。不能仅把较早e1/efe备份称作当前第三十七轮完整恢复包。设备不留历史副本，staging逐个核哈希后替换并清理。

先测只读新目录、时区、状态字段、ABI符号和资源；可撤回设置逐项保存/恢复原值，不能在用户播放时测试seek/resume/mode或替换队列。未来闹钟测试只创建明确命名的远期临时对象、按返回ID删除并核对原列表未变。灯反馈先验证新Web/Duplex版本配套，不用shut_all。音乐短/r、原工具prepare→commit、目标ID/进度和完整歌单批次必须回归，慢网络deadline须测试失败反馈。

如果验收失败，停止精确匹配的应用进程，从Mac恢复上述实际当前副本、核SHA/chmod后按原受保护stdin方式启动；原厂服务/ROM/认证不动。今晚没有保存新的设备恢复副本、staging、替换、启动或恢复操作，任何后续设备步骤都需要用户恢复这项授权。


## 本地入口复核补丁

主代理复核发现bridge仍将过大/错误类型arguments替换成空对象，可能绕过Web拒绝逻辑。现worker要求每个模型call显式含arguments字符串（无参数也必须是字符串`"{}"`），校验json-c原始字符串长度与strlen一致、≤3000字节，并要求一个完整JSON对象；缺失、非string、NUL、数组/标量、坏JSON、尾随垃圾或注释扩展均返回invalid_tool_arguments，不调用后端。tb_call再次独立防御过大/缺失/非对象字符串，绝不替换为`{}`。id/name/action等bridge字符串读取也检查NUL长度。

真实worker mock针对music_stop与session_end分别覆盖上述无效输入，断言后端执行次数0、end_session/music_touched未改变及错误明确；直接tb_call非法输入也在任何socket操作前返回错误。bridge检查累计259项；endpoint22项与本地总体回归再次通过，ARM候选重新构建。本地Web HTTP工具入口仍可接受对象形式arguments或缺省空对象供本机手工调用，模型worker协议不会对错误/缺失字段默认空对象。这次补丁仍LOCAL ONLY、未部署，未访问设备或云。
