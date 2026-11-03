# Report: Full-Duplex Upload and Wake Response Policy (Round 36)

2026-11-03。当前已部署默认全双工、正常唤醒短 ding、AEC 云输入失败关闭、20 秒追问/无语音/无进展超时，以及 30 分钟会话安全上限。待机只做本地唤醒，没有云连接预热。用户撤回的待机预连接、长期闲置会话和费用对比均没有实施或测试。

## 当前行为与实现

`native_agent_loop.h` 的 full 模式在模型回答与工具执行期间继续上行，不发送播放/工具 mute/unmute；保留 answer_mode、播放结束与追问状态。回答期间普通服务端 ASR.started 会发送 response.cancel 并清本地播放，无需再喊关键词。half 环境覆盖仍可供恢复诊断。

正常有 Key 唤醒只播放本地 100ms/400Hz/PCM 峰值 1200 的 ding 并保留 LED；ding 排入已有播放线程，不等播放结束再建连接。没有本地“收到”语言回复。无 Key 仅播放 missing_token 提示。模型指令为：你是豆包，用用户的语言简短回答。需要操作音箱时调用对应工具；工具未成功，不要声称成功。

保留完整两秒 preroll，不裁唤醒词。`native_wake_handoff.h` 在唤醒到 TLS/Upgrade/会话建立及退出清理期间由单一线程持续读取同一 ALSA/AEC 流，用有界 16 秒内存队列交接，不写录音。队列溢出或采集错误失败关闭，清理期间继续维护 AEC 并丢弃样本，退出后 join，再恢复 KWS 主循环读取。

`native_capture.h` 新增 `lx_capture_cloud_ready`：必须同时满足 capture、aec、非 failed、echo、pp、ecancel 和 prun。run_session 在建立任何云连接前检查；preroll 与每次 agent_send_audio 再检查，agent_capture 也检查。plain mico_record 回退仍供本地诊断/唤醒使用，不能进入云会话。`--voice-fixture`、`--voice-aec-fixture` 云 WAV 回放禁用；MODE_KWS_WAV 的本地唤醒检测仍可用，但其 WAV preroll 不得上传。此前 synthetic fixture 的云协议结果只作为历史证据，当前不能用旧入口重跑。

启动默认 `LX_SESSION_MAX=1800`、`LX_FOLLOWUP=20`、`LX_NO_SPEECH=20`、`LX_NO_RESPONSE=20`。会话安全上限不是正常关闭时间；ASR/输入/文本/音频/工具结果进展更新回合进展时钟，等待回答期间不按会话起点硬切。完整回答播放后进入 20 秒追问窗口。没有等待回答时，无语音 20 秒结束。工具仍执行或本地播放尚未空闲时不误用 no_response 结束；这些情况由 30 分钟异常安全上限兜底。

延迟日志分出 setup_to_tls、curl DNS/TCP/TLS、TLS→Upgrade、Upgrade→session_ready、ready→first_ASR、first_ASR→first_audio 以及 wake→first_audio。首音频指标是收到云端首音频事件，不能表述为声卡实际首次出声。新指标已编译部署，本轮没有为采集指标额外开云会话。

## 验证证据

本机 mock 实测：`tools/test-agent-duplex.c` 验证 full 回答/工具期间上行、无 mute、half 回归、普通 ASR 打断/cancel、会话安全上限和单轮结束通过。`native_session_policy_test.c` 26 项通过。`tools/test-wake-handoff.c` 验证单读取者、样本顺序、内存溢出失败关闭、discard、join 和重启通过。

`tools/test-agent-timeouts.c` 使用注入的加速单调时钟，避免真实录音或云上传：70 秒连续输出后进入追问，92 秒退出；70 秒输入转写持续进展后，90 秒无进展退出；首回合不回答在 21 秒退出；初始静默在 21 秒退出（当前循环条件为 >20 秒）；迟到 canceled-response done 不计新回合，24 秒无进展退出；工具 15 秒执行期间继续上行，结果/回答后正常追问退出。这证明代码能跨过 60 秒，没有证明真实环境已经完成一段 >60 秒的人类对话。

`tools/test-cloud-aec-guard.c` 验证 NULL/plain、failed、缺 echo/pp/ecancel/prun 均拒绝，完整 AEC 状态接受。ARM 构建 `sh tools/build-factory-wake.sh` 通过。设备实测旧云 fixture 入口返回 2、打印 disabled；`LX_SESSION_MAX=1801` 返回 2。没有新录音，没有新的 synthetic 云输入。

此前同一 5.86 秒双句 synthetic 的有界云协议对照：half 第二句不上行；full 接到第二次 input_started，普通第二句数学问题（不含唤醒词）被识别，服务端 ASR 打断，during_answer=180，回答 1+1=2。15 秒 synthetic 数学句后接实际 AEC 麦克风尾流的旧入口测试记录 input_started=1、audio_done=1、during_answer=455、dropped=0，没有重复自对话；当时未落实际麦克风录音或真实转写。这只支持有限测试范围内的回声控制，不宣称消除了所有实景回声。后续正式日志已有多回合/工具期间上传和真实打断事件，但未凭日志判断这些输入是人声还是自回声。

## 部署与恢复

最终设备 `/data/lx06-agent/lx06-duplex-armhf` SHA256：`efe0fd89eba180edfc83f81305b4b8b6a49517655b32580dde7798b80ec641c9`，PID 2794。start.sh：`9393a0ea1752e033e110714ecae5222ea97e22f8df0083b017a10d265f0185b8`；stop.sh：`7c305fdcfae4682593707a84e0d8cca83072f068afa850f9aa957e4236940189`。start 打印 safety=1800/no_speech=20，capture 打印 AEC Capture/8ch/mix6/48k。

