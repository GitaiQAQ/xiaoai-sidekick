# Report: Music Tool Short URL and Playback Confirmation Fix (Round 37)

2026-11-03。用户报告豆包调用工具后仍不能播音乐。本轮已修复单首工具与直接播放的URL队列格式不兼容、播放器启动时本地重定向服务阻塞、以及完整歌单查询/prepare时的内存峰值；原music_search→music_play→music.netease.commit已实测成功，并核对实际播放器歌曲ID和进度增长。测试播放已结束。未改极简模型prompt，不添加音乐强引导，不新开云会话。

## 根因与修复

旧语音日志存在music_search/music_play各2次、session_stop=music_queued、turns9、frames6042，最后music_deferred_commit=fail；daemon已回standby，非一直卡cleanup。旧日志没有ticket_age，不能归因于票据过期，也不能把session=pass解释为音乐成功。

独立原工具验收选择公开歌曲“夜的钢琴曲五 / 石进”，ID149297。prepare成功，commit在2ms返回pending_playback_not_confirmed。设备loopback HEAD短地址的Location仅在Mac内存解析，原CDN URL实测长度286；native_music.h拒绝>=160字节队列URL，单首nc_play却允许保存<901字节原CDN URL。榜单/歌单已经用本地短/r地址。2ms失败及真实长度证据证明单首接口不兼容；未输出URL或票据。

`src/native_netease.h`的单首deferred和直接播放现在都调用nc_local_url，以52字节loopback稳定地址排队；保留先验证当前账户能取得完整非试听流的逻辑。播放器读取/r时按ID和音质重新取得有效流，不把短期CDN地址存入队列，也没有放宽160字节限制。

短地址初次修复后，播放器实际稍后启动，却工具仍fail：Web在等待ubus结果时无法处理同一Web的/r请求，ubus1秒超时。`src/native_web_console.c`添加仅用于播放启动等待的请求处理：只接loopback GET/HEAD /r，其他并发请求返回playback_start_in_progress，禁止递归工具或mutation。`src/native_music.h`在player_play_music的ubus等待及实际状态确认期间处理重定向；该ubus命令有界5秒，外层子进程等待仍有8秒上限。工具成功要求ubus确认且实际status1、audio_id匹配首首目标，不能被另一首已有播放误判成功。

commit日志只输出ticket present/matched/expired、短URL长度及队列数量，不输出票据/URL。失败分别返回pending_ticket_missing、pending_ticket_mismatch、pending_ticket_expired或playback_not_confirmed。Duplex记录ticket_age、收到响应与安全错误标识，JSON整数判断ok。

真实票据出现后，Duplex最多等待15秒模型确认；正常回答结束仍立即退出会话提交。该截止仅作用于已排队音乐，防多回合反复打断拖过90秒票据期限，不限制普通持续对话。pause/stop/next/previous清本地旧票据，netease动作计入music_touched，避免误恢复旧音乐。正常语音仍需模型真正调用原工具；新增--music-tool-smoke只是有界设备诊断，用同一原工具prepare→commit，不开云、不绕过控制接口，失败返回1。

## 全歌单内存问题与恢复

实际验收后完整歌单查询及prepare暴露设备OOM：Web3921、464被内核杀掉，anon-rss分别10296、11664kB；这两次实测期间LAN一度拒绝连接，已立即恢复，不能把先前音乐通过当作整机一直正常。当前机内总内存约116MB且没有swap，具体触发时其他进程内存占用没有完整采样，不把OOM完全归因于某一个分配。

原实现一次解析完整provider歌单大JSON，释放后还可能保留heap页，且擦除响应时写满4MB容量。现保留600首上限：先以n0读取trackIds，再按32首请求song/detail，只保留ID、name、artist用于队列；保留顺序并分批释放provider对象，不一次解析538首的完整大对象。响应只擦实际收到字节后释放，每次Web请求完成后通过factory glibc malloc_trim归还空闲页。没有减回20首，也没有绕过账户认证或试听限制。

改后原music_play_chart prepare100首、music_play_playlist prepare538首均成功；没有commit这些多首测试队列，后续仅在仍是本轮测试歌且status2时清pending票据。完整metadata再次读取538首通过，Web存活，复核RSS约2440kB。进程存活与本轮测试支持峰值/驻留改善，不能承诺永不OOM。

## 最终实测结果

最终构建部署后原工具链：music_search、music_play都执行；music_smoke_prepared1，music_smoke_commit1，response received，error none，ticket_age1477ms，诊断退出0。播放器status1、audio_id=netease-149297、queue_total1、queue_index1；四秒采样进度0→3108ms，duration131004ms。

同首现有直接music.netease.play路径也返回ok1；status1、ID/单首队列匹配，进度0→3380ms。随后再次核对audio_id仍属于本轮测试歌，调用music.stop返回ok1、status2；没有结束他人已替换的队列。

只读回归：真实charts65项、chart tracks100、authenticated playlists50项、playlist tracks538项；未打印私人歌单名、账号ID或认证信息。原chart/playlist prepare回归分别100/538首成功，不启动这些测试队列。Web现有parser自测7项通过；Duplex full/half、20秒timeout与音乐15秒确认截止mock通过。没有声称真实人类语音点歌链已被本轮云端重放验证；本轮测的是相同本地工具与真实播放器完整执行。

## 部署、原厂状态与恢复

最终Web `/data/lx06-agent/lx06-web-armhf` SHA256 `55aa0dcf928a5244777fe10631b36196f6655630f508c810c633695a354f5c98`，PID536。

最终Duplex SHA256 `435477cfb919e2a72df931cfad268317771cd751060e3cd8b12ba4c418f4f12e`，PID3796。start SHA256 `9393a0ea1752e033e110714ecae5222ea97e22f8df0083b017a10d265f0185b8`、stop `7c305fdcfae4682593707a84e0d8cca83072f068afa850f9aa957e4236940189`。

构建：`sh tools/build-native-web.sh`（新增可复现脚本）、`sh tools/build-factory-wake.sh`。Web包含compiler-rt整数除法支持，经设备运行验证；两者都由factory loader启动，不改驱动或固件。staging逐文件SHA256验证再替换，凭据从Mac private JSON内存读后SSH stdin传递，未写入argv或工具输出。

原Web e1e0230...恢复副本只留Mac `private/deploy-recovery/r37/lx06-web-armhf`，0600；Duplex旧efe0及安全脚本只留Mac `private/deploy-recovery/r36/music-prior/`。恢复需停止精确匹配的对应进程，从Mac上传副本到staging、核对SHA256、chmod700并替换，再用受保护stdin启动；设备没有历史备份，staging已清。

最终原厂mipns1629、mico_aivs_lab3667、miio1585、central2440、Mesh3518、mediaplayer3214均存活。未作米家/推送端到端验收，不能仅凭这些PID宣称所有功能正常。

Duplex最终standby_entries1/cloud_connected0，唯一socket inode496486映射/proc/net/unix，没有该进程TCP/UDP；没有待机预连接。保留r36的AEC失败关闭、两秒完整preroll、短ding、全双工与30分钟异常安全上限/20秒正常关闭策略，唤醒模型参数不变。
