# Reference: 研究来源

第二十四轮以本机 ELF 指令、运行配置固定字段和隔离进程实验为证据，未新增外部资料。见[原厂两级判醒与独立引擎调用](../reports/factory-two-stage-wake-engine.md)：确认原厂模型初始化/推理可独立调用，实际观察到一级候选后二级超时拒绝；不等于已有“豆包豆包”模型或已修复误激活。

第二十三轮：原厂算法证据以本机 libxaudio_engine.so 的 Rice/Paddy、MFCC/CMVN、Viterbi/IsKeyword 与两级缓存调用反汇编为准，见[报告](../reports/music-trigger-and-factory-wake-review.md)。备选方案查阅 [Speex 官方源码](https://github.com/xiph/speexdsp/blob/master/libspeexdsp/mdf.c)、[Sherpa 官方 KWS 文档](https://k2-fsa.github.io/sherpa/onnx/kws/index.html)、[ARM 构建脚本](https://github.com/k2-fsa/sherpa-onnx/blob/v1.12.15/.github/workflows/arm-linux-gnueabihf.yaml)及 [Open-XiaoAI KWS](https://github.com/idootop/open-xiaoai/blob/main/examples/kws/README.md)。这些公开项目不是本机原厂 KWS 的源码；本轮 PC 实验未通过真人录音召回与 ARM ABI 兼容性检查，未部署。

- https://github.com/idootop/open-xiaoai
- https://github.com/idootop/open-xiaoai/blob/main/docs/flash.md
- https://github.com/idootop/open-xiaoai/blob/main/packages/client-patch/README.md
- https://github.com/idootop/open-xiaoai/blob/main/packages/client-rust/README.md
- https://github.com/duhow/xiaoai-patch
- https://github.com/duhow/xiaoai-patch/blob/master/research/lx06/info.md
- https://github.com/duhow/xiaoai-patch/blob/master/research/lx06/install.md
- https://xuanxuanblingbling.github.io/iot/2022/09/16/mi/ （旧版本研究，不保证适用本机）
- https://www.52audio.com/archives/38303.html
- https://docs.u-boot-project.org/en/v2026.10/board/amlogic/boot-flow.html

以上目前是来源索引，不代表源代码或网页已完整下载入项目。

- 本轮主要证据为本机原厂ELF、运行元数据和src中的隔离实验；见reports/native-program-boundaries.md。

第五轮直接证据/格式辅助来源（2026-11-01）：
- 本机/proc线程/FD/Unix inode映射、定点控制结构以及用户配合真实会话，是主要证据；详见第五轮报告。
- https://raw.githubusercontent.com/torvalds/linux/v4.9/arch/arm64/kernel/ptrace.c ：ARM32兼容任务r12入口/出口标记；已在本机合成进程验证。
- https://raw.githubusercontent.com/liteserver/binn/master/src/binn.h ：容器/标量tag对照；本机blob固定4B长度与当前格式不同，仅称观测方言。
- https://android.googlesource.com/platform/external/openfst/+/3da1eb108d36da35333b2d655202791af854996b/src/lib/fst.cc ：Google原始OpenFST FstHeader字段顺序/魔数；本机graph解析完整消费490B。
- https://raw.githubusercontent.com/alsa-project/alsa-lib/master/include/pcm.h ：ALSA格式值2的S16_LE语义对照；本机参数由原厂控制结构读取。

第六轮（2026-11-01）：
- 原厂库版本/符号来自实机C程序，TLS来自该程序固定HEAD实测；失败与厂商coredump副作用也记录在第六轮报告。
- https://raw.githubusercontent.com/curl/curl/curl-7_55_1/include/curl/curl.h ：按实机版本下载头文件和COPYING到PC tools，未安装设备。
- https://docs.volcengine.com/docs/doubao-voice/end-to-end-real-time-voice-full-duplex-version?lang=zh ：官方Seeduplex3.0新JSON事件/鉴权/音频格式；web直接open超时，通过官方搜索正文核对部分字段，完整SDK示例尚待下载。
- https://docs.volcengine.com/docs/doubao-voice/access-must-read?lang=zh ：官方新旧事件对照。只依赖官方原始接口信息，不把第三方旧版客户端当3.0实现。

第七轮ABI参考：https://raw.githubusercontent.com/json-c/json-c/json-c-0.12-20140410/json_object.h （PC tools/json-c-reference原始头；原厂ELF核对符号）。Ark POST schema官方：https://www.volcengine.com/docs/82379/1494384 （前轮已核对；客户端尚未认证实测）。

第七轮传输依据：https://curl.se/libcurl/c/CURLOPT_CONNECT_ONLY.html （设备7.55.1只用1）；https://datatracker.ietf.org/doc/html/rfc6455 （后续握手/掩码/分片依据）。本轮实机收到400，不等于WebSocket会话成功。

第八轮：https://docs.openssl.org/1.0.2/man3/sha/ （实机同代SHA1 ABI），RFC6455固定握手/掩码向量。原厂ELF新集中证据native-asr-boundary-disassembly.txt；原始audio-format的1ef44..1ef7c为ASR缓存半缓冲选择证据。

第九轮（2026-11-01）：
- 本机原厂libxaudio_engine.so反汇编与伪造对象动态测试为主要证据；见temp/analysis/native-dnn-frame-layout.md与logs/native-afe-frame-runtime.txt。
- https://docs.volcengine.com/docs/doubao-voice/end-to-end-real-time-voice-full-duplex-version?lang=zh ：本轮web抓取到正文（页面更新2026-10-27），确认JSON事件协议、X-Api-Key、model 1.2.6.1、16k/24k音频约束、mute/commit/cancel/close语义与全部事件名。
- https://docs.volcengine.com/docs/6561/1257544?lang=zh ：官方音色列表，S2S主打音色ID（Vivi/小何/云舟/小天 2.0）。
- RFC6455与libcrypto Accept计算沿用第八轮来源，仅用于本机回环演练，未连真实云端。
聚合音乐本轮来源（2026-11-01）：Music Assistant installation/music-providers/netease-cloud-music/qqmusic/player-support/dlna，https://www.navidrome.org/docs/overview/ ，https://github.com/hanxi/xiaomusic ，https://github.com/maotoumao/MusicFree 。完整链接、公开资料与机内证据区分见reports/aggregated-music-environment-assessment.md。

网易云协议参考（2026-11-01，本轮读源码未调用服务）：https://github.com/NeteaseCloudMusicApiEnhanced/api-enhanced/tree/main/module 下login_qr_key.js、login_qr_check.js、song_url_v1.js、vip_info.js。当前URL接口xeapi，不照搬旧weapi假设；详细路径见docs/NETEASE_ACCOUNT_PLAN.md。

第十六轮：以机内HTTP/原厂播放器状态和用户扫码/听音反馈为实证。eapi算法参考api-enhanced/util/crypto.js，二维码依赖Kazuhiko Arase qrcode-generator（MIT，web/QRCODE-LICENSE.txt），https://github.com/kazuhikoarase/qrcode-generator 。当前实测eapi账号与音源接口可用，不能据上游xeapi默认方式断言本机eapi不可用。


2026-11-03 原厂功能协议逆向以 LX06/1.94.13 的运行时 `ubus list`/`ubus -v list` 签名和本地提取的 system0 ELF 为证据；完整签名快照见 [运行时 ubus 签名报告](../reports/factory-runtime-ubus-signatures.md)。`mibt_mesh_rtl` 从 `/data/miio_mpkg/bin/` 运行且与只读 ROM 二进制哈希不同，因此 Mesh 静态结论基于只读下载并核对哈希的运行中 ELF；只读取了 `/proc/3518/exe` 二进制，没有读取其配置、数据库或密钥。协议结构、推断和未确认边界见[协议逆向报告](../reports/factory-protocol-reverse-engineering.md)。其他匹配的执行文件、字符串和导入项只支持局部调用路径，不能替代消息抓包或受控端到端验证。本轮未调用设置、扫描、绑定、红外、语音打印、通话或任意 MIoT/RPC 方法。

2026-11-03 Mesh 本地代理补充：对运行中 `mibt_mesh_rtl` 与匹配ROM的 `miio_helper` ARM ELF做静态反汇编，确认`/tmp/mibt_mesh_proxy.sock`服务端以`AF_UNIX/SOCK_SEQPACKET`创建并监听（backlog 4），`miio_helper`静态连接该路径；收到的最多8192字节数据被封装为内部事件。消息体含义和响应协议未恢复，也未进行成功往返实测。见[协议逆向报告](../reports/factory-protocol-reverse-engineering.md#local-mesh-proxy-transport)。未连接该socket、未发送消息。

2026-11-03 全ROM功能覆盖基线：本地提取的LX06 rootfs中枚举到55个`etc/init.d`常规文件、`bin/sbin/usr/bin/usr/sbin`顶层248个常规文件（41/19/126/62）；按内容头分类为169 ELF、76 shebang脚本（29/7/25/15）、3其他文件。检查发现本地副本权限均非可执行，包括已知ELF，因此不能用副本mode bit推断ROM原始执行属性。55个init脚本已有临时角色表；76个其他顶层shell script调用方与功能仍需逐项定位。实机只读ubus清单为26对象/260方法签名。上述只代表入口清点基线，不是功能数量或覆盖完成证明。覆盖范围、分类状态和目标门禁见[ROM产品功能覆盖台账](../reports/factory-rom-feature-coverage.md)。

同日继续枚举`/lib`与`/usr/lib`得258个ELF对象（`/lib` 120、`/usr/lib` 138）：203个`ET_DYN`（66/137）、54个`ET_REL`内核模块（均在`/lib/modules/4.9.61/`），以及1个`ET_EXEC` D-Bus helper（`/usr/lib`）。不能将258个对象统称为共享库。导出符号初筛发现IDM发现/连接、Mesh、MiLink/VoIP、ASR/NLP/IR、唤醒原因、云端心跳/MQTT/DNS和媒体播放API线索。后续caller交叉核对确认mpas引用IDM、voip_service经dlopen/dlsym引用libvoip→libmilink、语音和MIoT/DNS/MQTT/heartbeat daemon引用相应API；但libmesh.so与libvad.so尚无已发现caller，mibt_mesh自身含Mesh实现相关符号。导出/API引用只证明代码接口可达线索，不证明成功运行或端到端行为；逐库角色及provider/consumer关系尚未穷尽。本地rootfs当前无symlink entry，推测抽取过程未保留链接类型，需核原镜像元数据。`etc/config`下仅记录文件名bluetooth/dhcp/dropbear/network，没有读取配置内容。所有这些均为路径/ELF头/动态符号分类，不构成功能协议解析。

2026-11-03 ROM产品功能首轮域审查使用本地提取rootfs脚本和ELF（`temp/rootfs-factory-full/`）；触摸板/LED、无线配网、Bluetooth Classic/LE、DLNA/MiPlay、Line-in、闹钟/通知/儿童模式/PNS的静态证据及未知边界集中记录于[ROM产品功能覆盖台账](../reports/factory-rom-feature-coverage.md)。这些是静态分析，未读设备配置/凭据/日志或用户数据库，也未进行功能操作；个别行为实测仅引用之前已记录的窄范围测试。

2026-11-03 touchpad/ledserver 补充：ARM静态反汇编识别 Linux input key code→动作映射、press/release、音量步长、AUX与4秒alarm通知节流；`ledserver` 识别`led.show`/`shut`/`shut_all` handler字段、`<index> 0xRRGGBB`写入、L=8全灯色及部分L效果分支；nightmode五字段解析、HH:MM时分边界和sleep_mode未见直接写盘路径。此前将五字段配置描述为已持久化过度推断；后续重定位感知调用分析将纠正记录为仅确认init默认配置，setter写盘/commit未证。完整偏移和未知项见[ROM产品功能覆盖台账](../reports/factory-rom-feature-coverage.md)。未连接设备或调用setter。

同日继续追踪 LED 事件路径：`led.show` producer写入16字节节点的L/pos，链表逻辑按L移除同类待处理项，consumer回调路径将事件交给`light_show_effect`；RGB数据是否沿事件保留、线程注册与同步细节仍未闭合。具体ARM函数偏移见覆盖台账。纯静态本地分析。

同日 Mesh 本地IPC envelope补充：`miio_helper` 对 `/tmp/mibt_mesh_proxy.sock` 使用`AF_UNIX/SOCK_SEQPACKET`，以增量JSON parser读取，dispatcher提取整数`id`并匹配等待中的响应，未匹配消息则提取`method`与`params`；发送回调直接send buffer。服务端仍只确认读包、打4/1 tag和调用内部handler，未读到有效收发样本；具体返回体与每个method参数仍未知。依据两个本地ELF静态反汇编，详见[协议逆向报告](../reports/factory-protocol-reverse-engineering.md#local-mesh-proxy-transport)。

同日顶层厂商ELF初筛：`miio_agent`/`agent_client`/`agent_sendline`显示`/tmp/miio_agent.socket`本地RPC；`idmruntime`符号含CoAP/mDNS/IDM互联；`mitv_pstream`显示电视唤醒与PCM流；`mpap`显示AAC/ALSA渲染；`matool`/`heartbeatagent`涉及厂商API/心跳；`mfgutil`、`miso`、`EnterFactory`、`mtd_crash_log`属于制造/镜像/工厂/崩溃高风险工具，仅静态分类，没有执行或读取敏感文件。证据和未解项纳入[ROM覆盖台账](../reports/factory-rom-feature-coverage.md)。

同日补查多房间IPC：`linein` ELF对`/tmp/multiroom.fifo`的路径引用可追到非阻塞`O_WRONLY`打开及写循环，长度由read结果乘4计算，支持“PCM类缓冲producer”判断；具体格式、FIFO消费者和时钟语义未确认。`mpas`/`mpap`含`/tmp/.uv.ipc.sock`引用，但目前仅证明路径常量被使用，未证明bind/connect或socket角色。只读本地ELF静态反汇编，未执行播放器或连接IPC。

同日顶层ELF caller抽查把`dlna`、MiIO client/helper/mpkg、`alarmd`、`nano_httpd`、`central_lite`/`mdnsd`、`idmruntime`、`qplayer`、statpoints、USB monitor及固件维护工具分别连到init或shell入口；有些只由字符串/导入支持，不能据此写出完整网络协议。`micocfg_*`/`matool_*`可能是缺失symlink对应别名，尚不能还原argv0/调用关系。完整169 ELF caller-map未完成，候选unknown不等于死代码。详见coverage台账。

同日红外静态逆向：本机`mico_ir_agent` ARM ELF handlers与运行时signature显示play/play_batch/learn/read/codec字段；batch使用`|`、最多64片段、interval缺省100ms；learn key绑定、约60秒名义超时、Netlink接收头及codec的Base64/heatshrink流程已跟到。RX/TX sysfs、完整handler偏移和未知pulse/code语义见[ROM覆盖台账](../reports/factory-rom-feature-coverage.md#infrared-control-and-learning-static)。未执行红外操作或读取学习码。

同日共享库关系抽查使用本地ELF的`DT_NEEDED`，只证明动态装载依赖：`mpas→libidmsdk`、`mpap→FDK-AAC/ALSA/protobuf`、mediaplayer/miplayer→FFmpeg与小米播放器库、mico_aivs_lab→AIVS/IR codec、miio_helper→ubus/JSON/Wi-Fi库，以及mibrain/telemetry服务依赖。非完整依赖图，也不等价于函数调用或功能行为。详见ROM覆盖台账。

同日补充ROM入口普查：根文件系统有4个D-Bus相关文件（含一个daemon helper ELF）、3个hotplug hooks、6个uci-defaults hooks、4个rc.button脚本；仅列路径及类目，不读UCI配置值。Bluetooth D-Bus policy、网络初始化/事件hooks、failsafe/power/reset/rfkill的LX06条件与板级事件映射尚待分析，详见ROM覆盖台账。

进一步只读路径审计发现`etc/board.d`只有`99-default_network`，`etc/hotplug.d`仅Wi-Fi/iface/net hooks，没有显式LX06按键映射文件；这不能代替设备板级按键路由验证，power/reset/rfkill handler是否用于目标型号仍未知。详见ROM覆盖台账。

同日顶层76个shebang脚本完成首轮引用与功能分类（bin/sbin/usr/bin/usr/sbin为29/7/25/15，76/76）；依赖shell文本调用、ELF中的文件名提示和入口名字，引用证据与可信度有区分。工厂/OTA/清数据/SSH/崩溃/语音上传等高风险脚本仅静态标记、未执行。Voiceprint/VoIP的marker状态与media gate、音频/PNS/player链路见[ROM覆盖台账](../reports/factory-rom-feature-coverage.md#voice-privacy-and-voip-controls-static)，无个人记录或token读取。

同日PNS/environment/telemetry ELF静态逆向：`event_notify(src,event,detail)`验证src 0–5、event≤54，src分支6类；producer写`/tmp/mipns/pnshelper.fifo`帧，`mipns-xiaomi`重建并打开同FIFO。头字段映射和consumer parser未还原。environ/db与oneshot handler共享JSON配置读写路径，但具体schema差异不明。`statpoints`有Curl POST边界，daemon含临时JSON collector/uploader调用链；未读日志/临时文件/token/配置或payload，未上传。详见[ROM覆盖台账](../reports/factory-rom-feature-coverage.md#pns-event-bridge-environment-settings-and-telemetry-static)。

同日固件维护链静态分析：`ota→flash.sh→miso验证→boardupgrade.sh选另一启动槽并写分区`；`miso`加载/使用加解密与验签API，但信任根、封装格式及掉电恢复未解。另分清OpenWrt `sysupgrade/firstboot`、工厂切槽重启与`clear_user_data`删除路径。只读本地脚本/ELF，无程序执行、分区或用户数据访问、重启或固件写入。详见ROM覆盖台账。

同日MiIO provision/local RPC静态分析：`miio_agent`作为procd服务绑定AF_UNIX/SOCK_SEQPACKET `/tmp/miio_agent.socket`、backlog32，parser提取`id`/`method`但参数/回复schema未恢复；bind脚本进入BLE hidden/config_done/token-sync序列；unbind脚本静态包含清`/data/*`并重启，未执行。顶层ELF socket/FIFO字符串扫描发现central-service、multiroom、AIVS、VoIP endpoints，很多peer/framing未证。详见ROM覆盖台账。未读配置/凭据/令牌、未连接本地socket或云服务。


同日内核模块盘点：提取树`lib/modules/4.9.61`有54个`.ko`，`file`均识别为AArch64 ET_REL。按字符串/符号和init/module-list引用，识别IR GPIO RX/TX、auxin、两类I2C LED、蓝牙/Wi-Fi芯片分支、miso、OSS/ALSA、uinput及31个netfilter模块。54个中47个名字能在脚本或modules.d找到引用，7个未见直接引用；反向有50个清单名称在本地模块目录缺文件，不能由此判定设备缺功能，可能是抽取不全、内建模块或多板型分支。host缺modinfo/readelf，本轮不构成完整vermagic/ABI/设备树加载审计。未运行模块或访问设备。详见ROM覆盖台账。


同日vendor API caller交叉验证：`voip_service`的dlopen/dlsym路径对齐`libvoip.so`导出名（voip_init/login/logout/config等），该库DT_NEEDED依赖`libmilink.so`；mpas有IDM未定义符号引用；mico_aivs_lab、mipns-xiaomi与mibrain_service直接依赖并导入各自AIVS/ASR-NLP API；matool引用MIoT身份/token API入口，messagingagent引用DNS/MQTT，heartbeatagent引用SendHeartbeatAPI。未读取token值或云端payload。对全树ELF依赖未找到libmesh.so/libvad.so直接caller，mibt_mesh类daemon自身含Mesh实现符号。符号/导入仅为静态调用证据，不证明成功初始化或网络行为。详见coverage台账。


同日DLNA/MiPlay补查：`usr/share/dlna/*1.xml`中提取SCPD描述共53个DLNA/QPlay action；QPlay descriptor列出`SetNetwork(SSID, Key, AuthAlgo, CipherAlgo)`，只记录接口字段名，未读取任何值，且禁止调用该高风险凭据接口。`dlna` ELF出现AVTransport/ConnectionManager/Queue/QPlay/RenderingControl handlers字符串，但未见SetNetwork handler或Playlist/Favorites service URI，device.xml列出的服务集合与描述文件需进一步核对，不能将SCPD描述当成已注册接口。`mpap`路径字符串和`readAudioFifo`/AAC-ELD-480/import线索支持可能的multiroom读端，缺syscall callsite，保持未证实。详见coverage台账。


同日补充DLNA SCPD细节与hook对照：device.xml描述AVTransport/ConnectionManager/RenderingControl/Queue/Playlist/QPlay/Favorites七服务、共53 action及部分参数、枚举和值域；dlna ELF可见多组action-invoked handler字符串，但有AVTransport/QPlay未命中项，Playlist/Favorites service URN也未见。descriptor ≠运行时注册。QPlay SetNetwork包含SSID/Key/认证与加密算法输入，是凭据/网络更改接口，未调用或读取值；QPlayAuth的md5导入/日志只说明MD5相关线索，拼接公式未知。未执行任何DLNA动作。详见coverage台账。


同日运行时ubus签名/本地ELF词面交叉：首轮owner ELF词面扫描命中229/260；随后通过bluez_mibt_classical明确的mibt注册方法表确认connect/lock/disconnect，修正为233/260、剩余27个owner/registration未闭合。词面或静态表不证明运行时行为；剩余参数/返回/授权/副作用矩阵未完成。未触发任何运行时方法。


同日BLE GATT静态反汇编：`bluez_mibt_ble`含9个连续128-bit UUID常量及CCCD构造路径；handler/import线索覆盖设备信息、绑定、网络/Wi-Fi、加密注册、通知、RSA/AES/JSON MTU分片。`bluetooth`仅在config_done标记缺失时启动BLE子服务；这不证明完成配网后功能被禁用。候选`mible` ubus对象字符串未出现在26对象/260签名运行时快照，不能与`mibt.ble hidden`等同。UUID到特征映射、属性、加密握手和payload未知；`/etc/bt/rsa_pub.key`仅记录路径，未读内容。静态分析未连接设备、读取配置/密钥/日志或发送数据。详见coverage台账。


同日Wi-Fi配网静态隐私审查：`etc/init.d/wireless:1284`注册日志模板将country/uid/ssid/password/identity/bindkey变量作为格式参数；标记为潜在敏感日志暴露路径，并非已证明实际日志输出或泄露。`simple_dhcp.sh:154–172`兼容分支处理SSID/BSSID并写临时文件/调注册hook，未读取任何值。未打开日志、未导出字段实际内容。详见coverage台账。


同日Wi-Fi/MIoT配网与绑定状态机静态梳理：wireless把ssid/password/bssid/identity字段传给miio.connect（1秒超时）；config_mode会清临时标记、调用ubus并启动BLE；注册成功路径可能写config_done、停止BLE/按条件停Classic并重启dnsmasq/voip/AIVS/PNS/IDM/MiPlay。DHCP release/renew/bound传interface/type给miio.dhcp。bind_device有UID/解绑marker门槛，调用mibt.ble hidden、设置config_done并触发token sync。代码路径不证明授权或同步成功；日志模板credential字段风险见前条；没有读取值/日志、调用接口或写设备。详见coverage台账。


同日AIVS/Mibrain语音IPC静态追踪：`libaivs-message-util.so`含common_message/speech_message Protobuf-C descriptor与pack/unpack；rootfs未找到proto源。mipns-xiaomi的speech.usock存在注册、stream prepare/transmit/cancel/end、dialog/ASR/wake/VoIP消息类型及序列化/不完整发送错误路径；mibrain_service的common.usock关联authorization/service/event/track/MIoT-token消息初始化。消息类型名不是字段schema，方向与auth边界仍未知。voiceprint FIFO与VoIP call路径只记录接口/状态名，不访问声音、联系人、token或日志。详见coverage台账。


同日Mesh子设备方法分发表静态闭合：活动`mibt_mesh_rtl`中两组name→handler表均被注册/遍历代码引用，覆盖_local gateway/scan/bind、get/set_properties/action、group/scene与relay/TTL/friend/network配置方法。get/set properties handler字段含params/did/siid/piid/_no_cache/tid/value等，错误词包含gateway未就绪、group限制、node/product rule/PIID缺失等；pdid规则有query/update/cache/commit及TTL/count路径。账户授权来源、字段必填/类型、回复schema和产品支持全量表未知。活动ELF和ROM副本不同，按既有hash上下文区分版本。未连接socket、发送消息或读取Mesh key/节点数据库。详见coverage与协议报告。


同日PNS事件/帧静态深挖：event_notify helper约VMA 0x119a0，source 0–5映射mp/alarm/app/other/mibt-mesh/mdspeech，event≤54；0..15的mask 0x8501允许0/8/10/15省略detail。producer在FIFO帧偏移0/2放两个16位字段，payload从+4，事件4..7复制NUL字符串，部分ID 17/18、50..54有专用模板；50..53对detail前缀做比较后写一字节结果，但语义未知。consumer unlink/mkfifo(path,0777)/O_RDWR并逐字节发布ubus event，未证按producer header重组。mode为请求值，umask/effective mode未实测。全局事件字典与source子事件映射未闭合；无设备/日志/用户数据读取。详见coverage台账。


同日alarmd复核：闹钟和timer分别持久化于`/data/alarm/%s`与`/data/timer/%s`；记录字段/文件键、EVERYDAY/WORKDAY/WEEKEND/WEEKLY/MONTHLY/YEARLY等重复类别及weekday/monthday/yearday解析位置和值域部分已恢复。workday/holiday文件名、过期重复提醒重排、Hourly Chime状态/音量/事件链、store-show状态标签、micmute计时入口也有静态证据；具体语法、时区/节假日优先级、枚举转换、返回和mic恢复边界仍未知。此前仅受控测过单次闹钟开关/删除。未读记录/配置/日志。详见coverage台账。


同日BLE UUID/handler关系补查：GATT构造顺序为1个UUID尾06服务+8个尾10/11/14/15/16/17/12/13特征；16×76-byte callback registry被setup loop引用，0x2902 CCC与notify读写路径明确。四个直接数据流映射：尾11→encrypted_key_get_ind，尾14→rom_version_get_ind，尾16→rom_log_get_ind，尾12→wifi_init_encrypted_ind；尾10/15/17/13 handler映射未知。BlueZ注册参数中的数字3/8/16未能安全解释为权限；init未传security-level选项，默认加密要求未知。只读本地静态ELF，没有访问公钥内容或设备数据。详见coverage台账。


同日miio_agent本地RPC handler静态反汇编：VMA 0x12404读取最多1024字节JSON，抽取_to和method；明确register/unregister/bind分支（前二取string key，bind取integer address），generic消息用id重映射、删_to/疑似写_from后按注册client fd转发。socket有timerfd/session timeout；未发现params/result，不能标JSON-RPC。key授权、id反向响应、错误JSON、客户端caller和无线/绑定接口映射未知。未连socket或读key/token/配置值。详见coverage台账。


同日central_lite/IDM关联静态追踪：central_lite建立`/tmp/central_service_lite.socket` AF_UNIX/SOCK_SEQPACKET|NONBLOCK listener；mibt_mesh有同path SOCK_SEQPACKET connect调用，双方含gateway role method名，client有JSON通知模板，故确认这两个二进制的本地IPC。DNS-SD browse/resolve与`_miot-central._tcp`仅证明代码路径，未观察广告。central_lite init的START被注释；idmruntime START79且Bluetooth工作模式跳过，其CoAP/IDM符号未证与central socket关联。QLink的CoAP success是单独LAN路径。未读mdns配置或网络包。详见协议报告和coverage台账。


同日OTA/miso校验与双槽流程静态复核：miso导入PEM/RSA验证、SHA-1/SHA-256、AES-128-CBC/MD5并有rsa+sha256/sha1、checksum、magic、model/encrypted-image错误路径；头格式/checksum/信任根/算法选择未知，未读取key/cert。flash.sh实际验证门`miso -v`，boardupgrade选alternate kernel/rootfs槽后切boot_part；脚本无写后readback/自动rollback证据，DTB及bootloader/TPL可写shared分区。通用sysupgrade与vendor OTA分开。仅静态读取本地工具/脚本/模块，不执行或写分区。详见coverage台账。


同日Bluetooth Classic控制面静态交叉：bluetooth init在配置完成后持续启动A2DP Sink与bluez_mibt_classical；ELF直接导入ubus、paired-device/discoverable/connectable/recent/volume与MiConnect APIs。连接/断开路径连到mediaplayer media_control、qplayer提示和statpoints；pair whitelist字段/路径存在但未读内容。D-Bus MediaPlayer/Transport/Control/Folder/Item接口名及AVRCP/volume线索可见。ELF中的mibt 24-byte ubus_method静态表直接映射15个method到handler；前10项共用10字段解析策略，其他方法有独立policy。shared schema不证明每个handler使用全部字段；权限、枚举和BLE协调未知。未配对、连接设备或读设备/whitelist/config/log数据。详见coverage台账。


同日播放器/Line-in handler表静态逆向：mediaplayer ELF在file off 0x24340有43项24-byte method记录，映射handler VMA并与运行时签名字段类型交叉；后续校正PLT后确认start_player_ubus_server既在0x18330调用ubus_add_object(ctx,obj=0x5324c)，也在0x183e4单独注册work_mode_change_event；静态对象0x5324c同时关联名称字符串`mediaplayer`、43项method表及count，因此强确认该表归mediaplayer对象（但结构ABI槽位仍未全解）；mdplay/miplay是不同表面。shutdown timer有pause_later/after_finish/cancel_ending路径与uloop换算；上限/status2含义未知。linein.set_soft_vol将0归1、>=10夹到10，内部字段名与公开签名存在`set vol`/`vol`歧义；get回复payload未知。全为本地静态分析。详见coverage台账。


同日notification/child/sound_effect静态补查：mico-helper方法名create/query/delete/stop/clean/play/status/set，字段level/volume/expire/text/id与通知配置enable/autoplay路径，含缺失、重复ID、level disabled、空文本拒绝分支；过期单位、interrupt/autoplay语义未知，未读通知内容/文件。child_mode只确认nested message.isChildMode字段校验与kid_mode路径，未读状态。sound_effect有set_mode/get_mode、SysV shm APIs，并确认写`/data/dts_conf`路径；内容/模式值域/写时机/DSP协议未知。详见coverage台账。

2026-11-03 nightmode/mplayer/mio-helper静态表复核：mediaplayer静态object `0x5324c` 的名称指针在`+0x1c`（`+0x18`为0），method table pointer位于`+0x34/+0x44`、count 43位于`+0x38/+0x48`，与`ubus_add_object` callsite闭合；work_mode_change_event另行注册。nightmode `current`→handler `0x150b8`闭合，`set`五字段策略与解析/运行时结构更新可见，sleep_mode handler `0x1504c`仍是候选，status无静态method-name记录。重定位感知callsite复查纠正此前“nightmode.set写入配置”的结论：仅确认init默认文件；setter路径未见写盘/commit。miio_helper存在BT mode回调及config-mode/low-power候选生命周期，但ubus method注册关系未闭合。全为静态分析，未运行ELF/调用设备接口/读取状态与凭据；详见[ROM覆盖台账](../reports/factory-rom-feature-coverage.md)。

2026-11-03 alarm/ai_crontab注册表静态闭合：`alarmd`在VMA `0x1b95c`用object `0x37348`注册`alarm`（name `+0x1c`、table `+0x34=0x232e0`、count 25）；create/query/delete/close/open/modify映射到六个handler，别名复用handler。`mico_ai_crontab`在`0x11480`用object `0x25144`注册四项表，`new/notify/remove_all/dump`分别映射handler；owner/registration证据提升至241/260，未闭合19项。方法表不代表各自完整reply/auth/effect契约；没有读取闹钟记录或任务数据库、调用设备接口。候选procd/mibrain/voip owner未能闭合，详见[ROM覆盖台账](../reports/factory-rom-feature-coverage.md)。

2026-11-03 IR/linein静态ubus注册表闭合：`mico_ir_agent` VMA `0x11344`将object `0x25130`注册为`ir_agent`，对象table `0x13208`含6×24-byte记录；`play`→`0x12064`，其余play_batch/learn/learn_stop/read/codec同表。`linein` VMA `0x12e58`注册object `0x241a4`和`0x13494`七项表，`play_status`→`0x12bec`且无字段。owner/handler证据升至243/260、未闭合17项。`led.status`仍在runtime快照但ELF静态表未发现注册，owner未知。未调用接口、运行固件或访问设备；字段含义与硬件效果仍未全部恢复。详见[ROM覆盖台账](../reports/factory-rom-feature-coverage.md)。

2026-11-03 miio ubus静态注册表闭合：`miio_helper`在VMA `0x21fd0`以context `[r4]`和静态object `0x3fd78`调用`ubus_add_object`；对象`+0x1c` name ptr→`miio`，`+0x34` table→`0x2c250`，`+0x38` count=14。14条24-byte method记录确认`renew`→`0x20cbc`、`config_mode`→`0x20834`、`enter_bt_mode`→`0x207ec`、`exit_bt_mode`→`0x207a4`，均声明0字段；owner/handler计数247/260，剩余13项。四方法权限、回复和完整副作用仍未知；无线shell调用和本地agent socket为不同层。仅本地静态反汇编，未连设备/socket或读配置/凭据；详见[ROM覆盖台账](../reports/factory-rom-feature-coverage.md)。

2026-11-03 ledserver/mico-helper ubus表静态闭合：ledserver在`0x1158c`/`0x115bc`分别注册`led` object `0x281e0`与`nightmode` object `0x2821c`，各4个method；nightmode set/sleep_mode/status/current handlers=`0x151b4/0x15684/0x15378/0x150b8`，led show/shut/status/shut_all=`0x1578c/0x155e4/0x1504c/0x15170`。纠正旧候选：`0x1504c`是led.status，sleep_mode实际handler `0x15684`。mico-helper在`0x13dbc`注册`notify` object `0x25230`的8项方法，在`0x13dec`注册`path_child_mode` object `0x2526c`的两项方法；notify play/set→`0x132c8/0x13764`，child set→`0x12dd0`。notify.set方法记录的policy/count与handler内部4字段decode及runtime签名不一致，方法归属已闭合但参数契约保持待查。计数升至253/260、未闭合7项。setter persistence依然未证；全程只读本地ELF，未运行/访问设备、未读用户数据。详见[ROM覆盖台账](../reports/factory-rom-feature-coverage.md)。

2026-11-03 notify/child与ledserver表交叉复核结果：ledserver的两张4项表闭合led.status和nightmode全部四项；0x1504c是led.status，sleep_mode实际0x15684。mico-helper注册notify 8项和path_child_mode 2项，确认notify.play/set与child set handler，owner/handler总计253/260、7项未闭合。notify.set runtime签名是四字段，但其24-byte表项的policy/count字段与handler内解析策略交叉不一致，保留契约差异待查。`nightmode.sleep_mode`的两个`cfg_set_string`写配置调用已由ARM反汇编确认；独立的`nightmode.set`写盘/commit仍无证据。仅静态分析，没有设备访问或用户状态读取；详见[ROM覆盖台账](../reports/factory-rom-feature-coverage.md)。

2026-11-03 mibrain/voip ubus注册闭合：`mibrain_service`在VMA `0x12d24`注册object `0x332d0`（`mibrain`，table `0x1d200`，count 13），其中vendor_switch→`0x14824`、ai_service→`0x14e40`；`voip_service`在`0x15050`注册object `0x2f314`（`voip`，table `0x1b864`，count 7），milink_enable→`0x13f7c`、milink_disable→`0x14068`。owner/handler证据升至256/260，未闭合4项（service.set/add、system.info/watchdog）。method记录中field count不恢复完整字段使用、权限和副作用。只读本地ELF，无设备或个人数据访问。详见[ROM覆盖台账](../reports/factory-rom-feature-coverage.md)。

2026-11-03 TTS/Line-in/mibrain handler第二轮静态闭包：`mico_aivs_lab`在stream prepare调用`mkfifo(path,0600)`，FIFO open为`O_WRONLY|O_NONBLOCK`，最多300次、每次10ms等待；`TTSStreamingOutflow`在VMA `0x31cec`调用write。finish handler VMA `0x31e5c`有guarded `creat("/tmp/nano_httpd/httpd/fifo_finished",0600)`分支，且带匹配的创建失败日志，因此nano_httpd等待的marker生产端已闭合。音频FIFO ID构造、framing、marker时序/关联和URL映射仍未知。`libmibrain-common-sdk.so`同样包含TTS文件/FIFO字符串与I/O imports，callsite尚未闭合。`mibrain_service`的nlp_result_get读取缓存分片；vendor_who/aivs_authorization_get走内部授权处理；token update缺字段时报错、更新失败分支请求重启mico_aivs_lab；aivs_event_post解析namespace/name/payload；text_to_speech解析七字段并调用内部处理器，成功响应/ACL/后续影响未恢复。`linein`表start/stop→`0x12884/0x12904`；ALSA静态参数含48kHz双声道capture、period 1920/buffer 7680，输出分支48kHz或44.1kHz；`auxin_stereo_function`以非阻塞写端打开`/tmp/multiroom.fifo`。mediaplayer存在linein状态查询及start/stop命令，异常参数的restart helper只证明意图。只读本地ARM ELF反汇编/strings，未运行ROM、ubus、FIFO、音频或设备接口，无凭据和用户数据读取。详见[ROM覆盖台账](../reports/factory-rom-feature-coverage.md)。

2026-11-03 nano_httpd TTS consumer路径补查：ARM handler在VMA `0x1128c`调用`realpath(request_path,NULL)`，`strncmp(canonical,"/tmp/tts/tts_",13)`必须相等才继续，随后以`open(canonical,0)`只读打开精确路径；不另行生成FIFO ID。服务先发送HTTP/1.0 200/audio-mpeg3头，再以最多32KiB块从FIFO read并send，`select`等待可读；空闲分支检查`fifo_finished`是否存在，找到后unlink标记并结束，等待计数每轮+500ms至3000ms。与mico_aivs_lab的mkfifo/write/finish-creat路径构成静态producer-consumer握手；具体FIFO名/请求URL选择、bytes之上是否有 framing、标记时序/并发请求关联、错误响应语义仍未知。路径规范化后要求命名空间前缀，可拒绝解析到该命名空间外的路径；未做运行时HTTP请求或路径测试。证据为本地`nano_httpd`及`mico_aivs_lab` ARM反汇编/字符串；无服务启动、网络/FIFO访问或设备调用。

2026-11-03 mico_aivs_lab TTS FIFO命名与URL生成静态恢复：handle构造函数VMA `0x3288c`调用`gettimeofday(&tv,NULL)`，用C++ stream按顺序拼接`/tmp/tts/tts_` + arg1 string + `_` + `tv_sec` + `_` + `tv_usec` + `.` + arg2 string，并将路径存到handle `+0x60`。caller VMA `0x501dc`的arg1来自请求对象虚函数表偏移`+0x4c`的string结果（空值时用空串），arg2来自偏移`+0x34`的string结果；语义字段名仍无法从剥离符号的二进制确认。`SpeakHandle`及`dialog_id`/`tts`附近日志文本不足以将两个getter与字段逐一对应。stream prepare将`http://localhost:8888/`与FIFO路径拼成URL，存入`+0x78`，随后`mkfifo(path,0600)`并非阻塞写端open；handle setup还在`0x329a0` unlink旧`fifo_finished`。至此前述URL消费者调用方路径在mico_aivs_lab内闭合；是否还有外部ROM caller、HTTP client如何规范化组合URL中的双斜线、payload是否有更高层framing、并发stream与共享marker如何关联仍未知。依据factory ELF反汇编与literal strings，未运行程序、连接localhost、发送HTTP或访问FIFO/设备。

2026-11-03 procd候选注册链进一步核验：`/sbin/procd`唯一直接`ubus_add_object` callsite VMA `0x14b24`由`0x13e98`进入，静态object `0x2d4d8`的name `system`与count=6可见，但自定义table layout和方法名→handler记录未恢复。`service.set/add`无直接method记录或单独service对象注册callsite；watchdog设备/timeout/frequency字符串不等于ubus方法注册。`system.info/watchdog`和`service.set/add`继续作为四个未闭合方法。纯本地静态ELF检查；详见[ROM覆盖台账](../reports/factory-rom-feature-coverage.md)。

2026-11-03 procd system表恢复：object `0x2d4d8`由`0x14b24`注册，name=`system`、method table VMA `0x1af44`、count 6；24-byte记录恢复`board/info/reboot/watchdog/signal/sysupgrade`→`0x1435c/0x148d4/0x13f9c/0x1419c/0x14068/0x14100`，system.info/watchdog闭合，watchdog声明4字段。owner/handler证据258/260，仅`service.set/add`未闭合。二进制为ROM中LEDE ARM procd；仅本地静态反汇编，结果可复核于[ROM覆盖台账](../reports/factory-rom-feature-coverage.md)。

2026-11-03 procd/system对象表闭合：已从本地ARM ELF解出`system`六项表并匹配runtime签名，`board/info/reboot/watchdog/signal/sysupgrade` handlers=`0x1435c/0x148d4/0x13f9c/0x1419c/0x14068/0x14100`，watchdog field count=4。system.info与system.watchdog owner/handler已闭合。procd的service.set/add仍需查是否由其他注册路径提供；未执行procd或调用接口。详见[ROM覆盖台账](../reports/factory-rom-feature-coverage.md)。

2026-11-03 public-source cross-check (reference only): the extracted image identifies itself as LEDE Reboot SNAPSHOT `70-1-1`; I consulted the official [OpenWrt procd system.c](https://github.com/openwrt/procd/blob/main/system.c) and [LEDE ubus reference](https://git.openwrt.org/web/tree/docs/ubus.txt) for terminology around `system.info`/`system.watchdog`. The current upstream source is not established as the ROM's exact revision, so it is not used to infer this ELF's registration, field use, authorization, side effects or return payload. ROM conclusions above come from the local binary/runtime snapshot.

2026-11-03 notify.set discrepancy复核：PLT/JUMP_SLOT交叉确认`0x1147c`为blobmsg_parse。notify method record VMA `0x147a4`声明policy `0x14674`、count1（仅`level`）；handler `0x13764`明确用`0x14684`四项policy解析`level/enable/autoplay/volume`，与runtime signature四字段一致。相邻表行布局正确，不能用stride误读解释；现有证据无法区分ELF内部声明/decoder不一致与ROM/快照版本差异。方法owner闭合，但有效参数契约不确定。无设备访问/方法调用。详见[ROM覆盖台账](../reports/factory-rom-feature-coverage.md)。

2026-11-03 service.set/add全局候选复查：对rootfs `/bin,/sbin,/usr/bin,/usr/sbin,/lib,/usr/lib`常规文件检索dotted method名、`service_set/add`和注册API与service标记组合。无ELF含`service.set/add`字面串；`service_set/add`仅在Bluetooth/tracing/WLAN/media等不相关库命中。只有procd同时是候选service对象provider，但已确认其唯一add_object调用仅注册system；shell procd helper和reload_config的service调用是client侧调用，不证明server实现。未建立间接注册或dlopen provider；两项仍为运行时快照事实、ROM提取注册链缺口。详见[ROM覆盖台账](../reports/factory-rom-feature-coverage.md)。

2026-11-03 DLNA/multiroom/telemetry深查：`/usr/bin/dlna`直接service lookup 5个URN并给36个SCPD action挂`action-invoked`回调（AVTransport12、ConnectionManager3、QPlay7、Queue3、RenderingControl11）；其余17 action只有descriptor证据，36项handler的逐项字段policy/reply/error尚未恢复。`mpas`确认mkfifo(path,0666) helper和AF_UNIX pathname connect template，`mpap`确认4096字节read loop；这些实参尚未追到multiroom FIFO/socket路径，故PCM/AAC frame和socket schema未知。遥测链静态闭合`upload_stat_points→matool_collect_stat_points→statpoints→cleanup`及Curl POST/签名加密/10、20、30秒失败退避；不证明请求成功。statpoints_daemon `#START=72`，启动/触发未证；heartbeatagent触发与payload未知。未调用设备/网络或读取日志、配置/遥测/凭据；详见[ROM覆盖台账](../reports/factory-rom-feature-coverage.md)。

2026-11-03 environ/db detect与心跳边界复核：`environ_detect`/`db_detect`共用配置文件读/JSON解析及写路径，但独立schema未解；heartbeatagent的SendHeartbeatAPI符号/导入证明存在请求实现，不证明有init caller、触发周期或请求内容。均未读取当前配置、日志、遥测、序列号/token，也未发请求；见[ROM覆盖台账](../reports/factory-rom-feature-coverage.md)。

### Public source cross-check: procd service object

2026-11-03 official upstream branch comparison: the [OpenWrt procd `lede-17.01` service/service.c](https://raw.githubusercontent.com/openwrt/procd/lede-17.01/service/service.c) declares `main_object_methods` with `set` and `add` bound to `service_handle_set`, and `ubus_init_service()` registers `main_object`. This is public source code for a branch relevant to the image's LEDE lineage, but the extracted `70-1-1` image's exact procd source revision is not established. The local ELF lacks a corresponding `service` registration chain, so treat the public code only as a build/version discrepancy clue, not as evidence that this ROM implements those methods.

2026-11-03 ROM hook/module second pass: local evidence in `temp/rootfs-factory-full/init`, `/etc/init.d/{boot,bluetoothd,wireless,adbd,gpio_switch,dnsmasq,boot-test-start,boot-test-end}`, `/etc/hotplug.json`, `/etc/hotplug-preinit.json`, `hotplug.d`, `rc.button`, `uci-defaults`, `usr/share/dbus-1/system.conf`, and `etc/dbus-1/system.d/bluetooth.conf`. `/init` directly selects LED modules and board-gates GPIO IR modules; `boot` invokes `kmodloader` and `uci_apply_defaults`; board-conditioned Bluetooth/Wi-Fi loads are present. D-Bus system policy statically denies method calls by default with BlueZ exceptions. UCI defaults can execute at boot and package post-install; target consumption state is unknown. Additional paths include conditional data-volume format on mount failure, non-release ADB/RNDIS, config-driven GPIO, Wi-Fi-event DHCP and conditional test-script hooks. The target board/channel/config and effective runtime policy were not read. No hook, module, device interface, or state-changing path was executed. See [ROM feature coverage](../reports/factory-rom-feature-coverage.md#additional-hook-and-bus-policy-census) and the kernel-module section.

2026-11-03 DLNA `SetAVTransportURI` handler静态追踪：`/usr/bin/dlna` VMA `0x12ae4`通过`gupnp_service_action_get`读取`InstanceID:int`、`CurrentURI:string`和`CurrentURIMetaData:string`，与SCPD一致；URI复制上限8192字节，检查`qplay://`前缀并走不同内部状态/URI分支，最终调用`gupnp_service_action_return`且未设置输出参数。非qplay分支进入`0x12690`，其具体播放效果、校验/错误语义及SOAP成功含义仍未知。`dlnainit`仅在`dlna_status=1`配置条件下请求启动daemon；未读当前配置或设备身份值。证据为本地XML/init、ELF重定位和ARM反汇编；未运行daemon、发送SOAP或访问设备/网络。

2026-11-03 `libmibrain-common-sdk.so` TTS独立producer静态追踪：ARM DSO的`0x8bdc–0x92c8` TTS result handler在`0x909c–0x9100`构造`/tmp/tts/tts_`下FIFO并以`mkfifo(...,0777)`、`open(...,O_WRONLY|O_NONBLOCK)`创建写端；路径格式由`%s%d_%d%s`组成。`0x8cfc–0x8d14`拼接`http://localhost:8888/` URL，`write`送数据，完成分支unlink FIFO并close；重试间隔10ms、最多300次，耗尽时以`system`请求`/etc/init.d/nano_httpd restart &`。它与mico_aivs_lab使用同一服务命名空间但格式不同，未证明共享handle/caller。未运行代码/网络/FIFO/设备或读取运行时数据；见[ROM覆盖台账](../reports/factory-rom-feature-coverage.md#nano-http-tts-endpoint-static-arm-analysis)。

2026-11-03 mpas/mpap多房间路径参数静态复核：`mpas`在VMA `0xd7914`附近将调用方路径传给`mkfifo(path,0666)`；其AF_UNIX connect helper在`0x151c4c–0x151d08`把运行时string对象字段`+0x6c`复制到`sockaddr_un.sun_path`后connect。`mpap`在`0x3a414`以调用方传入路径open（flags `O_NONBLOCK`），在`0x3a630`循环read、单次上限4096字节。`/tmp/multiroom.fifo`及`.uv.ipc.sock`字符串均未通过静态指针引用与这些运行时参数闭合；linein→FIFO→mpap仍是推断，音频framing和Unix socket消息契约未知。仅本地ARM ELF/反汇编，无程序、FIFO、socket、设备或网络访问。

2026-11-03 BLE GATT注册参数与安全选项补查：`bluez_mibt_ble` VMA `0x22690`注册四个已映射特征（UUID尾字节`11/14/16/12`）时，各自添加CCC UUID `0x2902`并向helper `0x1a070`传原始值`3`和`16`；由于helper ABI/常量含义未恢复，不将其标记为ATT权限。CLI安全级别解析位于`0x13880`、`0x13948–0x1399c`，接受`low/medium/high`；后续数据段复核确认初始值为1（low），已知init启动未传覆盖参数。每特征安全门、payload/reply wire schema和结果码仍未知。仅本地factory ELF/字符串/反汇编，无连接、特征读写、配置/日志/凭据或设备状态读取。

2026-11-03 BLE security default correction: re-tracing the `-s/--security-level` parser maps `low`, `medium`, `high` to integers 1, 2, 3 and confirms `.data` initializes the stored level to 1 (`low`). The known init invocation passes no override, so that launch uses the binary's low default; this does not establish per-characteristic access/encryption requirements or exclude other launch paths. Static local ARM ELF only; no BLE connection or device/config/auth-data access.

2026-11-03 touchpad隐藏SSH序列静态恢复：`bin/touchpad`只把EV_KEY且value=1的按下事件送入DFA；`.data`表长度8，键码为`102,102,114,114,115,115,139,139`（本程序映射为mute×2、play×2、volume_down×2、volume_up×2），完整匹配调用状态回调，初始化目标解析为`sys_ssh_enable`。`difftime`与double常量60.0比较，超60秒或错误输入重置序列；硬件实际事件、去抖、各板型支持情况未验证。并行完成`miio_agent`静态路由：读请求最多1024字节，`_to`整数作为32槽位位掩码路由；保留原sender fd/id、替换转发id并注入`_from`槽号，反向匹配后恢复原id并回送原fd；未匹配反向消息广播是基于分支结构的推断。pending关联记录配置5000ms timer并清理，未见客户端超时响应。仅本地ARM ELF静态分析，未运行ROM、打开socket、访问设备或认证数据；详见coverage台账。
2026-11-03 alarmd create/modify/open/close/delete调用链静态补全：create blobmsg policy 11字段并走`0x14a94`记录构造器；modify解析type/id及更新字段，部分省略整数转为`-1`后交`0x14e40`；open/close切换内存记录字段并刷新调度，open有过期timestamp推进路径；delete移除内存节点和`/data/alarm/%s`文件。repeat parser可见weekday 0..6、monthday 1..31、month/day校验，但selector/优先级、持久化键值、状态字段语义和精确回复仍未知。仅本地工厂ARM ELF，无alarm ubus调用、设备数据读取；见ROM coverage台账。
2026-11-03 wireless/LED状态机静态补充：wireless `scan_prepare`在AP-only可停AP、启动STA并等待wpa_check；`scan_restore`只在AP模式且micocfg_ap_run为真时恢复。临时AP hostapd配置`wpa=0`受config_done marker gate控制。`wifi()`忽略miio.connect返回，最多等待20秒dhcp_done标记后回success/authfail，故不是直接认证结果。config_mode另有model/config_done/Bluetooth-work-mode gates。注册日志format调用含敏感身份/凭据变量，是源码级暴露候选，不代表运行时已输出；未读值或日志。`led`启动仅在nightmode.cfg缺失时初始化/迁移配置；`led.shut_all`停止当前effect并写0，status/current组装回复；nightmode.set两个helper的持久化语义仍未闭合。详见coverage。仅本地脚本和ARM ELF静态检查。
2026-11-03 nightmode.set写路径更正：复核`ledserver` helper `0x15d98`的重定位和literal表，五个调用落到`cfg_set_string`，key依次`total/light/volume/start/stop`，配置路径`/data/etc/nightmode.cfg`。因此五字段set调用链已静态闭合；未见cfg_commit，原子性/持久化时序未知。另`led.status`格式化回复、`led.shut_all`停止当前效果并向底层helper写0、`nightmode.status/current`组包回复有静态路径证据。仅本地factory ELF，无ubus调用或设备写入。

2026-11-03 touchpad SSH启用调用链及Dropbear门静态补全：8键DFA完整匹配调用`sys_ssh_enable`，脚本设置`/tmp/ssh_en=1`并请求dropbear start；Dropbear在Bluetooth work mode拒绝启动，release channel需三个marker至少一个为1。init校验schema默认PasswordAuth=0、RootPasswordAuth=0、RootLogin=1、port22、MaxAuthTries=3；command builder据配置传`-s/-g/-w`。这些是schema defaults不是设备实际配置，key/account auth与监听Interface未读取/未确认。没有读取host key或authorized_keys，也未启动服务。
2026-11-03 PNS调用方应答处理静态补充：`etc/init.d/pns`的mute/unmute仅在`pnshelper.event_notify(src=3,event=7/8)`返回JSON `code=0`时同步播放器`mic_off/on`并报告成功；alarm mic_open(`src=1,event=0`)检查整数code=0后才亮灯/成功，mic_close延迟6秒关灯并发送event=1但不检查reply。证明的是caller侧响应解释和顺序，不代表麦克风状态已变化。只读本地脚本，未调用ubus或读设备状态；见coverage。
2026-11-03 VoIP lifecycle静态追踪：`etc/init.d/voip`仅当`micocfg_country`为CN（空值默认CN）才procd启动`voip_service`；stop路径终止service/applite、请求`ring_tts_stop`并关LED 13/16。`clear_data`递归删除`/data/voip`后sync/reboot，是未执行的高风险动作。方法语义和账号/会话合同未恢复。只读init脚本，无服务调用、用户数据或token读取。

2026-11-03 静态维护复核依据：本地提取的 `bin/{ota,flash.sh,boardupgrade.sh}`、`etc/init.d/alarm`、`usr/bin/alarm_translate.sh`。确认 no-reboot 在写分区/切槽之后判断，BL2第六副本 erase `0x140000` 与 write `0x14000` 不一致，以及旧闹钟迁移先清目录、末尾删旧库且缺少结果检查。仅脚本字面和控制流证据；实际 NAND/启动/迁移结果未知，未执行或读数据库。详见 ROM coverage 的 Additional OTA and alarm-migration review。

2026-11-03 并行本地ARM静态证据：`usr/bin/mediaplayer` handler `0x16d20`→`0x25bbc`→`0x1f238`恢复index缺省、列表边界及内部播放/事件路径；`usr/bin/mico-helper` handler `0x12dd0`及open/write调用恢复isChildMode布尔到on/off配置写入。回复/授权/运行效果未证明，未读队列或儿童模式配置，也未调用接口。详见ROM coverage。

2026-11-03 Luna并行静态分析及主代理复核：本地 `mico-helper` create 0x138d4→constructor 0x120d4确认五字段ABI、volume无符号100边界与两分支，expire单位未证；`linein` 0x12328证明每帧4字节write，`mpap` 0x36be8/0x397d0确认FIFO常量用于字符串构造但尚未闭合consumer。使用系统objdump及本地ELF字节；无ROM执行、设备/网络/配置/个人数据访问。

2026-11-03 DLNA Seek静态补查：本地 `usr/bin/dlna` handler 0x12d90，gupnp getter call 0x12df0、sscanf 0x12edc和毫秒运算、position blob及player_set_positon ubus调用已追踪。主代理复核API和运算；TRACK_NR独立路径/完整错误语义未恢复，没有运行请求。详见coverage。

2026-11-03 Luna/root静态交叉核对：mico-helper expiry loop 0x12664使用time(NULL)对node+0x10有符号比较；DLNA SetVolume 0x15b44与system helper 0x15a34；alarmd micmute 0x1afe0三字段及now+h*3600+m*60+s计算、remove type3扫描0x13e04。仅本地ELF、重定位及SCPD，未执行或读取用户状态。见coverage。

2026-11-03 通知清理与Line-in补查：本地mico-helper 0x11f88→0x11cb4的unlink/释放及0x116a4的>30秒扫描门；linein 0x11ed0样本转换、0x11c58 ALSA配置和两playback分支。公开ALSA PCM官方参考仅用于解释format=2的S16_LE枚举（https://www.alsa-project.org/alsa-doc/alsa-lib/group___p_c_m.html），不能证明ROM确切库版本/硬件协商结果。纯静态，无设备/音频/用户数据读取。

2026-11-03 本地mico-helper 0x132c8→0x11fc0→0x13ed4的队列播放/pipe_exe调用与0x122c4队头插链经复核；0x11d0c导入为sync。无播放结果或用户内容读取，见coverage通知播放节。

2026-11-03 静态证据追加：libmico-common.so pipe_exe10e34→vsnprintf/popen10ee4与mico-helper13f58命令串；mphelper volume_set/get脚本及mediaplayer250b4/247fc/23a4c/232c0/235a4 mixer链。未读通知文本或运行日志，无payload/请求执行。见coverage。

2026-11-03 ELF metadata census: standard-library parser tools/rom-elf-inventory.py over local extraction; generated factory-rom-elf-inventory.md has427 files,1842 DT_NEEDED entries,0 parse errors. SONAME/basename matching only; mpap→libfdk-aac.so.1 unresolved. Not runtime loader/caller proof. Independent ARM review confirms default volume handler chain and no text escaping on traced notification pipe_exe path, no ROM execution.

2026-11-03 本地bluez_mibt_classical TV wake静态证据：handler2016c，proc_is_running helper22598，Mesh snprintf/system202ac/202b4，BLE与stop控制流。主代理复核Mesh分支/进程选择；实际电视/广告/服务操作未执行，MAC验证和ACL未证。见coverage。

2026-11-03 alarmd type3 descriptor373e0、expiry15f98→1d8c4→1cc24→system mic_on及pns脚本376+静态恢复请求链经root复核；非空闲VoIP提前返回/实际恢复未证。ELF parser独立objdump核对3类实际存在的样本；四PIE和mpap AAC主版本缺口另复核，无认证数据库读取。见coverage。

2026-11-03 alarmd recurrence evidence: circle enum372d4, selector12644, localtime/workday predicate16f80, daily normalization169f0, ONCE126dc. Luna self-review及root核验偏移/条件/固定秒数，未读假期配置实际日期或执行alarm。见coverage。

2026-11-03 alarm recurrence follow-up: local ARM alarmd disassembly confirms `0x169f0` uses fixed 86400-second steps to normalize candidate timestamps; weekly selection at `0x12864` searches the next seven weekday candidates, while WORKDAY/HOLIDAY loops at `0x127d0`/`0x12838` continue until the `0x16f80` predicate matches. Callers of negative/no-match and old ONCE results, persistence timing and DST behavior remain unclosed. No alarm code, ubus method, device, workday file contents or user state was accessed. See the [coverage report](../reports/factory-rom-feature-coverage.md#alarm-recurrence-selector-and-date-overrides-static-arm).

2026-11-03 alarm expiry caller follow-up: static trace `0x15e94–0x16178` dispatches type end callback then branches on the recurrence field; one-shot enters removal helper `0x13d14`, recurring selects a new candidate and calls scheduler-record builder `0x12e44`. Its zero return goes to timer re-arm; failure cleans the replacement. No direct alarm-file writer call was found in `0x12e44`, correcting the previous broad persistence claim; durable repeat advancement remains unknown. No runtime, device, config or user data access. See coverage.

Independent Luna cross-check of alarm recurrence: selector `0x12644` reads record+0x10 and writes its candidate timestamp to +8; normal and default exits both return zero, so this is not a validity/success result. In the expiry worker, circle zero bypasses the selector and enters `0x13d14`; nonzero records are rebuilt/reordered in the scheduler list. `0x13d14` formats the alarm backing path, calls unlink, removes the list node and frees memory. The worker→`0x12e44` path has no direct file writer call; indirect persistence through separately called helpers is not established. Additional selector call sites `0x1490c`, `0x159f4`, `0x1671c` remain unnamed. Main agent inspected the ARM disassembly; interpretation relies on ABI/field cross-links. No runtime/device/user-data access. See coverage.

2026-11-03 idmruntime UDP endpoint链16e774→1afb24→1ad3dc/socket→1ad4f4/bind与端口递增分支经Luna/root核对，具体URI/port未闭合。mitv_pstream frame builder11c8c和模板26140、9093连接路径、read/fortify分支；root另核factory libc __read_chk deb60超限调用__chk_fail，纠正65536误称clamp。无网络或代码执行，详见coverage。

2026-11-03 IR TX: module SHA61318c31b9ee8f272d79953875cb0fd2b493183ad143372c4f371cc95aa96f28, relocation group→ir_data0644→store.text148, allocation.text190/__kmalloc194 and4byte token writes1dc beforeodd guard1e8 independently reviewed by root/twoLuna. Kernel official sysfs docs only ABI terminology; no module/input execution. mico-helper SHA c7b18b8e15914d74cd52daeeaa0dd6590d856176310c0a015a68ced3833e1683 direct method bytes show notify.set policy14684/count4: prior count1 discrepancy claim withdrawn without attributing to version change. All10method owner/entry chains independently checked; otherfacets open.

### Public references for multi-ecosystem smart-home adapters

- Home Assistant, [Matter integration](https://www.home-assistant.io/integrations/matter/): Matter Server runs as a separate controller process; the official HA Matter Server app does not support 32-bit platforms. Used to qualify the draft's recommendation not to assume Matter Server can run on LX06. It does not establish compatibility with this speaker's kernel, radio hardware, memory budget or network configuration.
- Home Assistant, [MQTT integration](https://www.home-assistant.io/integrations/mqtt): MQTT supports discovery/configuration for common entity types and connects to a user-selected broker. Used as an example of an open protocol adapter that still needs an explicit topic/payload contract.
- Home Assistant, [HomeKit Device](https://www.home-assistant.io/integrations/homekit_controller/) and [HomeKit Bridge](https://www.home-assistant.io/integrations/homekit/): one integration brings HomeKit accessories into HA, the other exports HA entities to Apple Home. Used to distinguish device-side protocol support from exporting entities to another controller.

These public docs inform architecture options only; no integration is implemented or verified on the LX06.

2026-11-03 boot lifecycle source evidence: local extracted etc/inittab, etc/rc.common, etc/rc.local and etc/init.d/{boot,done,cron}; lexical 55-file census includes SHA-256 anchors. Static source only, no runtime/configuration/credential reads; missing rcS/link metadata explicitly retained.

2026-11-03 lifecycle review: local extracted init scripts with hashes in factory-init-static-census.md; additional sources usr/bin/clear_user_data, sbin/firstboot, etc/rc.button/reset, usr/sbin/boot_function.sh. Results in factory-init-lifecycle-contracts.md. All findings are static source observations or explicitly bounded implications; no new public-source or runtime evidence.

2026-11-03 native reset evidence: local factory miio_helper and libmico-common.so, objdump ARM disassembly/symbols/relocations, independent PT_LOAD and PC-relative literal/PLT word decoding, plus usr/bin/reboot.sh source. Hashes and addresses in coverage native MIIO reset section. No execution or device/current-state data access.

Parallel static cross-check: three independent agent reports over local factory alarmd, miio_helper/libmico-common and init/procd/jshn sources; main inspected chime setter/reply/OFF and init wrappers directly. Reports preserve source hashes and VMA/line evidence. No device execution, configuration/log/credential reads or new external references.

Resumed parallel review sources: same hashed factory alarmd and libmico-common for workday timer/loader/file_linecount and chime startup/get; miio_helper main/context initializer/miot_rpc/native method table plus local bin/unbind_device.sh and usr/bin/reboot.sh for distinct restore/reboot chains. Main directly inspected loader/free/marker sequence, allocation-count/store loops, chime parser and native handler disassembly. All are local static evidence; no actual version/workday files, identity values or reset operations accessed.
