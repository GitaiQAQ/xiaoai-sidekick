# Report: Wake Feedback, Indicator Lights, and Follow-Up Dialogue

本报告记录旧版 MFCC/DTW 唤醒实现的一次修复与现场会话验证。后续小修正没有真人复测；本文中的通过结果只适用于明确列出的程序版本和测试条件。

## 结论与范围

用户要求提高唤醒率、补齐连续对话，以及原厂一样的亮灯与“我在”。本轮已修改并部署机内程序，没有改写固件或停止原厂语音服务。用户确认本地声音/灯正常，随后真实连续对话和三态灯效反馈“全都正常”。测试结束停止自有服务。

## 实测结果

1. 修复模板索引单位错误：lx_kws_template_off 是帧偏移，必须乘 LX_KWS_DIM=26 再加到 float 数组。旧代码只有首模板地址正确。保留阈值12.5048及连续3窗，模板补充为旧录音0/1/3/5。主机和设备四份模板均匹配自身（距离0.0001），整段训练录音回放5次触发；不宣称真实召回率。53份合成负样本0触发；现场15秒67窗0触发，最低距离14.1956。logs/kws-revision14-enrollment.txt、kws-revision14-negatives.txt、native-kws-live-runtime.txt。
2. 固件 /bin/wakeup.sh 明确效果1/2/3对应收音/思考/播报。直接通过led ubus切换，只撤销自己记录的效果；会话未结束时持续保留灯。原厂 wakeup_wozai.wav 为16k单声道S16、5538样本，ALSA本地播放，无需云端TTS。logs/native-feedback-test-runtime.txt完整演示三态。
3. 连续追问使用同一个WebSocket和云端会话；回答时mute、播放后unmute，等待8秒追问，单会话硬上限60秒。第一轮错误使用snd_pcm_delay判断完成，全部37872样本写出仍未恢复收音；已及时停止并保存logs/continuous-live-first.txt。改为软件队列空/无写入任务后drain+prepare。第三轮实测2次回答完成、2次followup、1次session.created，105535样本全部写出、error=0、write_errors=0，见continuous-live-third.txt。用户明确确认正常。
4. 退出打印session_stop=signal turns=2、wake_led=off；原厂PID1282/2527不变，Mesh支持/启用/运行true。无自有进程和连接遗留。没有证明所有原厂功能回归通过。

## 最后一次修正的验证边界

真实两轮验证对应程序81961629F5913E8C2B86E4E7527C4371E7942704ABEB8D91304C8B5BDB754024。观察发现环境能量会一直重置追问计时，最后改为回答后的活动只依赖云端ASR事件；未重新做真人两轮或静默8秒验证。最新程序D46ABEA0B35B907E74044A184A2E4258EE70271C69CE2E91F8952694A5FB0D5D已构建、ELF校验、状态测试通过并安装，保持停止。不要将最后小修正写成真人验证已通过。

## 涉及的代码与回归记录

主要修改：`src/native_duplex_client.c`、`native_kws.h`、`kws_templates.h`；新增 `native_session_policy.h/test.c`、`tools/Check-KwsReplay.py`。工具脚本修正 Status/Stop 不再隐式安装、上传 SHA 先验后替换、FIFO 说明和 LF 行尾。历史构建副本当时保存在 `build/revision14-original`；这是本地历史材料，不是当前部署备份规则。当前设备恢复材料必须保存在 Mac `private/deploy-recovery/`。当时的回滚步骤见部署参考文档。

```powershell
.\tools\Build-NativeGlibc.ps1 -Duplex
.\vendor\zig-portable\zig-windows-x86_64-0.13.0\zig.exe cc -target x86_64-windows-gnu -std=c11 -Wall -Wextra -Werror -O2 -ffp-contract=off -Isrc -o build/host-kws-r14.exe build/host_kws_main.c
.\vendor\python-analysis\python.exe tools/Check-KwsReplay.py
.\vendor\zig-portable\zig-windows-x86_64-0.13.0\zig.exe cc -target x86_64-windows-gnu -std=c11 -Wall -Wextra -Werror -O2 src/native_session_policy_test.c -o build/session-policy-test.exe
.\build\session-policy-test.exe
```

会话策略21项检查包含重复done、两轮恢复、播放未完不计时、8秒计时、单轮模式。不能替代真实网络与声学测试。

## 未完成事项

补录8次干净唤醒词（用户选稍后）；真人持续交谈/电视误唤醒验证；最新版静默8秒自动关闭实测；小爱和豆包同时唤醒灯层仲裁；音乐、闹钟、米家完整回归；常驻待机窗口超时退出、重启后Key和自启动策略。未实现语音打断，不应宣传全双工。

依据主要为原厂固件与本机日志。暂停/恢复语义再次核对官方：https://docs.volcengine.com/docs/doubao-voice/end-to-end-real-time-voice-full-duplex-version?lang=zh （直接抓取超时，官方搜索正文可读）。
