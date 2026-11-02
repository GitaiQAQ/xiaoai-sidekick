# Report: Isolated Evaluation and Deployment of the Custom Wake Model

日期：2026-11-02。本文记录原厂模型回喂等价性、离线 fixture、集成测试和后续正式安装。早期隔离实验与后续生产安装是不同阶段，结论分别列出。回滚材料按当前规则保存在 Mac `private/deploy-recovery/`；设备不应保留 `revision*-original` 历史副本。

构建产物：build/lx06-wake-model-probe-armhf，SHA256 6eedc0e7b81e8142a127dc2252380242aa11e176fdc9d408f008db6633fbd6ac。原厂一级容器 SHA256 afeec50b870cf76f567f1d299129f175b3116745b883eb0d9bce00c1f7030342；二级 67d5fb16db8ed8f1665468b39cb5e4e4a7323dafcac9f6919ac78e666f34864a。v5：639597 B，de899046087da8729a245764ef853ba10884b555e9599d01dbb592995476610f。

隔离阶段只使用 `/tmp/wake-model-r30`，未改原厂模型、启动项、分区或正式程序；删除该目录即可撤回隔离测试。该阶段 Duplex SHA256 为 `cf231f7b5a6bb9ff374d51feef5e1a71c7a87a236916c40105052550332e4bcf`，当时尚未执行生产写入。后续正式安装的恢复材料应在 Mac `private/deploy-recovery/` 中按哈希保存；此报告早期方案中曾提及的设备端历史备份路径已废止，不可照做。

ABI：输出 1024 B，显式 8 字节对齐；+0x230/+0x238 用 memcpy 读 double。flag 1 为原厂确认，flag 2 为候选超时，不能称作唤醒成功。所有权按原厂静态代码：RiceEngineEnd 释放当前 +0x68；替换后的旧容器由探针在 Destroy 后释放。重复 Init 的资源及输出等价性需实测。

## 隔离测试与修正

直接执行 Zig 产物会在 main 前 SIGSEGV：当前产物无 PT_INTERP，与既有部署约定一致，必须显式 `/lib/ld-linux-armhf.so.3 ./lx06-wake-model-probe-armhf`。BusyBox timeout 使用 `timeout -t 45`。探针新增无缓冲阶段日志与 `--pcm16le -` 流式输入；工具 `tools/build-wake-model-probe.sh` 可复建。最终探针 SHA256 d16a7699f55862d4f8ede1db8e092cb086f5e43425f1c2100e095557086a769c，初始哈希仅对应早期版本。

正例金标准：既有 r25-tts/xatx.wav（16k mono S16）加 1 秒零音，fixture SHA256 34189d6d7d9311308353e7ef3c317d08ffa5f37a93d9bce4aa5ff94abd6d7483。原厂内置与两份原厂容器回喂均 frame106 一级候选、start0/end103、score7.189797，frame167 外层flag2超时，rc0、Destroy成功。240秒 s3 fixture 两种原厂加载路径均0事件。此金标准证明回喂事件/分数一致，未验证现场“小爱同学”召回。

v5 一级 + 原厂二级：240秒 s3，12次一级候选，12个flag2，0确认。原厂二级识别“小爱同学”，这不代表豆包一级无效。改用自有 l2-d160（1678701B，SHA256 5945e591204a6ce2dedbab1c8bd18197e5e9fb49b1ba9bf4b0f88dd2be00464f）流式输入，rc0，24000帧，18189ms，12事件中8个flag1确认、4超时，峰值RSS9448KiB，Destroy成功。外层+0x230/+0x238分数在已测flag1/2均0；有效分数来自原厂 rice 候选日志，不能把外层这两个double当候选score。

180秒 s2b 纯音乐流式 v5+l2：rc0、18000帧、11344ms、0候选/确认、RSS9576KiB。仅既有离线AEC近似音频，不能代替现场音乐长时验收。

## 内存压力事故与恢复

首次把s3、s2、s2b整段PCM放/tmp使本轮目录约27MiB、系统shared约36.9MiB。后续探针超时/137，dmesg出现 OOM 并杀 messagingagent；原厂 mipns/mico_aivs PID从1282/2527变3576/3667，由监护重启。不能称本轮原厂运行无影响。已删除本轮三个大PCM副本，available恢复约43.3MiB，改从Mac SSH stdin流式供音频，设备不保留录音。本轮未主动停止原厂，但内存压力导致服务重启；完整功能回归尚未做。

重复RiceEngineInit覆盖原有内存池和网络指针，原厂容器回喂峰值RSS比内置多约2.5–5MiB；Destroy成功不证明旧资源无泄漏。隔离单次进程结束由OS回收，生产集成应避免每个待机窗口重复初始化。原厂模型缓冲归引擎释放、被替换旧缓冲由探针另释放；错误初始化后清理尚非完整验收。

