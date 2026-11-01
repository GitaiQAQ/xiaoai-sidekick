# Reference: 第十四轮部署范围与撤回

用户要求修复低唤醒率、连续对话、亮灯与“我在”。本次只更新 /data/lx06-agent/ 下自有客户端和 start.sh/stop.sh/status.sh；不刷固件、不改原厂配置、不开机自启、不停止原厂进程。

部署前确认客户端已停，检查目标文件 SHA。主机旧程序与相关源码保存在 build/revision14-original/（旧程序 SHA256 45f2f3b50b1e63b86f059bb434154ad7cb350607b7c6abf34c4d517ab9104510）。撤回：先运行已修正为不上传的 Manage-LX06Service.ps1 -Stop；将旧二进制和 start.sh 恢复到 build/lx06-duplex-armhf、deploy/start.sh，再运行安装脚本；不启动即可保持原厂服务运行。该撤回只针对自有程序，不能视为固件恢复证明。

原厂 wakeup.sh 映射效果 1/2/3 为收音/思考/播报。会话切换先显示新灯效，再撤销旧灯效，避免出现暗档；退出只撤销本客户端记录的灯效，不调用 shut_all。最初独立短测用 L=8 静态蓝色，用户确认声音和灯环正常；后续改为原厂三态灯效，不能把前次反馈当作三态用户确认。与同时发生的小爱会话共享灯层，仲裁仍须回归。

原厂录音 /usr/share/sound/wakeup_wozai.wav 为 mono S16 16000Hz、5538 样本，直接 ALSA 播放，无 TTS 请求。会话默认 60 秒总上限、初次无语音 6 秒退出；回答播完等待追问 8 秒。同一 WebSocket/会话保存上下文；播放时发送 mute 并丢弃本地采样，播放完成才 unmute。暂不支持说话打断播报。任何真人云端测试必须有界，测试后停服务并检查连接。

第一轮实测发现 snd_pcm_delay 不能可靠判断原厂 default PCM 的播放完成：全部写入 37872 样本，但始终未切回收音。已改用软件队列为空、无写入任务后调用 snd_pcm_drain/prepare 作为完成屏障。仍需真人两轮验证，不提前宣称通过。

暂停/恢复事件已通过火山引擎官方搜索正文重新核对：
https://docs.volcengine.com/docs/doubao-voice/end-to-end-real-time-voice-full-duplex-version?lang=zh

不把模拟状态测试、ALSA 写入完成或灯接口返回当成连续对话/全部原厂功能回归通过。
