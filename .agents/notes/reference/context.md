# Reference: 项目上下文

GitHub / Claude 接续请先读[长期接续入口](claude-wake-analysis.md)，包含当前未解决问题、逐轮证据、构建命令及不入库材料的恢复边界。

## 原厂两级引擎已独立调用（第二十四轮）

见[第二十四轮报告](../reports/原厂两级判醒与独立引擎调用-第二十四轮.md)。只读探针从原厂进程读了 108 B 控制字，两级实际 chunk=5、min/max-self-loop=1/80、acoustic-scale=0.1、min-trans-loglikelihood=-1000、score-threshold=1、wait=60、frames-forward=30。固定链路与映射检查在 src/native_wake_config_probe.c，错误目标拒绝。

src/native_wake_engine_probe.c 在隔离进程预加载设备自带 pthread/stdC++/ALSA 后，直接初始化、销毁原厂两级模型；特意传不存在的模型路径仍加载内置权重，与加载分支相符。10 秒生成静音批量推理 622 ms、峰值 RSS7040 kB、无非零输出；合成“小爱同学”一级命中 score8.114393，二级未确认，随后 flag2 超时。最终探针7596 B，依赖既有原厂库，不是完整独立安装包。上述不是现场召回/音乐误触发验收。

tools/Build-WakeProbes.ps1 和 tools/native-wake-dependencies.py 可重建与核对依赖。所有本轮设备临时程序、fixture、trace 已清除；原厂 PID1282/2527 未变、TracerPid0，正式 Duplex 哈希未变。未采集现场音频、未改模型或生产服务。仍缺“豆包豆包”可用模型及全部确认/拒绝逻辑，音乐自激活尚未修复。

## 当前首要故障：音乐自激活仍未解决（第二十三轮）

原厂成本补测：两级内置模型共 2318298 B（2.21 MiB），已包含在 4282440 B（4.08 MiB）的 libxaudio_engine.so 中；加 mipns-xiaomi 与 libmdspeech.so 三文件共 4.58 MiB（不含其他依赖）。整进程 smaps RSS 13976 kB / PSS 12672 kB；10.02 秒采样约单核 62.5% / 四核总能力 15.6%。这些不是独立 KWS 的最低需求或新增任务成本，详见第二十三轮报告资源补测。

用户明确反馈无人呼叫仍不断自激活，并要求保留“豆包豆包”。此前第二十二轮两次判醒不能证明准确识别或误触发已修复。现有日志有 71 次判醒、21 次 no_speech；至少 8 次候选暂停前后以完全相同距离从 hits=2 变 hits=3，复用了旧音乐窗口；还有绕过候选暂停的直接判醒。开始排查时自有语音服务已被 signal 停止，本轮未重启或替换正式版本，原厂播放器保留。

本轮深入确认原厂 Rice/Paddy 的 MFCC/CMVN、TDNN prior/Viterbi/IsKeyword 和两级缓存处理；尚未完整还原全部拒绝条件，原厂“小爱同学”模型不能直接声称支持“豆包豆包”。新增 --wake-audit 隔离诊断，AEC 输出与硬件参考只在内存中比较；45 秒音乐 217 窗、18 个模板候选，简单参考距离拒绝规则不充分，未上线。PC 上现成神经 KWS 能检出合成语音，却未检出已有真人录音；预编译 ARM 库另有 glibc 版本不兼容，未安装设备。详细证据、构建哈希与边界见[第二十三轮报告](../reports/音乐自激活与原厂判醒复核-第二十三轮.md)。当前仅有诊断进展，没有完成修复。

## 音乐下唤醒笔记补全

已核对并补全 implemented 下的[三项叠加成因与重引入条件](../implemented/bug-fix/2026-11-01-music-wake-stopped-firing.md)和[同音频采集 A/B 方法](../implemented/testing/2026-11-01-capture-ab-comparison.md)，与采集架构及第二十二轮报告互链。本轮仅文档核对，无新设备实测。纠正单麦 12.05 与阈值 12.5048 的大小判断；区分最低距离与连续确认、诊断阈值与线上 15.0 阈值。诊断实际为 11 路，只有 ch0 与六麦混音含 AEC 分支，20 秒样本缓冲约 6.7 MiB 为代码推算，并非实测 RSS。

## 能力边界与打包提案（暂不实施）

用户要求记录[能力边界与二进制打包提案](../proposed/architecture/2026-11-01-capability-boundaries-and-binary-packaging.md)：后续米家等能力保持代码模块与接口独立，不直接混入 Web 或 Agent；不要求独立 bin 或常驻进程。若实际比较证明合编更小且其他约束满足，优先合编。随功能设计推进，尽量不产生额外改造与基础设施。本轮仅文档修改，无代码、构建或设备改动；合编与拆分的体积差异尚未实测。

## 用户目标与偏好
- 设备：小爱音箱 Pro LX06；App 固件版本由用户确认为 1.94.13。
- 用户已有 GPT / DeepSeek / 豆包等 API 条件；未收集或写入任何密钥。
- 希望修改音箱本体，不接受额外常开电脑/NAS 桥接作为最终方案。
- 保留米家家电控制；计划网易云音乐、歌单、语音点歌和自然对话。
- 用户允许继续设备研究，要求所有成果和上下文留在独立项目。
- 驱动已经由用户安装，用户明确要求不要重复安装。

## 已确认的本机事实
- Windows PowerShell；用户目录顶层读取/Projects 建目录遇到访问拒绝，桌面项目目录可写。
- 用户提供工具原路径：C:\Users\gitai\Desktop\Amlogic_Flash_Tool_v6.0.0。
- Amlogic USB VID_1B8E&PID_C003；WorldCup Device；驱动 oem117.inf。
- USB identify 返回 AmlUsbIdentifyHost / This firmware version is 0-7-0-16；该值不是 App 系统版本。
- 单次晚执行 identify 曾错过接口；循环执行后识别成功。
- bulkcmd printenv boot_part bootdelay product_model 和 mtdparts 返回 success，但未返回实际文本；不能声称已经读到分区表或启动分区。
- help mtdparts 返回 failed。
- 尚未执行 setenv/saveenv/partition/mwrite/reset，未刷固件、未取得 SSH。

## 原厂备份
bootloader 2097152 bytes；tpl 8388608；boot0/boot1 各 6291456；system0/system1 各 41943040；data 20840448。
全部 mread 返回 OK，文件长度与请求一致。manifest.csv 保存当前项目副本 SHA256。尚未双读比对、解包验证或恢复测试；仅为已读取分区集合，不等于含所有设备密钥/校准数据的完整闪存备份。
首次按旧研究示例读取 system0 0x2820000 失败，实际传到 0x2800000；后改为 0x2800000 重读成功。其他大小来自公开教程，尚未取得本机权威分区表验证。
原副本 C:\Users\gitai\Desktop\LX06-backup-20261102 保留。目录日期沿用创建时命名，不代表时钟已核准。

## 已有研究与判断边界
- LX06 公开拆解：Amlogic A113X 四核 A53、256MB DDR3、128MB NAND；本机尚未通过 Linux 实测。
- ARMv7 编译目标是现有改造项目所用用户态目标，不能因此把 A53 硬件说成仅支持32位。
- Open-XiaoAI 与 MiGPT 原仓库已归档。Open-XiaoAI 有 1.94.13 对应补丁与原版固件。
- 原项目客户端转发音频/事件，依赖 Server；省掉外部服务器需要自研机内逻辑，尚未实现。
- 原厂服务、ubus、语音/播放事件有公开研究；旧版家电控制入口不保证适用 1.94.13。
- 云端模型 + 本地轻量程序有希望；资源占用、回声消除、打断和米家调用需要实测。
- 网易云官方历史 Linux 桌面版不能直接等同于可用于 LX06；账号与音源接口仍待验证。

## 后续步骤
1. 核验备份：重复读取关键分区对比哈希，检查镜像头、分区布局、固件版本和原厂服务，避免打印账号凭据。
2. 下载并静态审查与 1.94.13 匹配的补丁/原版，记录来源、哈希与实际修改；先明确恢复边界。
3. 制定最小写入方案，保留原厂内核和另一系统槽位；任何刷写前说明具体分区、镜像与后果。
4. SSH 后读取 CPU/内存/存储、进程/ubus/音频服务；验证播放和家电控制。
5. 开发机内 API 客户端、音频调度、工具调用，再研究音乐和连续对话。

