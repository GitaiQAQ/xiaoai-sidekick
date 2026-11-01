# Reference: 机内豆包唤醒、反馈与连续对话

当前为第十四轮，历史参数和旧限制见 KWS-revision13.md。源码 C11、Zig 交叉编译；待机本地判醒，不建立云端会话。

## 当前行为

“豆包豆包” → 原厂收音灯 + 本地“我在” → 连接豆包 → 收音 → 原厂思考灯 → 播报灯 → 回到收音灯等待追问。后续追问不需要再叫唤醒词，同一云端会话保留上下文。退出才撤销本客户端启用的灯效。

用户已确认本地“我在”和亮灯正常，并在真实两轮测试后反馈“全都正常”。日志见 ../logs/continuous-live-third.txt：2 次回答完成，2 次恢复追问，105535 样本全部写入，错误事件 0、播放错误 0。该确认对应 SHA81961629... 的版本；最新 SHA D46ABEA0... 只进一步收紧追问超时活动判断，尚未重做真人验证。

## 唤醒修复

原 native_kws.h 的模板 offset 以帧为单位，传给 float* 时漏乘特征维度 26。首模板正常，其他模板取到了错误的数组位置。这是实际代码缺陷，旧报告中的距离和阈值评估不能直接用于修复后的识别器。

修复地址计算，保留阈值 12.5048 和连续 3 窗，补入旧录音有效段 5，目前模板为 0/1/3/5 共 4 份。四个训练片段均匹配到自身模板，整段回放 5 次检测（包含重复计数），不是独立测试集准确率。53 条中文合成负样本 0 触发，15 秒现场短测 0 触发、最低距离 14.1956。真人持续说话、电视、远场仍需验证。用户选择稍后补录 8 次干净唤醒词。

## 采集与音乐下的判醒

采集走 `src/native_capture.h`：默认 `Capture` 8 通道，6 个麦克风取平均，Speex AEC 输出 16 kHz。音乐播放且 AEC 生效时，阈值放宽到 15.0，连续 2 个窗口命中就先暂停原厂播放器 2.2 秒，用干净音频做第 3 窗确认；未确认则恢复，候选暂停有 6 秒冷却。静音环境维持 12.5 / 3 连击；8 通道或 Speex 不可用时回退 `mico_record`，音乐下为 4 连击加候选暂停。实测线上两次唤醒在候选暂停后以 12.5082、14.6845 确认，同一实例有 2 次未确认的暂停。详见 [audio-pipeline.md](audio-pipeline.md) 与 [第二十二轮报告](../reports/采集路径回声消除接入-第二十二轮.md)。

## 会话限制

- LX_SESSION_MAX：默认 60 秒，允许 1–120 秒；限制每次云端会话，不等于服务总运行时长。
- LX_FOLLOWUP / --followup N：回答实际播完后等待追问，默认 8 秒，0 关闭连续追问，最大 30 秒。
- LX_NO_SPEECH：初次有效语音等待默认 6 秒，允许 1–30 秒；环境活动可能延长等待，最终由总上限兜底。
- 最新版回答后仅依据服务端 ASR 事件更新活动，避免风扇等环境声持续重置追问计时。该收紧已编译并通过状态测试，未再实测完整静默 8 秒退出。
- 播放时发 input_audio_mute.commit，继续读取但丢弃本地麦克风；软件队列和写入任务结束后 snd_pcm_drain/prepare，发 unmute 并恢复上行。因此目前为半双工追问，不支持说话打断播报。
- SIGTERM/SIGINT/SIGHUP 退出时发送关闭事件、停止播放并撤销自己启用的灯层。

灯效直接复用原厂 /bin/wakeup.sh 中效果 1=收音、2=思考、3=播报，不运行该脚本的其他播放器/会话操作。新灯先显示再撤销旧灯，避免暗档。与小爱同时唤醒时共享灯层，仲裁与音乐/闹钟完整回归尚未完成。

## 构建与验证

```powershell
.\tools\Build-NativeGlibc.ps1 -Duplex
.\tools\Run-NativeProbe.ps1 -Kind feedback-test
.\tools\Run-NativeProbe.ps1 -Kind kws-score
.\tools\Run-NativeProbe.ps1 -Kind kws-live -Seconds 15
.\tools\Run-NativeProbe.ps1 -Kind kws-signal -Seconds 5
.\vendor\python-analysis\python.exe tools\Check-KwsReplay.py
```

Check-KwsReplay.py 使用 build/host-kws-r14.exe，构建命令见第十四轮报告；验证模板偏移和现有 53 份负样本。会话状态测试为 src/native_session_policy_test.c，共 21 个检查。

## 安装与运行

```powershell
.\tools\Manage-LX06Service.ps1               # 更新 /data/lx06-agent/ 自有文件
.\tools\Manage-LX06Service.ps1 -Start -Standby 60
.\tools\Manage-LX06Service.ps1 -Status
.\tools\Manage-LX06Service.ps1 -Stop
```

Status/Stop 已修正为不上传、不改目录。安装先验临时文件 SHA，再替换目标。Key 从 private/credentials.json 经 stdin/FIFO 进入内存，不写普通文件、不进参数。脚本需 LF 行尾。无开机自启；当前待机窗口超时仍会退出服务，尚不是无限常驻。

本轮测试完成后服务已停止，避免测试继续计费。部署范围和旧版本撤回见 DEPLOY_REVISION14.md，不涉及刷固件。