日志保留于忽略目录 temp/analysis/wake-probe/{builtin,refeed,xatx-builtin,xatx-refeed,v5,v5-l2-s3-stream,v5-l2-s2b-stream}.log。测试期间未录新音、未云上传、未修改backups或原厂模型；尚未生产启用v5。

300秒s2流式v5+l2：rc0、30000帧、18572ms、0候选/确认、RSS9456KiB。两段纯音乐共480秒0事件，非未来长期零误触发保证。

## Duplex 集成测试（生产启用前）

新增 src/native_factory_kws.h；src/native_duplex_client.c 的 kws_listen 在 `LX06_WAKE_ENGINE=factory` 时使用两级原厂解码器，只接受flag1，保留音频preroll。必须提供 `LX06_WAKE_MODEL1` / `LX06_WAKE_MODEL2`；加载失败停止监听而不悄悄转回DTW。模型每进程只初始化一次，待机窗口复用，开始窗口送1秒静音冲刷旧候选；窗口冲刷与真实音频交界尚未真人验收。旧DTW用于原有诊断和未配置factory的启动路径。`--factory-pcm` 从stdin重放现有PCM，不开音频设备、不读Key、不连云，帧数有界200000。

`tools/build-factory-wake.sh` 编译候选（-Wall -Wextra -Werror通过），build/lx06-duplex-factory-wake-armhf SHA256 4044651056f303d1536e7d4eae7946591412d956a53145114897e084c7e75266。所需libpthread-2.25.so从设备只读复制到忽略的temp/rootfs-factory-system0/lib，未安装驱动。候选/tmp隔离回放s3：24000帧、确认8、input_error0、rc0，与探针确认数及事件帧一致。

OOM后只读回归：原厂mipns/mico_aivs、miio_client、central_lite、Mesh、mediaplayer进程存在；播放器ubus响应code0、status0、volume15、loop_type1；未重启设备。仅证明进程/状态可访问，未验证米家真实设备控制、Mesh组网或真人原厂唤醒。

/data实读可用1224KiB，持久两模型2318298B，加候选约123KiB和旧程序/脚本备份，空间不足。未删除历史revision目录，未写/data正式程序、启动项或模型。建议由主代理形成旧revision逐文件归档与SHA256清单后协调清理；必须保留本轮可恢复副本。临时/tmp只保留模型/候选/日志，无长PCM。恢复正式状态可删除本轮/tmp目录；真正部署前应先保证至少模型+候选+恢复备份的空间。

## 正式安装结果

用户随后明确授权清理全部设备历史部署备份并要求以后不留。主代理完成清理后/data可用7468KiB，本轮不创建revision目录。恢复文件已只读复制到Mac `private/deploy-recovery/r30/`：旧 `lx06-duplex-armhf` SHA256 cf231f7b5a6bb9ff374d51feef5e1a71c7a87a236916c40105052550332e4bcf；start.sh SHA256 5288233348e09f75feb6f2baea8d1366ac7b4b1b5d0218b924a571e886f4c2de。未改start.sh。

候选改为默认factory（`LX06_WAKE_ENGINE=dtw`可明确选择旧诊断），默认模型路径 `/data/lx06-agent/models/d96-v5.binn` 与 `l2-d160.binn`。最终binary SHA256 be273e5494386b815b5aa5937d1fe798429211e2c2f8d767945281e200cb3995，已安装 `/data/lx06-agent/lx06-duplex-armhf`（0700）；两模型0600，设备SHA256分别与de899046…/5945e591…一致，sync后/data可用5300KiB。早期404465…候选仅用于中间验收。Duplex集成s2b180秒：rc0、确认0、input_error0。

恢复步骤：先用现有stop.sh停止Duplex（如果已启动），经SSH上传Mac恢复目录的旧binary到短暂staging，核对cf231f…后替换正式binary、chmod700；删除新models目录中本轮两文件。start.sh未改无需恢复。恢复后再次核对正式binary哈希，按既有受保护stdin提供Key启动；恢复后删除短暂staging。该恢复方案只有文件/哈希校验，未真的回滚新版本验证完整功能。

当前Mac private/credentials.json缺失，历史Web状态credentials_loaded0，正式语音daemon未运行。安装模型与离线验证不等于已经启用豆包待机；用户需在原有受保护流程提供Key，才能启动现有daemon。禁止用占位Key假装运行。

正式路径默认模型实测：未传模型env，`/lib/ld-linux-armhf.so.3 /data/lx06-agent/lx06-duplex-armhf --factory-pcm` 重放s3，rc0、24000帧、confirmed8、input_error0，与隔离探针一致。最终binary123876B。收回所有本轮日志到Mac后已删除/tmp/wake-model-r30，staging_removed1；最终available38092KiB、/tmp占用8828KiB、/data可用5288KiB。原厂/米家/Mesh/播放器上述进程仍在，播放器code0 status0；无新原厂完整功能验收。正式binary哈希仍be273e…b3995。