## 最新进展：开启 SSH 优先
用户明确要求先按 Open-XiaoAI 开启 SSH，系统逆向分析留后。
boot0、boot1、system0、system1 已第二次从设备读取，四者与原备份 SHA256 全部一致。验证副本位于 rom。
SSH 补丁来源：https://github.com/idootop/open-xiaoai/releases/tag/LX06_1.94.13
文件 LX06_1.94.13_patched.squashfs；发布页 SHA256：3fbcfcd8a2130f5421d0bc3a3b22cb5a3de59b5d3678a3a14b23541aa9474225。
原版 LX06_1.94.13.squashfs；发布页 SHA256：317a54909f12b63f22128965bf9005c06435d15f61c4202bb88c4524bf1e2ef4。
本机命令行 HTTPS 下载失败：curl schannel SEC_E_NO_CREDENTIALS；Invoke-WebRequest 连接关闭。尚未取得补丁文件。
后续：用户浏览器下载补丁到 firmware；核对 SHA256、SquashFS 文件头和小于等于 40MiB；说明具体写入方案后执行项目教程（bootdelay=15，boot_part=boot0，saveenv，写入 system0）；读回核对；启动后验证 SSH。保留 boot0/boot1 内核与 system1。尚未执行这些写入。
注意：boot.img 以 ANDROID! 开头属于启动镜像封装格式，不代表用户态为 Android。

补丁已取得：firmware/LX06_1.94.13_patched.squashfs，SHA256 与发布值匹配。用户要求移动，Downloads 原文件已通过 Move-Item 移出。下一步可执行此前授权的 SSH 补丁刷入流程。

最新设备状态：已完成 system0 SSH 补丁刷入并读回 SHA256 匹配；bootdelay=15、boot_part=boot0 已保存。尚未重启、尚未实测 SSH 登录。此前‘未刷写’描述为历史状态，以此处为准。详细输出 logs/ssh-flash.txt。

SSH 已验证成功：root@192.168.100.210，使用补丁默认密码。硬件 LX06、ROM 1.94.13，内核 Linux 4.9.61 aarch64（实测）；不能把 ARMv7 编译目标当成内核架构。IP 为 DHCP 分配可能变化。用户要求先启用 SSH，现已完成，深度系统分析留后。

## 系统分析第一轮（2026-11-01 北京时间）
用户已装回外壳，授权通过 SSH 及本地固件系统性分析，要求报告、保留所有原功能并改接豆包。
报告已生成：.agents/notes/reports/LX06-豆包接入分析报告.md。下轮先读报告。
实机修正：四核 A53，频率暴露 1.2GHz；内核收到 128MiB RAM（dmesg 131072K），MemTotal 118760kB、MemAvailable 约50MB。物理芯片是否更多未验证。用户态 ELF32 ARM，内核 aarch64；发行文件 LEDE/meson/axg_32/glibc。
/data 可用约8.2MiB；/tmp 消耗 RAM。不能按256MB内存做预算。
实机分区表：system0=0x2800000；system1=0x2820000。先前 system1 只读40MiB；从 Linux 读取尾128KiB失败，dmesg有I/O错误及factory bad block；不能称完整闪存备份/保证恢复。失败读取副本归档 rom/system1-ssh-read-partial.img。原system1 SquashFS归档测试通过，ROM为1.88.221，未执行回退。
本机原厂system0与补丁普通文件差异：1984相同、11变化、1新增；符号链接受Windows权限限制未全解出，但7z归档测试通过。补丁除SSH还改ALSA、libxaudio_engine（仅6字节）、禁用OTA。严格全功能保留尚未满足（至少OTA改变）。
短测 arecord -D noop -r16000 -fS16_LE -c1 -d1 /dev/null成功；拾音进程PID不变。未保存音频、未测语音质量/AEC。
原生 ubus mibrain/mediaplayer/alarm/Mesh 等方法签名已保存；播放器与Mesh状态查询成功。原生技能文字执行、豆包鉴权和语音替换未测。
instruction.log 当前不存在；源码和二进制有线索，先找生成条件/可靠会话分流，不能假定监听ASR日志就能阻止原厂回答。
未停止任何原厂服务，未控制家电、改闹钟、音量或认证信息，未部署开机项。本轮除之前刷补丁外不再刷写。
豆包相关HTTPS可达且证书校验0，未鉴权文本返回401/语音域名根404；不能当成模型调用成功。未取得用户密钥。
源码包已存 rom/upstream/open-xiaoai-main；ZIP commit注释bc3396c64e2a435f354eb5cb12a203981f1fe422。PC TLS失败，改为通过音箱curl流式传回本地，没有在音箱持久安装工具。GitHub API读取commit遇403，但ZIP注释已提供快照commit。
本地7zip可移植解包工具已从官方7zip站下载23.01，未安装系统服务。tools/ssh-askpass.cmd只含上游公开默认口令，不是用户私有密码；tools已gitignore。
后续建议：ARM机内最小程序->豆包文本鉴权->用户指定设备的原生技能验证->旁路事件->按会话分流->实时音频->完整回归。详见报告，不应直接照抄源码中的shell拼接和重启原语音进程。

用户新方向：唤醒阶段小爱同学/豆包豆包双入口共存。详见.agents/notes/reference/wake-routing.md。上游KWS文档对128MB LX06有加载失败限制，部署约20MB；当前/data空间不足。示例实际仍唤醒原小爱，需自研豆包路由。未安装KWS或改设备。原报告文件被其他进程占用，补充内容独立保存在WAKE_ROUTING.md。

## 原厂唤醒链路第二轮（用户要求直接固件/实机分析）
入口报告：.agents/notes/reports/原厂唤醒链路-直接固件分析.md；应优先阅读，不能再用Open-XiaoAI的KWS资源限制代替本机能力判断。
实机读取native-wakeup-runtime.txt确认mipns-xiaomi加载libxaudio_engine.so，打开pcmC0D3c；配置中的wakeup_model.bin实际不存在。
从原厂ELF符号、ARM反汇编、GOT重定位还原：PaddyEngineImpl::Init创建1/2状态，RiceEngineInit->RiceLoadModelBin默认分支复制内置mdata_wakeup_mobile（639597B）/clone（1678701B）。模型内keyword-seq为数值串3,4，不是中文文本关键词；未恢复标签映射，不能承诺可改成豆包。
排除误导性导出：set_wakeup_prefix_suffix仅syslog；PaddyEngineImpl::LoadModel为mov r0,0;bx lr；register_callback也仅日志返回。xaudio_register_callback确实写函数指针，init_system/create_xaudio回调边界值得进一步分析。
定位mipns调用点：0x28a64->init_system，0x2677c->mdspeech_wakeup_info_send，0x26d80->mdspeech_create，0x26e34->厂商ALSA raw read callback，0x26010->mdspeech_data_read。属于静态调用证据，未动态下断点。
消息层：从libaivs-message-util.so的protobuf-c描述符提取12个消息。实机本地speech端点是Unix datagram；上行注册、准备、字节流、voip、唤醒延迟；下行准备结果、expect_speech、asr_partial、dialog_finish。类型数字、外层帧、完整时序未恢复，未注入消息。
mdspeech_create/data_read/wakeup_info_send定义在libmdspeech.so；mdspeech_register_alsa_raw_read_callback定义在原厂libasound.so.2.0.0（厂商扩展）。
实机主要文件MD5与本地原厂一致；音频库与patched副本一致。日志与CSV已保存。进程1282/2527保持不变，本轮未录音、未改服务或配置、未上传环境音。
PC工具增加Python嵌入版3.12.8、capstone5.0.3、pyelftools0.31；wheel按PyPI元数据SHA256核验。脚本保存在tools；反汇编/描述符/模型证据在temp/analysis。反汇编常量注释仅启发式，重要推论必须核原始指令/重定位。
后续优先恢复原厂唤醒与处理后音频的ABI、libmdspeech会话路由边界，再研究第二关键词；独立第三方KWS仅备选，不作为主线。

