# Report: 聚合音乐环境方案评估（2026-11-01）
> 历史验证记录：本文保留对应阶段的实验条件、数据和限制；其中设备进程、部署位置与恢复目录只描述当时状态，不是当前部署说明。当前操作边界见项目的有效 Reference 文档。

用户新授权：优先音箱内解决，必要时NAS或路由器承接部分功能；目标扩大为聚合音乐环境，不限网易云。NAS/路由器型号与Docker支持已询问，尚未获得回答。本轮不安装服务、不启动音乐或云端语音会话。

## LX06 资源与接口约束

logs/music-architecture-runtime.txt：MemTotal118760KiB，可用43784KiB，无swap；/data可用8228KiB，/tmp为tmpfs（不是可增加容量的磁盘）；get_dlna_status返回info0，本轮未启用。原生mediaplayer有player_play_url、音量/进度/循环/播放操作接口，签名在tools-capability-inventory.txt。

原厂usr/lib/libffmpeg-miplayer.so（1453948B）静态符号有MP3/AAC/FLAC/Opus/Vorbis解码器、HTTP/HTTPS协议、HLS/MP3/FLAC解封装器。证明组件已编入，不证明播放器暴露所有能力、每种流可成功播放或高码率资源足够。未播放测试。后续须验证URL、重定向、Range、Content-Length/分块、TLS、首播/暂停/seek/连续换曲。

流式播放只需有界缓冲，不必把一首歌下载到/data，也不应把大缓存放/tmp侵占RAM。示例：320kbit/s缓冲5秒为200000B（约195KiB）；CD规格PCM 44100*2ch*2B约176400B/s，缓冲5秒约861KiB；未计解码器/TLS/栈/队列。若云端地址可直接播放，NAS只做控制/鉴权，音频可从平台直达音箱；需要请求头、特殊封装或转码才考虑NAS代理流。

## 方案比较

|方案|适合位置|优势|限制/结论|
|---|---|---|---|
|自研小型C适配层+原生播放器|音箱优先|复用已存在TLS/JSON/音频库、少量状态与有界缓冲，可统一多个音源|每个平台登录/签名/续期需要开发维护；不能承诺完整会员客户端等价；先做网易云单平台闭环再扩展|
|Music Assistant+网易云API服务|NAS|现成多平台聚合、曲库、搜索、队列、推荐，QQ/网易云和本地媒体|官方当前要求64位系统、至少2GB RAM；不能放LX06。网易云另需自建API服务；实际提供商版本/账户权限逐项测。优先NAS备选|
|Navidrome/OpenSubsonic|NAS，强路由器可评估|自有文件曲库、搜索/歌单/流式与转码；可被聚合器接入|主要是自有曲库服务器，本身不解决网易云/QQ会员账户；不作为完整云音乐聚合替代|
|XiaoMusic|NAS参考|面向小爱音箱，便于参考音箱控制及自有媒体链路|原项目已于2026-06-22归档；不是本项目豆包机内工具链，不能据此承诺会员聚合能力|
|MusicFree|仅参考音源插件抽象|搜索/歌单/播放地址等接口设计可借鉴|手机端Android/HarmonyOS/ReactNative应用，不能直接安装到LX06；插件功能不等于会员授权，需独立检查|

路由器优先只承接轻量HTTP媒体服务、简单代理和少量适配；不在型号未知时建议安装Docker/MA或大量转码，避免影响路由本职。实际强x86软路由可按服务器能力另评估。

## 推荐架构

豆包tools调用统一music接口：search、get_playlist、enqueue/play、pause/resume、next/previous、seek、get_state。返回包含source、source_track_id、title、artists、duration、可用质量和权限状态；不能仅按歌名把不同版本合并。

音源层分别实现网易云、QQ、HTTP自有文件、OpenSubsonic等provider；密钥/登录状态留在provider执行处，豆包只拿最少元数据，不传Cookie或带鉴权的播放URL。音箱native renderer执行播放、报告实际状态、负责与豆包TTS/原厂音乐的抢占/恢复。先原生play_url，必要时再小型代理/自有解码。

同一个音源API既可在音箱内调用，也可后续移到NAS。平台直链能用就直连；代理/转码可选，不强制所有音频绕NAS。队列必须有唯一控制方，避免MA和音箱各自切歌。NAS不可用时音箱仍保留小爱和豆包基本语音，不应造成启动阻塞。

## 实施步骤与验收条件

1. 用自有短样本验证原厂URL流式播放及MP3/FLAC，记录实际缓冲、CPU/RSS、暂停/seek/断流。测试前保存播放状态，不干扰正在播放内容；可先在PC临时提供测试HTTP源，PC不成为最终依赖。
2. 接入网易云有效账号，验证一首手机端有权限的会员歌完整播放，检查返回是否试听/权限不足；不假定拿到URL即代表全曲可听。
3. 做统一provider和队列接口，豆包tools接搜索/播放/切歌，回报真实执行结果；测试语音时暂停音乐、退出恢复的仲裁。
4. 达到资源上限或平台适配维护成本过高时，将同一provider服务移到NAS；评估MA而非无条件重写聚合器。
5. 增加QQ/自有曲库与跨来源搜索。各平台会员/地区/专辑权限保持独立；无损/高采样率逐项验收，不能把有FLAC解码器当作192k/24bit全链路支持。

## 查阅的一手来源

- https://www.music-assistant.io/installation/ ：64位、至少2GB RAM，Docker或HA App；硬件要求不满足LX06。
- https://www.music-assistant.io/music-providers/ ：跨来源统一曲库与元数据合并，匹配不保证。
- https://www.music-assistant.io/music-providers/netease-cloud-music/ ：扫码、单向曲库同步、搜索/收藏/推荐、最高HiRes FLAC，另需NeteaseCloudMusicApi，权限按账户。
- https://www.music-assistant.io/music-providers/qqmusic/ ：扫码、搜索与单向同步，QQ端变更可破坏第三方接入。
- https://www.music-assistant.io/player-support/dlna/ ：支持DLNA但兼容差异/队列flow/seek/转码需测，不能承诺LX06自动兼容。
- https://www.navidrome.org/docs/overview/ ：自有曲库、OpenSubsonic、实时转码。
- https://github.com/hanxi/xiaomusic ：小爱音乐方案，页面显示2026-06-22归档。
- https://github.com/maotoumao/MusicFree ：插件化移动播放器；不是机内Linux无头服务器。
`n硬件补充：用户回复“arm 支持 docker”，位宽、RAM和具体设备仍待确认。
