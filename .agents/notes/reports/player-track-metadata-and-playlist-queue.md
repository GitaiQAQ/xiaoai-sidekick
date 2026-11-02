# Report: 播放器曲目信息与歌单队列：第十八轮
> 历史验证记录：本文保留对应阶段的实验条件、数据和限制；其中设备进程、部署位置与恢复目录只描述当时状态，不是当前部署说明。当前操作边界见项目的有效 Reference 文档。

## 功能行为

播放器卡片现在读取原厂播放状态中的当前 `audio_id`，并与 Sidekick 保存在进程内的网易云曲目表关联，显示歌名、歌手、歌单名、队列位置和进度。页面增加上一首/下一首；控制调用原厂 `player_play_index`。网易云页面可以读取榜单、登录账号的歌单和歌单前 20 首，也可以将榜单或个人歌单前 20 首加入原厂播放器队列。豆包实时语音工具加入读取个人歌单、读取曲目、播放歌单、上一首和下一首。

音箱原厂播放器仍负责解码、流式读取、自动切曲、循环模式和进度。Sidekick 只把网易云会员接口返回的短期音源 URL 与曲目元信息交给原厂队列，不缓存歌曲文件、不转发音频。网易云会话仍留在原有 `netease.session`；本轮没有读取或输出 Cookie、账号身份或密钥。

## 播放器所有权与曲目映射

原厂 mediaplayer 提供 `player_get_play_status`，返回 `play_song_detail.audio_id`、位置、时长和 `track_list`；另有 `player_play_index` 接口。函数 `parse_audio_player` 会遍历 `audio_items`，队列由原厂服务维护。接口盘点及反汇编摘录分别保存在 `logs/tools-capability-inventory.txt` 与 `temp/analysis/usr_bin_mediaplayer-annotated.txt`。

此前读取到的自有直连曲目状态只含固定 `audio_id`，所以播放器卡片只能显示“正在音箱播放”。本轮为网易云队列使用每首歌独有的 `netease-<歌曲ID>`，并在 Sidekick 进程内保存对应标题和歌手；状态轮询据此得出当前曲目及其索引。原厂队列保存实际播放顺序，不另造一个后台播放器。

## 未覆盖行为

- 当前每次从榜单或个人歌单装入前 20 首。歌单更长时，后续页面不自动续装；这样避免预先解析整张长歌单的短期 CDN 地址。
- 队列 URL 在开始播放前一次性取得。暂停很久后再切到尚未播放的曲目时，网易云临时链接可能过期；自动刷新与原厂队列续装尚未实现。
- 首页曲目名称依赖 Sidekick 本进程保存的队列元信息。音箱重启、Web 服务重启，或改由原厂小爱/其他应用替换队列后，原厂状态可能仍可读，但 Sidekick 不会把未知 `audio_id` 猜成标题。
- 本轮完成交叉编译和 ELF 架构检查并部署；没有进行页面/接口自动测试或新一轮真人语音验证。因此“我的歌单”端点、队列切歌索引基数以及设备实际返回的网易云 `audio_id` 映射仍需后续现场确认。
- 豆包工具变更已随二进制部署，但尚未做真实语音调用验证。原厂小爱主动调用这些新工具尚未接入。

## 历史部署记录

当时部署前停止了 Sidekick 唤醒监听，并确认原厂 `mipns-xiaomi` 与 `mico_aivs_lab` 仍运行。旧文件曾暂存在设备 `/data/lx06-agent/revision18-original/`（现已废弃此做法）；网易云会话未修改。该阶段 Web PID 3446 监听 18080，豆包语音停止且未配置开机自启。此历史目录不是当前恢复来源。

本地源码构建产物：

- Web：`build/lx06-web-armhf`，SHA-256 `E45B9CB98F49E50E8D21DBE7F6F3D43FBF99D47074C62BA4688F049518071F2A`
- Duplex：`build/lx06-duplex-armhf`，SHA-256 `3D561922CB32B946344C5B4DFACC4B7850F6F62737FADBE90E2D5DA8A4C65F9D`

部署时两者均按 ELF32 ARM、原厂 glibc 2.25 显式加载检查通过。设备文件哈希和进程快照见 `logs/queue-deploy-state.txt`；部署前备份见 `logs/queue-revision18-backup.txt`。

当时的回滚步骤是停止 Web 和 Sidekick 语音进程，从上述设备目录恢复文件，并保留 `/data/lx06-agent/netease.session`。该步骤仅用于解释本轮历史操作；当前回滚材料应从 Mac `private/deploy-recovery/` 读取并校验。


最终又部署了保存会话自动刷新的 Web 构建，PID 为 3446。当时保留了功能改动前版本和上一版队列版本作为比较基线；这些设备端副本属于历史做法。