## 原厂会话与回调第三轮
入口：.agents/notes/reports/原厂会话与回调-第三轮分析.md。
实机只读/proc/1282/mem共16字节：音频库导出g_handle -> 0x000cfd30；+0x10 -> 唤醒对象0x003263f0；+0x64回调0x19528、+0x68上下文0x5fc20。与静态mipns_wakeup_1_cb相符。未改写内存。
main0x13bc4～13c00传asr回调0x19740、wake回调0x19528，经0x288e4包装到init_system。真实wake调用r0=context、r1=1、s0=float角度；asr入口r0=context/r1=buffer/r2=length。仅部分ABI，完整init_system参数/返回值未验证。
第二唤醒回调0x18e7c引用mipns_wakeup_2_cb；原始唤醒还走distrib_wakeup.c和libmdspeech多设备仲裁，分流不可忽略winner选择。修正上一轮简化图：libmdspeech不是简单speech socket转发库，本地Protobuf组包/收发实际在mipns/aivs_agent.c对应代码内。
恢复上行type0～7：register、prepare、cancel、stream、latency、voip、tts_finish、multi_channel_upload。恢复下行0～10：register response、prepare response、stop capture、expect speech、truncation、dialog finish、asr timeout、asr partial、disable wakeup、enable wakeup、disconnected。见JSON/反汇编片段。
特别修正：asr_partial子消息仅final+length，不含识别文字。原厂audio bytes也不能直接视为16k PCM（运行参数opus32，需验证）。
已生成native-speech-reconstructed.proto、upward-types.json、downward-dispatch-table.json、callback-abi-excerpts.txt、wire-builder-excerpts.txt。
.ARM.exidx粒度不足，不能据其自动恢复函数范围；对应输出改为方法限制说明。
本轮未动态hook、未注入消息、未保存或上传音频、未停止服务/改变配置。健康快照PID1282/2527保持，Mesh正常；MemAvailable约45096kB。
下一步是受控唤醒的时序/音频格式核对，以及只记录元数据的可撤回观察方案设计；尚未实现第二关键词或豆包接管。

## 持续推进与编码边界（2026-11-01）
用户明确要求不要每次阶段性汇报后暂停，已建立持续目标；报告不是工作终点。继续原厂固件/实机主线，真实缺少接口凭据或必要物理配合时才提出具体需求。
45秒有界只读strace仅记录sendto/recvfrom元数据（-s0），见logs/native-passive-metadata-trace.txt；只观察到心跳/多播，未捕获实际唤醒会话，不能宣称会话时序已验证。观察器已退出，无后台监听。
libaivs_sdk.so原厂导出常量OPUS_BITRATE_32K=32000、OPUS_BITRATE_64K=64000，见native-opus-bitrate-constants.json。opus32不能直接解释为32kHz采样率；仍需查明mipns->aivs本地流是PCM还是已编码Opus。
SDK中存在AudioEncoder及asr.opus.encode_by_client、asr.format.codec等配置字符串，表明存在编码选择分支；尚未证明本机采用哪个分支。新增codec分析脚本和JSON均在tools/temp/analysis下。
已询问用户豆包接口类型（方舟文本/实时语音/两者），此问题不阻止本地分析；未要求把密钥发到聊天。
进一步静态证据：mico_aivs_lab在0x229a4比较vendor字符串soundai，0x229b8/0x229bc条件加载CODEC_OPUS或CODEC_PCM，再0x229c0调用AivsConfig::putString。仅证明云端codec配置选择，不代表本地socket流已为Opus；需继续追踪SDK编码分支。片段见mico-codec-selection-excerpt.txt。

## 第四轮：编码边界与原生程序实机验证
入口报告：.agents/notes/reports/编码边界与机内程序-第四轮.md；操作范围.agents/notes/reference/native-probe.md。
已编写src/native_protocol.c/h、lx06_probe.c、unix_handover_test.c。ARM32 hard-float静态musl探针31056B，在音箱通过20项合成协议检查；ELF32 EM_ARM flags0x5000400，无PT_INTERP。独立socket实验24156B，随机/tmp目录隔离验证路径移位后的双向转发、发送者路径保持与恢复直连。没有触碰原厂socket。代码/脚本均在项目，build已gitignore。
临时二进制执行前远端SHA256核对，结束清除；隔离socket目录也清除。日志native-probe-runtime.txt/native-socket-test-runtime.txt。最后10:25:48健康检查PID1282/2527未变，Mesh支持/启用/运行true，原socket存在，无测试残留。
Zig0.13.0下载到PC、官方SHA校验通过（79163968B，d859994725ef9402381e557c60bb57497215682e355204d754ee3df75ee3c158），不安装设备。主机Windows编译失败(user32导入库Unexpected)，改Linux ARM编译成功，协议在实机验证。设备无base64，首次传输退出已清理，后改printf八进制转义；tools/Run-NativeProbe.ps1支持probe/socket-test。
SDK编码证据：mico_aivs_lab0x229a4～0x229c0选CODEC；SDK0x10c8d0～0x10c8e0默认ENCODED_BY_CLIENT=false，0x18e094～0x18e20c读取并分支到0x18ef58；PCM/SoundAI跳过编码器、Opus构造编码器。GOT基址0x333000、offset0x12a8等经ELF重定位核对。高可信静态推断本地可能传未编码音频，不把opus32等同采样率；实际采样参数/会话分支未动态验证。
下行外层type=1已从mico0x35adc写栈外层+0x0c再0x35ae8打包确认；send助手0x355cc显式sendto sockaddr，无额外头观察。证据codec-boundary-excerpts.txt/sdk-codec-got-xrefs.json/sdk-encoder-config-xrefs.txt。
annotate-native-arm.py增加缓存ELF段以避免重复读取，以及PLT skipdata以跳过SDK入口字面量；先前SDK PLT未遍历完整的问题已修正，SDK注释重生成。仍需核原始指令，常量注释是启发式。
后续：带异常恢复的透明观察器先隔离故障测试，再考虑真实socket；不把当前socket机制实验等同原厂应用接管成功。第二唤醒词、豆包鉴权/语音/打断、原厂工具和全功能回归仍未完成。任务保持active。

## 第五轮进行中：只读会话观察与模型拆包
新增src/native_speech_observer.c，单线程PTRACE_SEIZE观察recvfrom，只用process_vm_readv逐字节读取协议元数据、跳过字节载荷。native_protocol增加reader接口；合成测试21项通过，额外guarded reader确认不读合成音频3B。
实机合成进程验证：ARM32 regset72B、r12入口0/出口1；正常detach后目标继续收包；观察器SIGKILL后目标继续收包。未设置EXITKILL，不写寄存器/内存。.agents/notes/reference/native-observer.md为具体操作与撤回范围。
原厂mico PID2527 / TID2601 / FD8 / socket inode8402对应speech.usock；TID2602/FD9为common.usock不能混用。5秒空闲观察正常结束、TracerPid0、PID未变、Mesh正常，无报文，不能算唤醒验证。
用户已答复现在方便配合，已开启60秒TID2601 FD8观察；第二SSH确认TracerPid785，随后提醒用户说“小爱同学，今天星期几”。当前正在等待该窗口结束，工具session86324；不要重复启动。结束需核对logs/native-observer-runtime.txt、TracerPid0、进程/Mesh与临时清理。
直接固件反汇编推导AudioRecord控制结构ELF VA0x4254e0；仅只读其32B标量配置：capture 48000Hz/8ch/2bytes/ALSAfmt2；output16000Hz/7ch/2bytes/ALSAfmt2。这是AudioRecord中间输出，不等同最终ASR。JSON live-audio-record-config.json。
静态ASR cache ELF VA0x4253f0：只读16B回调控制字段确认+18=0x19740(mipns_asr_cb)，+1c=0x18c48，context+20=0x5fc20，enable+24=0。调用点0x1eaec长度1920。未读取缓存音频。
重要：识别并完整解包了两份内置唤醒模型！容器标记与BINN匹配，但blob长度为固定4B大端（当前上游不同），自写native-model-container.py以边界/节点数/终止位置核验。两个模型各46节点、完整消费639597/1678701B；包含conf、model、fst、am、prior五份blob，保存在temp/analysis/models下，不改原固件。
配置已解码：MFCC40维/40mel/16000Hz，keyword-seq 3,4，frames-per-chunk5，模型版本level1/level2；prior有10项、left_context40/right_context9。am声学网络637996/1677100B。
native-model-fst.py完整消费两份490B OpenFST vector/standard v2图，10状态19弧；输出标签仅0->2为3、8->9为4，其余0。这说明3/4处于同一条识别图路径，不能解读为两条中文唤醒词。标签中文映射尚未恢复。官方/原始库来源需要补充SOURCES。未给模型写入新参数。