当前待机日志 standby_entries=1、cloud_connected=0。进程唯一 socket inode 494669 映射 `/proc/net/unix`，没有该进程 TCP/UDP socket；实测快照支持待机无云连接。源码只在检测到唤醒后进入 run_session。该快照不是永久网络监控。

miio 1585、central 2440、Mesh 3518、mediaplayer 3214、mico_aivs_lab 3667 存活。进一步只读核查确认旧 mipns-xiaomi PID3576 被 OOM kill：内核 uptime `170959.531553`，被杀进程 anon-rss `10552kB`。当前 `/usr/bin/mipns-xiaomi` 已为 PID1629，PPid=1、State=S、40线程、RSS10988kB；start_ticks=17096454，核查时 uptime=172068.81。按 Linux 常用 USER_HZ=100 换算约在 OOM 后5秒重启，时间换算为推断；`/etc/init.d/pns` 实测 USE_PROCD=1、respawn参数 `3600 5 0`，pns status返回0，二次检查新PID仍存活。这与 procd 5秒监护恢复一致，无需再次重启。OOM/自动恢复约发生在本次最终部署前18分钟，不能把它归因于这次二进制替换，也不能凭进程存在宣称米家/推送端到端功能正常。设备可用内存偏紧（本次 free: total118760kB、free5864kB、available26812kB、无swap）；只读核查未增加后台云会话或录音。

覆盖前把先前部署 binary `c423918f0e4ed3b84320a8e1c2670962f0e56ad65513c981ae439d8ee279e58f` 与 start/stop 收入 Mac `private/deploy-recovery/r36/timeout-prior/`，0600；更早恢复资料仍在 Mac r36，不覆盖。所有 staging 上传 SHA256 校验后才替换，设备没有历史备份，staging 已清。凭据只从 Mac private JSON 内存读取，经 SSH stdin 传递，不在 argv、工具输出或仓库记录。

恢复：停止当前 duplex，从 Mac timeout-prior 上传上述三个文件到临时 staging，逐个校验哈希，chmod700 后替换，再用同样受保护 stdin 启动；恢复文件只留 Mac。旧 stop 的 ps 全文 grep 曾杀含 binary cp 字符串的 SSH 父 shell，现改 loader 候选实际 argv 精确匹配；keywriter 以 comm 标签识别，避免陈旧 PID 误杀。不要恢复更早未修复的 stop 作为常规回滚。

唤醒模型参数此次不变；wake recall/MFCC 排查由父代理/训练任务负责。用户所述静音 6 tokens/秒没有本轮独立核验，未做账单对比，不能承诺特定费用收益。

## 音乐故障续查（进行中）

用户报告豆包不能播音乐后，只读查旧2794日志：music_search/music_play各2次、session_stop=music_queued、turns=9、frames=6042，随后确有 music_deferred_commit=fail、session=pass、standby_resumed。因此 session=pass 只代表语音传输清理成功，不代表音乐成功；检查时daemon单线程回standby，未卡在cleanup。private play_ticket TTL90秒，而反复打断/追问可拖长确认，存在过期风险，但旧日志没有ticket_age，不能断言旧故障是过期。

续修 `native_agent_loop.h`：真实票据出现后最多等待15秒模型确认，正常回答播放完仍立即music_queued；仅有票据的音乐启动路径适用，普通持续对话不受此限。加速mock持续输出验证16秒music_queued通过。`native_tool_bridge.h`记录ticket_ms，对music_pause/stop/next/previous清理本地票据，并把netease动作计入music_touched，避免旧音乐错误自动resume。commit日志新增安全的age、是否收到响应、错误标识，按JSON整数解析ok。新增有界`--music-tool-smoke`调用原music_play工具prepare后原music.netease.commit，不开云、不保存URL/票据、不绕过工具。失败返回1的候选修正尚待最后一起部署。

实际工具验收：现有公开音乐搜索“夜的钢琴曲五 石进”返回ID149297；music_play标准音质prepare成功，但2ms内commit失败pending_playback_not_confirmed，足以排除该次票据过期或cleanup拖延，实际播放器问题尚待修复。单首nc_play允许保存长度<901的原CDN URL；commit原样传lx_music_play_queue，后者拒绝>=160字节URL；榜单已有本地短/r重定向路径。随后设备loopback HEAD `/r/netease/149297/standard.mp3`实测返回Location，CDN URL长度286，明确超过160上限；Location只在Mac内存解析，没有输出或写文件。单首队列URL长度不兼容根因已实证，已交父代理修web/netease短URL及细分错误元数据后再验收。

本阶段已部署duplex `3e5fd75cc9debdfe2d6d56247ccbd273d990a8f2f5368a19b6ac108b01648ae6`、PID3488，start/stop不变；恢复前版efe0和安全脚本只留Mac `private/deploy-recovery/r36/music-prior/`。新CLI失败退出码修正候选为 `435477cfb919e2a72df931cfad268317771cd751060e3cd8b12ba4c418f4f12e`，编译通过，尚未部署。不能宣称音乐已修好。


音乐续查已在第三十七轮完成，最终Web/Duplex部署与实际播放证据见 `music-tool-short-url-and-playback-confirmation-fix.md`。本报告前述进行中状态为当时记录；最新部署以第三十七轮报告为准。
