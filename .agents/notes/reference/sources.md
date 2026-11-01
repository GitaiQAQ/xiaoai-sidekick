# Reference: 研究来源

第二十四轮以本机 ELF 指令、运行配置固定字段和隔离进程实验为证据，未新增外部资料。见[原厂两级判醒与独立引擎调用](../reports/原厂两级判醒与独立引擎调用-第二十四轮.md)：确认原厂模型初始化/推理可独立调用，实际观察到一级候选后二级超时拒绝；不等于已有“豆包豆包”模型或已修复误激活。

第二十三轮：原厂算法证据以本机 libxaudio_engine.so 的 Rice/Paddy、MFCC/CMVN、Viterbi/IsKeyword 与两级缓存调用反汇编为准，见[报告](../reports/音乐自激活与原厂判醒复核-第二十三轮.md)。备选方案查阅 [Speex 官方源码](https://github.com/xiph/speexdsp/blob/master/libspeexdsp/mdf.c)、[Sherpa 官方 KWS 文档](https://k2-fsa.github.io/sherpa/onnx/kws/index.html)、[ARM 构建脚本](https://github.com/k2-fsa/sherpa-onnx/blob/v1.12.15/.github/workflows/arm-linux-gnueabihf.yaml)及 [Open-XiaoAI KWS](https://github.com/idootop/open-xiaoai/blob/main/examples/kws/README.md)。这些公开项目不是本机原厂 KWS 的源码；本轮 PC 实验未通过真人录音召回与 ARM ABI 兼容性检查，未部署。

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

- 本轮主要证据为本机原厂ELF、运行元数据和src中的隔离实验；见reports/编码边界与机内程序-第四轮.md。
- https://ziglang.org/download/ 与 https://ziglang.org/download/index.json ：PC交叉编译工具官方来源；0.13.0 Windows包已按该JSON的SHA256核对。元数据副本temp/analysis/zig-download-index.json。

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
聚合音乐本轮来源（2026-11-01）：Music Assistant installation/music-providers/netease-cloud-music/qqmusic/player-support/dlna，https://www.navidrome.org/docs/overview/ ，https://github.com/hanxi/xiaomusic ，https://github.com/maotoumao/MusicFree 。完整链接、公开资料与机内证据区分见reports/聚合音乐环境-方案评估.md。

网易云协议参考（2026-11-01，本轮读源码未调用服务）：https://github.com/NeteaseCloudMusicApiEnhanced/api-enhanced/tree/main/module 下login_qr_key.js、login_qr_check.js、song_url_v1.js、vip_info.js。当前URL接口xeapi，不照搬旧weapi假设；详细路径见docs/NETEASE_ACCOUNT_PLAN.md。

第十六轮：以机内HTTP/原厂播放器状态和用户扫码/听音反馈为实证。eapi算法参考api-enhanced/util/crypto.js，二维码依赖Kazuhiko Arase qrcode-generator（MIT，web/QRCODE-LICENSE.txt），https://github.com/kazuhikoarase/qrcode-generator 。当前实测eapi账号与音源接口可用，不能据上游xeapi默认方式断言本机eapi不可用。