第五轮真实窗口已结束：session86324退出0，无需再等或启动。65条消息解析成功，错误0；日志prepare和wakeup_latency两条控制，其余63条stream；音频字段累计157440B，实际只读取元数据961B，未读取音频。已存logs/native-observer-user-wakeup-runtime.txt和health.txt。用户明确反馈“正常唤醒并回答”。10:42:34核查TracerPid0/State睡眠、原PID1282/2527未变、Mesh正常、socket和无临时残留。修正上文进行中状态。
报告已写.agents/notes/reports/真实会话与唤醒模型-第五轮.md，README/WAKE_ROUTING/NATIVE_OBSERVER已更新。
观察器后续源码新增流计数、min/max长度和首条流日志，已重新通过合成/正常detach/观察器SIGKILL测试，但未对用户再开窗口；不要把这些新统计写成前次窗口实际取得的数据。旧真实窗口日志已单独归档，不要覆盖。
模型解析脚本对am不再输出权重中的随机ASCII串；只保留结构化标签/头部，decoded/manifest已重生成，FST结果不变。
下一步可继续：恢复原声学网络am自定义加载格式/标签语义（头部34等字段未解释，不要凭数值猜层数）；进一步静态证明最终ASR音频格式；准备动态glibc程序复用设备现有libcurl/TLS和原生音频库，先做无网络/不打开音频的ABI探针再接接口。现有static musl探针不能直接当作dlopen原厂glibc库的基础。豆包接口类型问题仍未答复、未取得凭据，暂不阻断这些本地工作。

## 第六轮：复用原厂动态库和C程序TLS验证
报告.agents/notes/reports/原厂动态库与云端传输-第六轮.md；新增src/native_library_probe.c、arm_glibc_entry.S/c、tools/Build-NativeGlibc.ps1和native-library-elf.py。当前二进制7664B，ARM32 hard-float，依赖libc.so.6/libdl.so.2、GLIBC_2.4导入版本。
Zig自动glibc链接时frontend内部崩溃；源码对象单独编译正常，改用自写最小启动入口与原厂libc-2.25.so/libdl-2.25.so链接。产物无PT_INTERP，必须显式/lib/ld-linux-armhf.so.3执行；本轮初次误直接执行探针SIGSEGV。已修正运行包装及ELF校验，成功运行，不能再直接exec该产物。自写入口无应用构造函数支持，不可未经核实套用任意C++程序。
重要副作用：原厂coredump.sh运行过，/tmp/.core_history/lx06-agent-libr_11时间10:54。脚本会mtd_crash_log归档并成功才标记；未读/删归档，具体存储未核实。因此本轮不能声称系统完全无写入。仅测试探针崩溃，原厂PID1282/2527保持，Mesh正常。失败日志和清理检查均保留。后续程序main设自身PR_SET_DUMPABLE=0并读回0（已实测），不改全局coredump配置，也不能防main之前的入口错误。
默认库探针用strace -f -s0 -e network验证无网络系统调用，RSS最新940KiB；复用glibc2.25/libcurl7.55.1/OpenSSL1.0.2l/c-ares1.13.0。easy_init/setopt/perform/send/recv/getinfo/cleanup均存在。
--tls模式仅固定HEAD，保留证书/主机名校验、禁重定向/代理、5秒connect/10秒total；Ark/api/v3/chat/completions返回401、openspeech根404，均curl0/verify0；最新握手155/104ms，峰值RSS1992KiB。未带Key、未请求模型、未上传音频，不能当鉴权成功或完整Agent资源预算。logs/native-libraries-runtime.txt和native-tls-runtime.txt；早期成功和失败输出分开保留。
PC已下载curl官方7.55.1头文件及COPYING到vendor/curl-7.55.1-headers，构建使用真实头定义避免手写选项编号。设备没有安装/更换任何动态库。
用户明确回答“两种都有”。新主线实时语音+方舟文本辅助；已创建config/credentials.example.json和private/credentials.json（只在不存在时复制，后续绝不覆盖），.gitignore新增private/。tools/Check-LocalCredentials.py只输出存在/格式标志，禁止打印值。.agents/notes/reference/credentials.md有说明。当前空模板检查全0；用户已收到填写或给已有文件路径的异步问题，尚待回复，密钥不应进入聊天/日志。
官方当前Seeduplex3.0为新JSON Realtime事件，不是旧StartSession二进制。官方搜索可检索docs.volcengine.com/docs/doubao-voice/end-to-end-real-time-voice-full-duplex-version?lang=zh，但web open超时，search返回官方正文可用。已核对路径/api/v3/duplex/realtime/dialogue，X-Api-Key；session.create里model固定1.2.6.1（营销名3.0不要误填模型值），输入16k pcm/speech_opus、输出24k pcm_s16le可选，建议20ms640B输入，麦克风停止需input_audio_mute.commit避免超时。使用前仍需下载官方完整示例/核对字段与音色，不用社区旧版事件替代。
后续具体动作：读取项目私有凭据只做存在/格式检查，不输出秘密；准备受限认证客户端（设备已有libjson-c.so.2.0.2约30KB可复用，需核对符号/头，不要臆造ABI），固定短文本/小输出上限验证Ark；实现WS TLS raw通过现有curl_easy_send/recv，不追加大运行时；Seeduplex session.create/close先不接麦克风。第二KWS/ASR格式/原厂音频仲裁与功能回归仍待完成，不因只缺Key就停止所有本地实现。
最后11:06:56原PID1282/2527、Mesh正常、无/tmp/lx06-agent-*或core.lx06*残留。动态启动问题已处理，任务保持active。

## 第七轮进行中：机内Ark客户端
新增src/native_ark_client.c。动态复用原厂libcurl/json-c，固定Ark HTTPS POST，max_tokens32、不重试、5s连接/20s总超时、响应上限64KiB、证书和主机名校验开启、禁代理和重定向。凭据设计为stdin输入，不经argv；自身dumpable关闭，不打印响应正文/密钥，HTTP200还需choices/message/content形状有效。尚未实际联网执行此客户端，不把源码功能当实测。
Build-NativeGlibc.ps1新增-Ark，ELF校验器支持明确白名单两种二进制，仍要求原厂显式加载器、libc/libdl依赖和GLIBC_2.4。Run-NativeProbe新增ark-test，不读取凭据，只运行无网络校验自检。
实机ark-test通过（头注入/非法model字符等检查），原PID1282/2527未变、临时文件已删；logs/native-ark-test-runtime.txt。最终SHA256 334f49882dc52275b635ad80baafb6c88f7ac74dec3d473a3bab48fe28b03144。注意自检尚未覆盖json-c解析/HTTP响应解析，也未证明鉴权或完整资源占用。
凭据检查仍全0；不重复追问。下一步实现秘密stdin专用SSH运行包装（不能用现有sh脚本stdin混合传密钥）；补充原厂JSON解析无网实测，再有凭据时做一次固定短文本认证。Realtime WebSocket/双唤醒/音频仲裁仍未完成。整体目标active。

第七轮后续实测：Ark原厂json-c解析自检已通过（正常completion及错误/截断/空数组/null等），并检测字符串内NUL。最新Ark SHA c94b259bb169ff7e9cfa551313b932638f4799030f8d9f8387d66e213b34b8e1。Invoke-ArkSmoke.ps1新增独立固定SSH命令+JSON stdin通道；-TransportTest仅公开假密钥、不联网，实机通过并清理，PID1282/2527和Mesh正常。真实模式尚未调用，凭据模板仍空。
新增native_library_probe --ws-probe，Run-NativeProbe -Kind ws-transport。CONNECT_ONLY=1 + easy_send/recv复用原厂TLS可用，RAND_bytes/EVP_EncodeBlock生成握手nonce；不带认证的固定Upgrade请求收到HTTP400/header1217B/TLS verify0/RSS1992KiB。仅传输拒绝验证，不是101握手成功，未发送session或音频。HTTP原始头未保存/打印。
新探针clock_gettime/__isoc99_sscanf导入GLIBC_2.17/2.7，原厂libc版本定义核对存在后扩展该二进制版本白名单（Ark仍2.4）。首次构建校验拒绝后未部署，修正再执行，无崩溃。报告.agents/notes/reports/本机认证客户端与实时传输-第七轮.md。下一步完整WS握手/帧边界与实时会话schema，第二唤醒/音频仲裁主目标尚未完成。

第七轮再推进：新增src/native_websocket.h帧头解析/客户端掩码编码，不协商扩展，限制单帧64KiB，拒绝server mask/RSV/保留opcode/非最短长度/碎片控制帧/非法close长度。RFC6455 Hello掩码向量、部分头与64KiB边界/超界测试在音箱通过，日志native-ws-test-runtime.txt，临时文件已删、PID不变。此模块未整合网络会话；UTF8/close码、跨帧消息分片状态、101头与Sec-WebSocket-Accept仍待实现，不能称完整WS客户端。探针最新SHA f67eb6461b72a48167774fb63ec4c5acfe3e9f6c9e370a8ef565050938772d8c。

