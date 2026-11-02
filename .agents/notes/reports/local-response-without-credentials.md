# Report: Local Wake Acknowledgement Without Cloud Credentials

日期：2026-11-02。本报告记录无云凭据时的本地唤醒回应，以及有凭据时的正式语音会话验证。凭据由主代理安全写入 Mac `private/credentials.json`（0600）；本轮只在内存读取 `realtime.api_key`，经 SSH stdin 传递，不进入 argv、输出或设备普通文件。恢复材料保存在 Mac `private/deploy-recovery/r31/`，设备未创建历史部署备份目录。

**结论：** 无 Key 的 daemon 可保持本地监听；经手动触发分支验证，音箱能播放“收到”和缺凭据提示，且不建立云连接。真实唤醒现场仍存在一级候选被二级拒绝的情况；无 Key 下由真人关键词触发后的完整现场验收没有单独完成。固定 TTS 短句的完成事件也未确认，因此不能宣称该提示音完整性已验收。

## 实现变更与设备状态

- src/native_duplex_client.c：启动时读一次可选Key；没有Key也保留daemon和本地两级检测。确认唤醒后播放wake_ack，再播放missing_token提示，回待机，不执行run_session或云连接。凭据存在时保持既有唤醒后实时会话。
- deploy/start.sh：空stdin不再拒绝启动；仍通过0600 FIFO交给daemon。
- src/native_web_console.c：允许空Key启动本地语音daemon；重载时用stdin载入现有Key，音乐代码与当前磁盘源码一并编译。
- web/app.js：无Key不禁用启动按钮，并说明本地唤醒/配置提示。其余音乐交互不改。
- src/native_factory_kws.h：ready和事件立即fflush；Duplex每10秒输出读取次数、样本数、峰值与平方均值，帮助区分采集不工作和模型拒绝。只统计、不录音保存。
- make_ack：固定文本speech_text_buffer.commit生成音频，关麦事件input_audio_mute.commit；不再上传旧enroll录音。生成仅固定短句、有界连接和接收。

最终实测安装哈希：

| 文件 | SHA256 |
| --- | --- |
| /data/lx06-agent/lx06-duplex-armhf | a4553ad732769d6ecd8510caffc8d7ebb3be7d8e2be4830ffaf1872e10720f6e |
| /data/lx06-agent/lx06-web-armhf | 8c2ee543047adba9abbed7026d8c3b6038c40b5815e2f4ad0a8dcc2e93a05d8f |
| /data/lx06-agent/start.sh | 4d8d5c860ea015ab847637aa7710a42ebb100e92527a738966292a34012c1b89 |
| /data/lx06-agent/web/app.js | 7a94f18d7d4a8c5c7d721717be6ee51e2ba9a63de15e25d659682f4324c9e0a1 |
| /data/lx06-agent/wake_ack.wav | ae15539edf7ad74c0cd6bdddf7e67722a76459d7d20b048c0fc594cdde4a854a |
| /data/lx06-agent/missing_token.wav | 830b341061d0500d0f0b20feb4eb0042eca3072662683ba8fdd67874e5fcd043 |

## 实测结果与限制

初始正式daemon1660存在、采集就绪，但全缓冲日志没有及时显示factory ready。用户反馈无响应后优先加可观测性，不改阈值。新进程2149明确factory ready，活跃10秒采集约154560–160640样本；实际出现一级候选frame371、score0.097227，二级未确认、flag2超时。后续也有score28.348906等一级候选被二级拒绝。此为召回问题，不能以进程活着等同唤醒正常。

同一新进程随后实测flag1确认、wake_ack=played、HTTP101、session_ready1，执行一次music_search，接收并播放306950音频字节，written_frames153475、write_errors0、audio_done1/done1，followup超时后回待机。另一次flag1后仅呼叫未继续说话，连接ready但无speech，以no_response回待机。只证明这些事件走通，不能泛化为所有呼叫稳定可用。

固定TTS：收到704ms、11264样本，服务端返回两段音频但未观察到output_audio.done；后续两次严格结束校验未过，未把失败输出覆盖成成功。保留第一次短音频，经Mac本地SenseVoice识别为“收到”后用于本地wake_ack。协议结束未确认、真人听感未验收，仍有短句末尾完整性限制。缺Token提示3573ms、57170样本，收到output_audio.done、生成rc0；本地识别“还没有配置语音频据请先配置”（ASR同音字差异）。两音频仅固定文本生成，无现场新录音。

无Key常驻实测：空stdin start.sh 5，PID2550 key_loaded0/local_wake_only1、factory ready；5秒窗口结束后standby_window_renewed，7秒仍存活，没有cloud连接。随后强制本地回应分支（--listen-once --followup 0，非真人keyword检测）实际播放收到+缺提示，两次wake_ack=played、wake_local_ack no_key1 cloud_connected0、rc0。首次将--listen-once放末尾被现有参数遍历忽略，走普通检测并超时；该次不算回应验收。没有将该参数问题扩大到其他接口。

已恢复凭据daemon PID2762（最后启动记录）、Web PID2289，Web ready credentials_loaded1；最后正式语音日志可见云session_ready后no_speech回待机。无Key设备现场呼叫尚未单独验收，部分真人呼叫仍二级拒绝。所有唤醒后回应都从本地文件播放，不依赖每次云TTS。

编译-Wall/-Wextra/-Werror、node --check web/app.js、sh -n启动/构建脚本、git diff --check通过。原厂小爱/米家/Mesh/播放器PID仍3576/3667/1585/2440/3518/3214；原厂完整功能未重新验收。所有本轮/tmp候选/短音频/测试日志已撤回或清除。最终内存available23256KiB、/tmp9420KiB、/data可用5544KiB。

## 恢复材料与未执行的回滚验证

Mac private/deploy-recovery/r31/保存上一版Duplex（be273e…）、Web（52a954…）、start.sh（528823…）、wake_ack（e0ee13…）及旧app.js。需要回滚时停止对应daemon，经短暂staging上传并核对各恢复文件哈希，替换正式文件；删除本轮missing_token.wav，然后由受保护stdin恢复启动。成功核对后清理staging，不在设备积累revision目录。本轮未实际执行回滚验收。