## 第八轮：实时握手/流处理与直接音频分支
新增native_websocket增量接收器（调用方缓存/累计64KiB）、跨帧UTF8、控制帧/Close校验及101/Upgrade/Connection/唯一Accept校验，拒绝未协商扩展/子协议。音箱ws-test通过基础向量+33项流/握手检查+原厂libcrypto RFC Accept固定向量，日志native-ws-test-runtime.txt。仍不是完整符合性测试。
native_library_probe --ws-stdin新增语音Key stdin -> 固定端点X-Api-Key ->101+Accept ->masked Close1000/等待3秒close代码，未发送session.create/音频，认证分支尚无实际凭据实测。--ws-check-key仅无网校验。所有接收数据不输出正文。
新增通用tools/Invoke-DoubaoSmoke.ps1，-Realtime取realtime.api_key原始stdin，默认Ark JSON；旧Invoke-ArkSmoke薄包装兼容。Realtime TransportTest在设备通过/无网络/PID与Mesh正常/清理，日志realtime-stdin-transport-test.txt。凭据检查仍全0。不得把之前此文件的transporttest=假Key当真实鉴权。
无认证Upgrade新代码实测400/头1224B/verify0/RSS2004KiB，并清理。最新probe SHA d34447d5b3741e95cfcc3a788261fb904d61404369dae9da9a59685b718a56ea。未启动任何常驻程序；原厂PID1282/2527未变。
直接固件：新增native-asr-boundary.py，输出7函数491行。xaudio_wrapper_dnn 0x16148调用AFE_DnnProcessPostAgc；wrapper_wakeup 0x200d0调用WakeupProcess。旧audio-format中1ef44..1ef7c显示engine+44的buffer/length，长度/2并条件性buffer+=半长后cache_data_save；可确认选半缓冲，具体语义尚未证明，不直接命名声道。最终ASR格式仍待验证，不把7ch中间流送豆包。未读音频或执行原厂音频库调用。
报告.agents/notes/reports/实时握手与音频分支-第八轮.md；操作.agents/notes/reference/realtime-client.md。下一步隔离验证真实认证代码控制流（模拟server、无真实Key/云会话），官方session schema/音色，再追ASR最终格式。目标active，未完成双唤醒/音频仲裁/米家/全功能回归。

用户已暂停目标，随后要求handoff to file。已写项目根HANDOFF.md作为接手入口；它覆盖最新状态、凭据缺口、设备/构建限制、未完成项，以及暂停前AFE_DnnProcessPostAgc两队列的新静态线索。暂停后仅本地整理，没有再次连接音箱。恢复前必须等用户明确恢复，前文active记录已过时。

## 第九轮：DNN帧结构落定与认证控制流回环验证
用户2026-11-01明确恢复，选择"恢复并连设备"。12:55复核：IP仍192.168.100.210，PID1282/2527、TracerPid0、Mesh support/enable/running true、MemAvailable45716KiB、/data可用8344KiB，两个speech socket存在。删除/tmp遗留的lx06-session-metadata.trace（2941B，内容已存logs/native-passive-metadata-trace.txt），.core_history保留。
静态：新增tools/native-dnn-frame.py与temp/analysis/native-dnn-frame-layout.md、native-dnn-frame-disassembly.txt。队列元素为int16（CreateElementQueue按capacity<<1申请，入队ldrsh/strh逐样本），0xa0=160样本=320B。AFE_DnnProcessPostAgc把in+0x140的160样本与DNN的256样本分入两队列，各攒够160后输出640B：out[0,0x140)与out[0x140,0x280)各160样本。_pass是直通拷贝，推理只在DnnWeProcessPostAgc。唤醒线程对buf+len/2做全零检测，命中打"asr data all zeros, switch to backup"并翻转半缓冲选择；默认保存第二半。
仍属推断：运行时len是否640；保存前全局门[obj+0x40]==1的语义；缓存后是否Opus编码；16k/10ms与AudioRecord 16000配置一致但未由该函数证明。
动态：native_library_probe新增--ws-test-server/--ws-local，用原厂libcrypto算Accept，在127.0.0.1回环演练101/正确Accept/错误Accept/400/不回Close四种场景，同一条认证分支代码。good场景客户端pass且close_code=1000，服务端确认收到客户端关闭帧；其余三种均按预期拒绝。日志logs/native-ws-loopback-runtime.txt。这是明文回环，不等于真实云端101；真实认证仍需凭据。
探针SHA256 35e8579dcfdeaf87ec96ddbc771051014a71bc675d6514f7bc9e7a18bf8f762f，ELF校验通过，原厂PID/Mesh未变、/tmp无残留。报告.agents/notes/reports/音频帧结构与认证控制流-第九轮.md。
下一步：用标量方式核对运行时帧长/门控；官方session.create/close与音色；有凭据后各做一次受限文本与实时升级；第二唤醒、音频仲裁、米家工具调用与功能回归仍未完成。

动态复核：新增src/native_afe_frame_test.c，只dlopen原厂libxaudio_engine.so，用导出队列与AFE_DnnProcessPostAgc、伪造节点对象喂12帧合成数据，不打开音频设备。实测第0次-1，之后每帧640字节，平面0起点0/160/…/1440连续，平面1按第k次调用5000+1000k；afe_frame_test=pass frames=10，与静态推导一致，日志logs/native-afe-frame-runtime.txt。首版把对象+0xc当内嵌缓冲导致测试进程SIGSEGV，实际该字段是缓冲指针；崩溃限于测试进程、dumpable=0、未新增.core_history，原厂PID不变。测试二进制SHA256 f43e658200d08dc2488ed9a715d39983ee2ab9b4d8ebdd0cd5ad1d4beac57cf4。
协议核对：官方Seeduplex页面（更新2026-10-27）正文确认端点/X-Api-Key/model固定1.2.6.1/输入16k且音频Base64放进JSON文本帧input_audio_buffer.append/20ms640字节/输出仅24k可选pcm或pcm_s16le或ogg_opus/voice必填/关麦input_audio_mute.commit/判停input_audio_buffer.commit/打断response.cancel/结束session.close，以及全部上下行事件名。S2S主打音色Vivi2.0 zh_female_vv_uranus_bigtts、小何2.0 zh_female_xiaohe_uranus_bigtts、云舟2.0 zh_male_m191_uranus_bigtts、小天2.0 zh_male_taocheng_uranus_bigtts；文档示例的vv_jupiter与音色表不同，需按控制台确认。已写入.agents/notes/reference/realtime-client.md，属公开资料整理，未实测。
## 第十轮：真实Key认证与无麦会话
用户提供语音API Key，只写入private/credentials.json的realtime.api_key，经独立SSH进程stdin传给探针，未进日志/argv/远端shell。Shape检查：realtime就绪，ark仍空。
真实升级：101/verify0/Accept校验通过/close1000，ws_operation=auth-upgrade-close result=pass，日志logs/realtime-auth-smoke.txt。
新增--ws-session与Invoke-DoubaoSmoke -Session：发送JSON session.create（model 1.2.6.1、input pcm 16000、output pcm_s16le 24000、voice zh_female_vv_uranus_bigtts），收到session.created，发session.close后收到session.closed，再完成WS关闭1000；session_created=1 error_event=0 session_closed=1，全程未发音频、未开麦克风。说明官方JSON协议与音色ID被服务端接受。
探针SHA256 a5e3b8b56af71e2637b3b75a289ec786965271957dd1721b0236fa849756fdc9，峰值RSS2024KiB，原厂PID/Mesh正常、临时文件清理。
未验证：音频上行/下行、mute/commit/cancel真实行为、64KiB帧上限、Ark文本。Key曾出现在聊天，建议轮换。报告.agents/notes/reports/真实认证与无麦会话-第十轮.md。
第十轮补测：方舟Key与doubao-seed-2-1-lite-260915已验证。先用不存在的模型名得到404（鉴权通过），chat/completions返回200且choices形状有效；新增--responses模式走/api/v3/responses，max_output_tokens=256时返回output[0]=reasoning、output[1]=message并通过形状校验。32 token时推理耗尽预算导致无message，说明做Harness要预留推理预算或关闭思考。文本模型会把推理作为结构化item返回，实时语音协议没有对应事件。日志logs/ark-auth-smoke.txt，报告第十轮已补第六节。
## 第十一轮：机内实时语音最小闭环
采集走/etc/asound.conf已有的dsnoop PCM mico_record（16k单声道S16），播放走default（softvol→dmix→hw:0,2，24k单声道），原厂进程不停、配置未改。新增src/native_duplex_client.c：复用原厂libcurl/OpenSSL/json-c/libasound，https+CONNECT_ONLY手动Upgrade，20ms/640B一包Base64进input_audio_buffer.append，下行delta解码进60秒环形缓冲由播放线程写ALSA，播放时麦克风置零做半双工回声保护，退出前播完余量。
修复的坑：wss://不被工厂curl支持；音频字段是delta；4KB缓冲截断大消息；环形缓冲未初始化致播放线程秒退；2秒队列丢包；静音18秒服务端断会话。
最终实测：transcript=38、audio_delta=27、imported=488134B、written=244067帧且与解码样本一致、audio_peak=11317，用户确认能听到完整回答。报告.agents/notes/reports/机内实时语音最小闭环-第十一轮.md。
限制：持续上传（空闲计费）、半双工、定时结束、无唤醒常驻与断线重连、未接米家工具。
## 第十二轮：机内唤醒词检测（2026-11-01）
入口：.agents/notes/reports/机内唤醒词检测-第十二轮.md；操作：.agents/notes/reference/kws.md。
实现 src/native_kws.h（纯 C、无 libm：自写 cos/log/sqrt、MFCC 25ms/10ms/512FFT/24mel/13+13 维、Sakoe-Chiba DTW、端点裁剪、滑窗检测）+ src/kws_templates.h（3 个模板，生成物）+ tools/kws-build-templates.py；客户端新增 --kws-score/--kws-capture/--kws-live/--kws-wav/--kws。Build-NativeGlibc 加 -ffp-contract=off，主机与设备打分逐位可比。
设备实测自检：8 段中剔除 300ms/3180ms；6 段两两 DTW 选 0/1/3（780/640/850ms）为模板，簇内最大 11.3680，阈值 14.7784；判决 0.0001/8.2515/11.3680/14.0157 低于阈值，2290ms 并段因长度门限拒绝，530ms 弱段 16.2833 高于阈值。整段滑窗 5 次判醒（1820/4420/11820/13620/16420ms）全部在说话处，尾部噪声窗最低 16.8997 不误触发。
现场负样本实测：--kws-capture 15s 最低 17.0379、--kws-live 30s 最低 18.9865，均 0 次误唤醒；--kws-wav 回放 1854ms 判醒、前滚 4800 采样，stdin 无 Key 时停在 detection_only 未联网。每次运行后 PID1282/2527 未变、临时文件已删。
重要修正：此前 CMN+全维+阈值 17.72 的版本在现场 45s 出现 15 次误唤醒（日志已被覆盖，未留档），改为不去均值 + c0 包络×0.25 + 运行底噪门后才归零；属实测结论。
限制：注册录音噪声大、同词多遍距离 8-14 与现场其他人声 15-17 分布上沿重叠；弱段会漏检；唤醒后真实 Key 的会话闭环尚未实测。第十二轮补充（现场负面结果）：安静期 30s 待机 0 误唤醒、15s 采样 0 判醒；但房间持续有人说话时 20s 出现 8 次误唤醒（最低距离 10.5620），说明 3 模板 + MFCC/差分 + 20% 带宽 DTW 对任意 0.6-0.9s 连续语音判别力不足，阈值只能在安静环境用。最终二进制 SHA256 0303e2b7ee4955b2c5c27960a387bffd9391b05424a1dc3a182d6c5cfc25e921；--kws-dump 输出特征供模板生成。后续建议：安静环境补录、提高分辨率、LX_KWS_MIN_HITS 提到 3-4、最终判决交给小模型。第十二轮重构：会话抽成 run_session()，--kws 改为"待机→判醒→会话→回到待机"同进程循环（新增 stage=standby_resumed，kws-standby 测试入口）；重构后自检数值不变，二进制 SHA256 721019add167d8d11b3db2fc3e750f4e7b7cc8dbbf7dda7ca4314ef61354742d。--kws 12 实测在 5.06s 被室内说话声判醒并停在 detection_only（未联网）。第十二轮回环补充：重构后 --duplex 8 与 --kws-wav+真实 Key 均跑通（判醒→101→session_ready→前滚 4800→上行→关闭，session=pass）；回放无提问所以没有回答内容，"说话→回答→播放"仍未验证。最终二进制 SHA256 a57d60c6d3371bf9cb7465f302727abfe6b372071461efb2cebc752dae935313。修复了重构中漏绑 RAND_bytes/EVP/json 的问题并加 session_failure_stage 便于定位。
计费风险事件（2026-11-01 14:52）：用户发现持续计费后核查设备，发现 /tmp/lx06-agent-realtime-62d858e0（PID 3509）仍在运行并保持到 openspeech.bytedance.com（157.255.232.105:443）的 ESTABLISHED 连接、持续上行音频。已 kill 该进程并删除 /tmp/lx06-agent-*；同时停止仍在现场测试的子代理与它拉起的 --kws 待机进程（PID 3554）。复核无 lx06-agent/doubao 进程、无 openspeech 连接、无自启动项，原厂 1282/2527 未动。根因是演示客户端在进程存活期间持续上传，只有约 18 秒无音频时服务端才断会话；后续必须按唤醒开会话、回答结束即关闭，现场测试结束立即杀进程。KWS 阈值与误唤醒问题仍未解决（当前 3 模板、阈值 14.7784 仅适合安静环境）。
## 第十三轮：误唤醒收紧、会话上限与常驻部署（2026-11-01 15:09）
本轮先处理计费事故（见 logs/operations.md 第 78 条），再完成剩余四项。
KWS：阈值从 1.3×簇内最大（14.7784）改为 1.10×（12.5048），连续命中从 2 窗改为 3 窗；模板不变（段 0/1/3）。新增离线负样本集：Windows Huihui TTS 生成 53 条 16 kHz 中文语音（34 句日常话术及语速变体），主机打分 0 次误唤醒、最低距离 19.68；现场高噪声（峰值 52–58 dB、底噪 34–46 dB）待机 20 s 与 15 s 均 0 次判醒，最低距离 17.84/15.29。回放注册录音在 2.07 s 判醒，无 Key 停在 detection_only。阈值收紧的代价：注册录音 4 段同词里只有前两段能在滑窗判醒，第三段被相邻长语音并段、第四段距离 14.02 高于阈值；现场可能需要说两遍。
会话安全：新增 LX_SESSION_MAX（默认 25 s）与 LX_NO_SPEECH（默认 6 s），SIGTERM/SIGINT/SIGHUP 置停止标志后退出循环并发送 session.close；日志区分 max_duration/no_speech/signal。真实 Key 回放闭环：判醒→101→session_ready→前滚 4800→299 帧→6 s 无人声触发 no_speech→session=pass；结束后无进程、无连接、无临时文件。8 s SIGTERM 测试打印 standby_stopped=signal 并退出 0。
部署：新增 --daemon（fork+setsid+pidfile）；deploy/start.sh 用 0600 FIFO 传 Key（不落盘、不进 argv），stop.sh 杀客户端与写入者并删 FIFO，status.sh 查看进程/连接/日志尾。已安装到 /data/lx06-agent/（仅新建该目录，8.2 MB 可用），二进制 SHA256 688100848D23532CBAA22D07E882FE9BA7A00D414A63666848DC09CF08D53870；未配置开机自启。Manage-LX06Service.ps1 提供安装/启动/停止/状态，-FakeKey 可只验证启停。
未完成/风险：没有用真人持续说话的现场负样本复测误唤醒；他人声音、远场、电视音乐长播未验证；唤醒词需相对孤立；Key 只在启动时传入，重启后需重新启动服务。
## 第十四轮（2026-11-01）：唤醒/灯效/连续追问
用户明确继续并要求修复低唤醒率、连续对话、“我在”及会话全程原厂灯效。详见.agents/notes/reports/唤醒反馈与连续对话-第十四轮.md、.agents/notes/reference/kws.md。实际修复模板帧偏移漏乘26，现4模板0/1/3/5，阈值和3窗不放宽；53合成负样本0触发、15秒现场0触发。用户稍后补录。接入原厂wozai.wav与效果1/2/3收音/思考/播报；用户确认。真人两轮第三窗口捕获2次完成与恢复追问，用户说“全都正常”。第一窗口因snd_pcm_delay判完成失败，已改drain/prepare。最后将追问活动从能量改为云端ASR，防噪声延长；最后小修未再真人验证。当前安装SHA D46ABEA0B35B907E74044A184A2E4258EE70271C69CE2E91F8952694A5FB0D5D，60s硬上限/8s追问/6s初次无语音，21策略测试通过。测试后服务停止、原PID1282/2527/Mesh正常。无开机自启，待机超时仍退出；同时小爱灯层仲裁/全功能回归未完成。旧版在build/revision14-original，撤回见.agents/notes/reference/deploy-revision14.md。

## 工具接入开始
用户要求列举功能并按顺序实现。已重新只读查询实机mediaplayer/alarm/mibt/miio/Mesh/ir_agent/nightmode/sound_effect/miplay接口，保存logs/tools-capability-inventory.txt；顺序见.agents/notes/reference/tools-roadmap.md。下一步工具调用基础+音量/播放状态，再控制与闹钟、米家/红外。当前未注册豆包tools，未控制家电或修改闹钟。

## 聚合音乐方向（2026-11-01）
用户允许NAS/路由器辅助，但机内优先，目标扩展多音源聚合。已做只读内存/空间/DLNA状态检查，原厂libffmpeg静态确认MP3/AAC/FLAC/Opus与HTTP/HTTPS/HLS组件；尚未做音乐流播放。评估见.agents/notes/reports/聚合音乐环境-方案评估.md。推荐机内原生renderer+可迁移轻量provider；NAS后备Music Assistant（官方64位/2GB要求，不适合LX06，网易云还需API服务）；Navidrome用于自有曲库。硬件型号/Docker支持已问，待答。未部署NAS/路由器/音乐服务。
`n用户补充后备设备为ARM且支持Docker；尚不清楚具体是NAS还是路由器、arm64/armv7及RAM，不能据此认定能跑Music Assistant。

## 机内优先与清理审计
用户再次确认机内优先。只读审计见.agents/notes/reports/机内空间与内存清理评估.md：/data剩8388KiB，可用RAM约44792KiB；根squashfs只读100%正常；/tmp888KiB。历史日志约1.9MiB为候选，但未删除；2MB miio_mpkg/bin/mibt_mesh_rtl是Mesh程序不能当缓存。发现zram0未配置，未启用。先流式/有界缓冲/原生复用，不清原厂功能。

## 播放地址日志取舍
用户明确接受原厂日志记录播放URL，要求不为此增加代理/转发开销。音乐适配改直接HTTP/HTTPS播放，保留URL长度/协议/控制字符校验；账号密码、Cookie、豆包密钥仍不写日志。之前“必须本地代理隐藏鉴权URL”不再作为要求。

## 第十五轮：机内音乐流基础
新增native_music.h/client.c和回环HTTP测试源，原厂player_play_music真实音乐payload+media=common验证播放/暂停/继续/替换曲目/非播放。此前play_url临时播报路径导致恢复缓存歌单，已用mphelper暂停；后改正确音乐接口。测试缓存队列已替换为测试单曲，不能声称旧歌单恢复；音量未改、循环恢复1。用户接受播放URL日志，取消隐藏URL代理。日志和限制见.agents/notes/reports/机内音乐流与播放控制-第十五轮.md。最新F09BDFD1...通过7参数检查，自检RSS396KiB非播放总预算。全部临时程序退出删除，未部署常驻音乐服务，网易云账号/会员/豆包工具仍待做。

## 灯效收尾与网易云登录
用户反馈灯仍亮。16:02查询无lx06进程，播放为测试单曲非播放status2，led有effect3；只撤销L3后led info为空，原厂1282/2527不变。补充Test-NativeMusic.ps1清理自身测试曲目的残留L3。网易云下一步见.agents/notes/reference/netease-account-plan.md，已核对QR key/check、会员查询和song_url_v1（当前xeapi）上游源码；尚无可扫码入口或账户会话。

## 第十六轮：机内Web和网易云登录
已部署 http://192.168.100.210:18080/ ，HTTP+JSON v1/SSE，原生服务RSS约1.8MiB。用户扫码确认，会员标记110；0600会话持久化，Web重启后刷新验证恢复登录。用户指定萨顶顶《左手指月》536096151，非试听音源、播放器时长230544ms，用户确认正常播放。报告见.agents/notes/reports/Web控制台与网易云登录-第十六轮.md。最新Web SHA C8F0DF425177F6CD8B0CC464FA2F806C8898AC24D5A45DEB505F9BBB948F2AF5；语音788F884C...新增快照/待机续期。收尾voice.stop成功，歌曲仍播放，音量31/循环2为现场读数，未强改回旧值。Web保留，无开机自启。豆包tools分发、米家控制、歌单/队列/自动续播、FLAC未完成。HTTP检查通过；自动浏览器启动被审批拒绝，未做截图。

项目正式命名 XiaoAI Sidekick，副标题“大模型驱动的小爱音箱副驾”，仓库名 xiaoai-sidekick。Web标题、侧栏、首页介绍、页脚及README已统一；历史目录/二进制/设备路径保留，避免破坏部署。当前工作目录不是Git仓库，未执行远端改名。






## Latest: track metadata and factory playlist queue (revision 18)

Deployed track title/artist/playlist/index display, previous/next controls, Netease chart and authenticated user-playlist browsing, and factory queue playback for the first 20 tracks. Doubao tool definitions now include playlist browsing/playback and queue skipping. Factory player owns audio decoding, queue order and progress. Sidekick keeps queue metadata and up to 20 short-lived audio URLs in memory. Longer playlists do not auto-refill; long pauses may allow later CDN URLs to expire.

Web SHA256: `E45B9CB98F49E50E8D21DBE7F6F3D43FBF99D47074C62BA4688F049518071F2A`. Duplex SHA256: `3D561922CB32B946344C5B4DFACC4B7850F6F62737FADBE90E2D5DA8A4C65F9D`. Web PID 3446 is running; voice remains stopped. Factory processes 1282/2527 remain. Pre-feature files are in `/data/lx06-agent/revision18-original/`; the queue release before the final refresh fix is in `/data/lx06-agent/revision19-original/`. Native ARM/glibc builds and ELF checks passed. No automated or live voice tests were run.

## Latest follow-up: account playlist listing fix (round 19, 2026-11-01)

Report: `.agents/notes/reports/歌单列表大响应修复-第十九轮.md`. Field failure: "读取我的歌单" always returned `network_or_tls_failed` while charts, chart tracks and account refresh worked. Instrumentation showed `CURLE_WRITE_ERROR` (23): the provider answered that request with `Content-Length: 728466`, far above the old 262144-byte receive buffer, and the write callback aborted. Replaying the app's byte-identical encrypted body and cookie with the device's own `curl` returned the compact 112820-byte form eight times, so the request itself is fine; why the server returns the larger inlined form to the in-app connection is an open observation (same libcurl, different OpenSSL build).

Fix in `src/native_netease.h`: `NC_RESPONSE_CAP` = 1 MiB, explicit `response_too_large` on overflow, `transfer_error_<code>` otherwise, total timeout 8 s -> 20 s. Temporary /tmp dumps were removed before the final build.

Deployed Web SHA256 `86DA9335C1DC079EB11CE602459628191B95D19E4894E8CAADF082C33FAC9BA2`, PID 2107; Duplex unchanged `3D561922…`. Pre-deploy state saved to `/data/lx06-agent/revision20-original/` (Web `e45b9cb9…`). Factory 1282/2527 and Mesh unchanged; voice stopped.

Live verification: 50 account playlists listed in ~0.9 s; playlist tracks 19 (limit 20); playlist queue 19 tracks; card shows `Nonsense / Sabrina Carpenter / @Gitai的十年精选辑 / 1/19` with advancing position; next/previous, pause/resume, stop and search all confirmed; headless Edge render confirmed the page. Doubao playlist tools still await a live voice session.

## Agent loop 与工具注册表（第二十轮，2026-11-01）

用户要求把流程标准化，便于后续能力接入。本轮完成两件事。

工具注册表：新增 `src/native_tools.h`，集中定义工具名、描述、JSON Schema、动作名和可见范围；模型目录、工具名映射、执行分派共用这一张表。Web 控制台新增仅本机 socket 可用的 `POST /api/v1/tools`（目录）和 `POST /api/v1/tool`（单次调用），执行统一走新拆出的 `action_execute()`；`/api/v1/command` 与页面路径保持不变。语音端删除硬编码的 `tb_tools`/`tb_method`，会话建立时拉取目录（失败时降级为无工具会话并打印 `tool_catalog=unavailable`），工具调用改为转发到 `/api/v1/tool`。实测：目录 12 项；`music_charts`、`music_search`、`music_playlist_tracks` 经新通道返回真实结果；未知工具返回 `unknown_tool`；RPC 与页面路径回归正常。

Agent loop：新增 `src/native_agent_loop.h`，提供 provider 接口（stats/send_audio/send_json/pump）和 host 接口（capture/recover/play_delta/playback_idle/led/publish/tool_start/tool_busy/tool_take/tool_has_ticket）。轮次策略、静音/解除静音、工具轮、追问窗口、会话预算和日志都在这个循环里；豆包实时在 `native_duplex_client.c` 中作为第一个 provider，设备侧（ALSA、播放环、灯效、状态发布、工具线程）作为 host。行为与原内联循环一致，`--transport-self-test` 通过，唤醒回放会话 `session=pass`。

计费相关修正：会话开始 10 秒内没有任何回答开始就关闭（`session_stop=no_response`），避免唤醒后空转；本地语音门限 900→2500，降低环境噪声反复续命无语音计时。实测唤醒回放无后续语音时，415 帧（约 8.3 秒上行）后以 `no_response` 结束，未到 60 秒上限；此前日志里出现过 49 秒空转会话。

## 语音交互调优与 AEC 逆向（第二十一轮，2026-11-01）

详细记录见 `.agents/notes/reference/audio-pipeline.md` 与 `.agents/notes/reports/语音交互调优与回声消除逆向-第二十一轮.md`。

- 新增 `session_end` 工具：模型在用户结束对话时调用，播完最后一句立即回待机；本地工具链路已验证，真人语音未测。
- 唤醒应答改为豆包音色：`--make-ack` 通过实时模型音频注入生成“我在。”，归一化到峰值 30000；`feedback_ack` 优先播放 `/data/lx06-agent/wake_ack.wav`。实时可配 `--wav-peak` 对比。
- 回答音量：实时语音峰值约 −8.5 dBFS，播放时统一乘 `LX_ANSWER_GAIN`（默认 250%，带削顶保护）。
- 唤醒策略：音乐播放时连续命中要求 3→4；第 3 个命中暂停播放器 2.2 秒用干净音频确认；会话 10 秒无回答关闭（`session_stop=no_response`）；本地语音门限 2500。
- 唤醒词打断已实现（播放期间本地 KWS，命中后清空播放缓冲、解除静音、丢弃本轮剩余音频），未真人验证。
- AEC 逆向结论：`hw:0,3` 8 通道 = 6 路麦克风（ch0–5）+ 2 路播放参考（ch6–7，静音时为 0）；原厂 AEC 在 `libxaudio_engine.so` 内，实为 Speex（128/4096 + 预处理 128/16000），设备自带 `libspeexdsp.so.1.5.0`。原型 `--aec-test` 实测音乐下约 19 dB 抑制（5309→585），静音 4843→893；尚未接入正式采集。`speex_echo_ctl` 用 `dlsym` 取不到，原型沿用默认 8 kHz 配置。
- 当前部署：Web PID 3662，Duplex 为本轮原型版本，语音已停止；具体文件与回滚基线见 `logs/operations.md` 第 105、106 条。现场仍存在音乐下误唤醒与漏唤醒，下一步是把 AEC 接入采集路径。

- 记录规范：`.agents/notes/README.md`（中文版 `README.zh.md`）定义 Agent Notes 的 `{lifecycle}/{class}/yyyy-mm-dd-topic-title.md` 布局、六类封闭分类、格式与归档规则；门禁为 `scripts/verify-agent-notes.ps1`。本轮的三份笔记已迁入 implemented/architecture、implemented/feature、proposed/architecture；参考文档与 reports/ 留在树外。

- 文档结构扩展：Agent Notes 之外的文档统一迁入受控区域——手册、协议、策略、部署说明、状态流水与索引放 `reference/`（小写 kebab-case，`# Reference:` 头部），逐轮证据放 `reports/`（`# Report:` 头部），两者不再跟随生命周期分类。门禁同时检查三个区域。

- 材料目录重组：`research/` 拆为 `rom/`（可公开的固件与上游源码，用于无音箱复现）、`private/`（录音、运行时快照、设备回读、凭据）与 `temp/`（根文件系统、逆向分析、清单，可由 tools 重算）；`SOURCES.md` 迁为 `reference/sources.md`。工具脚本路径同步更新，`tools/local/` 收纳 SSH 凭据并加入忽略列表；`tools/` 本身不再被忽略。分类与发布规则见 `rom/README.md`、`private/README.md` 与 `tools/README.md`。

## 采集路径接入 AEC（第二十二轮，2026-11-01）

`src/native_capture.h` 成为唯一采集入口：默认 `Capture` dsnoop 8 通道，ch0–ch5 六麦平均作麦克风、ch6 作参考，3 点平均降到 16 kHz，128 样本一帧跑 Speex AEC + 预处理，输出同时供本地唤醒与云端上行；打不开 8 通道、缺 `libspeexdsp` 或 Speex 初始化失败时回退 `mico_record`，`--no-aec` / `LX06_AEC=0` 强制回退。

关键修复与参数：

- 本机库导出 `speex_echo_ctl`，不是 `speex_echo_state_ctl`；之前采样率一直是库默认 8 kHz。显式设为 16000 后，同一段语音的 AEC 距离从 13.3 降到 9.6。
- 麦克风从单路 ch0 改为 6 麦平均：音乐下语音距离 12.05 → 11.66，刚过 12.50 阈值。
- 预处理会损伤语音约 2.5 个距离点，但它是音乐残留的主要抑制；关掉会误唤醒。
- 音乐下阈值放宽到 15.0，2 连击后暂停播放器 2.2 秒做第 3 窗确认，6 秒冷却；静音环境仍 12.5 / 3 连击。
- 资源：单核约 21%，RSS 约 2.3 MB。

线上验证：音乐播放中两次真人“豆包豆包”以 12.5082、14.6845 在候选暂停后确认，会话正常建立并恢复播放；同一实例有 2 次未确认暂停。用户确认可用。高音量、远场、打断和长时间稳定性未验证。诊断入口 `--mic-compare <秒>` 只在内存里并行打分各采集版本。报告见 `reports/采集路径回声消除接入-第二十二轮.md`。

## 音乐队列播放中断：CDN 直链过期（2026-11-01）

用户反馈播放中“连接失败”。实测原厂 `mediaplayer` 日志：22:50 至 23:15 正常播放 5 首；23:15:17 第 6 首自动切换起连续 10 次 HTTP 403（`err code:-10102, detail:403`），剩余队列全部无可用 URL，原厂播报“播放失败，请检查网络或换一个试试吧”。所有 URL 的时间戳都是 `20261101231505`，即 23:15:05 到期；网络本身正常（RSSI -45 dBm、ping 0.28 ms、TCP 建连成功后返回 403，Mac 复取同一 URL 也 403）。根因是 `nc_play_chart()` 在入队时一次性签发 20 首直链，队列寿命超过直链有效期后不续期。第二十六轮已改为本机 302 现场刷新并支持 100 首与顺序循环，见下节。

## 本地 302 刷新、600 首队列与原生循环（第二十六至二十八轮，2026-11-01/02）

队列 URL 改为 `/r/<provider>/<track-id>/<quality>.<ext>`，例如 `http://127.0.0.1:18080/r/netease/1329831232/exhigh.mp3`。Web 只对回环来源提供该路由；请求到达时用现有网易云会话调用单曲取流接口，校验后返回 302 和 `Cache-Control: no-store`。原始 CDN URL 不再入队，队列寿命与直链有效期解耦。

队列上限从 20 提高到 600（`LX_MUSIC_QUEUE_MAX`）；`nc_play_chart()` 改为单次取 600 首元数据，构造本机 URL 后分 300 首一块入队（`REPLACE_ALL` 后 `APPEND`，再 `player_play_index 0` 回到开头）。`NC_RESPONSE_CAP` 提高到 4 MiB，以容纳 538 首歌单的详情响应。`web/app.js` 的“前20首”改为“前600首”。

原厂播放器本身支持列表循环，之前是参数名传错。实测 `play_mode` 映射：`SEQUENCE` → `loop_type=1`（列表循环），`LoopSequence` → 2（末尾停止），`LoopTrack` → 0，`LoopRandom` → 3。`lx_music_play_queue()` 使用 `SEQUENCE`；不再需要 Web 轮询重排。

实测：`/r` 回环请求 302，目标 CDN 200（11.9 MB）；局域网请求 403；非法 ID/平台返回 400/404；HEAD 同样 302。538 首歌单实际入队 `queue_total=538`、`requested_total=538`，首曲播放状态 1、`loop_type=1`，Web RSS 约 2.6 MB、可用内存约 28 MB。600 首测试队列被原厂接受（请求 JSON 121 KiB、状态响应 8.5 KiB）；1000 首单次详情请求超过 20 秒。部署二进制 SHA256 `52a954bb3bdcf382b0e543ad38927bbc5cce04cfdd8ecbdb6490ae2001b8ad82`，PID 3940；旧版在 `revision21` 至 `revision28-original`。重启时未带语音 Key，`credentials_loaded=0`，语音启动功能需重新提供 Key 后重启。
