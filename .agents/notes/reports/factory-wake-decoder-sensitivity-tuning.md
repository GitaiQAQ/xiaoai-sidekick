# Report: Factory Wake Decoder Sensitivity Tuning (Round 34)

2026-11-02。用户反馈当前“大概率不理、完全没法用”，授权调得更容易。本轮不把第三十一轮少数成功视为可用；只用既有真人/AEC与纯音乐PCM经SSH stdin流式重放，不新录音、不做无界云会话。context/operations由主代理更新。

## 真正的控制项

静态证据（1.94.13 libxaudio_engine.so）：RiceLoadModelBin @0x59990–0x5999c解析 `--score-threshold` 到state+0x3e8、默认1；@0x59b3c解析min-self-loop到+0x73c，@0x59b5c解析max到+0x740；wakeup-wait-frame在+0x364。IsKeyword @0x57d90检查最终token关键词字段，RiceTdnnProcPriorsSoilViterbi @0x576fc–0x57728只将网络输出减prior。ProcessEmittingViterbi @0x57a54–0x57a5c在音素前进时要求循环计数严格大于min_self_loop；@0x57b8c检查max_self_loop。原厂Paddy候选超时 @0x55074–0x550c0比较等待帧并输出flag2，flag1仍要求二级关键词成立。

实测自有两个state：score_threshold1、min_loop1、max_loop80、wait60。把二级score-threshold改0、probe读回0，s3事件帧/分数与基线一致，仍12事件/8确认。不能把这个字段当作Rice日志score的直接阈值旋钮；尤其原先score0.097/0.204已经可以候选/确认。numpy参考前向的margin是关键词与背景路径累计成本差，也不能直接当该字段或外层输出double。当前外层+0x230/+0x238仍为0，无判醒分数含义。

一级FST八个关键词前进边成本2.25；二级为4.0，二级比一级严格。降低这些边的成本可减轻路径罚分；仍保留8音素序列、完整两级、循环/时序门槛。另一个有效项是min_self_loop；1改0仍要求实际自循环计数>0，没有绕过完整音素序列。没有对生产进程写未知偏移：只修改已核对的BINN conf/FST字段。

新增 tools/wake-tune-container.py，可配置 --score-threshold、--keyword-step-weight、--min-self-loop、--wait-frames。FST重写前先要求提供的decoded JSON逐字节还原原FST，识别8条非自循环关键词边；保留am/prior/TransitionModel字节。模型候选、日志、固定TTS组合均在temp/analysis/wake-tune-r34/（忽略）。src/native_wake_model_probe.c新增已核对控制标量读回，最终probe SHA256 6ac4abe0edaed0f3215d3acdabbebcfbb542551368da2d62d7f76de8cc808393。

## 有界ARM对比

每次只在/tmp放一份一级和二级候选及小probe，PCM流式；为了不再次OOM，测试期间仅短暂停Duplex，finally用已授权Key经stdin恢复，不改原厂进程/分区/模型。每次45秒BusyBox timeout界限，进程结束释放资源。

s3为已有240秒音乐下真人录音。人工labels.csv定义58个pos片段；事件使用返回a1/a2的时间中点，以片段start−0.3至end+0.5匹配。该口径用于本轮所有配置，不把事件次数当准确率；重复确认可以落在同一片段。聚合句口径为间隔严格小于0.5秒合并，35组。

| 配置 | 原厂确认次数 | 命中人工片段/58 | 标注外确认 |
| --- | ---: | ---: | ---: |
| 原版，loop1，FST一级2.25/二级4 | 8 | 5 | 0 |
| 仅二级score-threshold0 | 8 | 5 | 0 |
| 二级前进边3 | 9 | 6 | 0 |
| 二级前进边2.25 | 10 | 6 | 0 |
| 二级前进边2 | 10 | 6 | 0 |
| 一级1.75/二级2，loop1 | 14 | 9 | 0 |
| 一级1/二级2，loop1 | 17 | 10 | 0 |
| 一级1/二级2，loop0 | 49 | 25 | 0 |

loop1的最易罚分档（一级1/二级2）在s2b180秒及s2300秒纯音乐均0候选/确认；分别11165/18570ms，RSS9568/9600KiB。神经权重没有改。

另取已有TTS corpus，固定seed34选10正例、20相近词、20普通句，以各1秒零音分隔生成内容132.42秒、另补1秒尾静音的本机fixture（实际输入133.42秒），输入不落设备。以同一事件区间口径匹配：

| 配置 | 正例命中/10 | 相近词误接受/20 | 普通句误接受/20 |
| --- | ---: | ---: | ---: |
| 原版 | 9 | 1 | 0 |
| 一级1.75/二级2，loop1 | 9 | 2 | 0 |
| 一级1/二级2，loop1 | 9 | 2 | 0 |
| 一级1/二级2，loop0 | 10 | 6 | 0 |

拒绝把最后过松组合直接部署：虽然真人确认明显增多，相近词误接受也显著增多。继续撤回额外FST放松、只比较原罚分+loop0，寻找较温和的有效修改。TTS小样本并非整体泛化准确率，纯音乐有限负例也不保证将来零误触发。

## 恢复准备

已将当前两份正式模型只读收回Mac private/deploy-recovery/r34/：一级de899046087da8729a245764ef853ba10884b555e9599d01dbb592995476610f，二级5945e591204a6ce2dedbab1c8bd18197e5e9fb49b1ba9bf4b0f88dd2be00464f。后续若安装，只替换本轮通过的模型，正式Duplex/Web/启动及本地回应文件不变。恢复用Mac两文件经短暂staging上传校验后替换，启动时仍用受保护stdin提供Key；设备不保留历史部署备份。

## Follow-up candidate

撤回过强罚分、仅 `min_loop=0` 并保留原 FST：s3 确认 14 次、命中 8/58 个人工标注片段、合并句 6/35，标注外确认 0；TTS 样本命中 10/10 正例、误接受 1/20 相近词、0/20 普通句。该候选较保守，但仍容易漏检。

较平衡配置保留一级 FST 前进边成本 2.25，将二级成本从 4 降至 3，并将两级 `min_loop` 设为 0；`score_threshold=1`、`max_loop=80`、`wait_frames=60` 不变。实测 s3 确认 22 次、命中 13/58 个人工标注片段、合并句 7/35，标注外确认 0；TTS 样本命中 10/10 正例、误接受 3/20 相近词、0/20 普通句。相比原版的 5/58 片段、3/35 合并句，召回有所改善，但仍不足以证明可用。

候选模型只修改 FST/配置，没有修改声学网络：

- 一级候选 `temp/analysis/wake-tune-r34/l1-basemin0.binn`，639,597 bytes，SHA-256 `471dfb5ea0e8977e53550ad3ab392beb3b685609a08e51ddf84a8b5ccada9ba3`。
- 二级候选 `temp/analysis/wake-tune-r34/l2-min0-step3.binn`，1,678,701 bytes，SHA-256 `96917162f1ec9bb589f098cc19283f676d2e73f777adbe37f7f55f3c9600c92c`。
- 一级、二级声学网络 SHA-256 分别为 `4160020f3e14d2412d313340657befad8fa4325d803597139ed5752a65a57fec` 和 `cbd5264fe8787fb92e3848a254603d671460467dc9d3092cf40f46ae1647736a`，与 v5/l2 原网络一致。

部署前置条件是候选通过 s2b 180 秒和 s2 300 秒纯音乐测试；任何非零确认都取消安装。通过后仅替换设备的两份模型，核对哈希，再用正式程序回放 s3。恢复材料保存在 Mac 的 `private/deploy-recovery/r34/`，不在设备保留历史副本。该候选尚需用户现场复核，不能称为误唤醒问题已修复。

## Final device installation

后续实测中，平衡档通过了纯音乐回放：s2b 180 秒（18,000 帧，11,200 ms）和 s2 300 秒（30,000 帧，18,663 ms）均为 `flags=0`、`confirmed=0`、`rc=0`，峰值 RSS 分别为 9,564 KiB 和 9,580 KiB。随后按既有授权将两份模型安装到设备，读回 SHA-256 分别匹配 `471dfb5e…ada9ba3` 和 `96917162…600c92c`。正式 Duplex 使用默认模型路径重放 s3，得到 24,000 帧、22 次确认、`input_error=0`、`rc=0`，与隔离探针结果一致。

带凭据恢复启动后，语音进程报告 `key_loaded=1`、两级 `factory_kws_ready` 和 `stage=standby`，待机预算为 3,600 秒。Web 未改；当次只读观察到原厂、米家、Mesh 和播放器进程仍在。本轮采集 10 秒，采样数约 159,040–160,640，记录的安静峰值为 23–40（原记录未标单位）。本轮没有完成新的真人关键词现场验收，也没有测试云端会话，因此不能称用户已确认可用或保证完整原厂功能。进程号仅是当次观察值，不代表当前进程状态。

本轮只改变 Sidekick 自有模型的配置/FST；Duplex 二进制 SHA-256 `a4553ad732769d6ecd8510caffc8d7ebb3be7d8e2be4830ffaf1872e10720f6e` 未变，原厂库与原厂模型未改。临时目录 `/tmp/wake-tune-r34` 已清理，`MemAvailable` 为 23,944 KiB、`/data` 可用 5,532 KiB、`/tmp` 已用 9,888 KiB（可用 49,492 KiB）。与原版相比，人工标注片段命中由 5/58 增至 13/58，合并句由 3/35 增至 7/35；TTS 小样本相近词误接受由 1/20 增至 3/20。绝对召回仍低，且小样本显示误接受上升。现场关键词效果和完整功能仍待用户验证。


## ARM 与此前 macOS 口径的区别

此前 macOS v5 的 27/58、17/35 是**一级 numpy 参考前向**：MFCC→网络→FST，取关键词路径与背景路径成本差 margin<0 的帧，将间隔不超过100帧的命中合成事件；它没有这次原厂Paddy的二级确认、循环计数门槛、两级缓存和等待超时条件。本轮ARM的8确认→5/58、3/35是**v5一级加l2-d160二级的原厂连续推理**，只有外层flag1算确认，flag2超时不算。因此二者不是同一准确率指标；不能把27→5全部归因于ARM平台或前端差异。原厂前端是否与numpy完全一致仍未实测对齐，属于另一个未验证项。

ARM离线探针每次送160个S16样本（10ms），t0对应录音原始首样本；没有额外前滚、ACK、100帧零音冲刷或设备实时等待。日志frame是事件返回时刻，a1/a2是解码器返回的边界；匹配用t=(a1+a2)/200秒，**不用事件返回frame/100**，以免把二级确认/60帧超时延迟当成发言时刻。该映射按10ms帧计数推定，不是逐条强制对齐后的字级边界，容差沿用start−0.3/end+0.5秒。没有在生产现场帧计数与旧录音之间做匹配。

58个正例区间来自人工labels.csv的session/start_s精确键，未标不回退ASR。35组合由这些区间按相邻间隔严格小于0.5秒合并；边界正好0.5秒另成一组。每个区间/组只要有一个确认中点落入容差就记一次命中；多个确认可落在同一组，所以22确认不等于22个独立句子，也不等于22/58。

可重算方法：从对应probe日志提取flag1的a1/a2，按上述中点与区间容差匹配，分别统计命中区间数、命中合并组数、没有落入任何人工正例容差的确认数。基线日志temp/analysis/wake-probe/v5-l2-s3-stream.log；最终temp/analysis/wake-tune-r34/balanced-s3.log。TTS固定列表在temp/analysis/wake-tune-r34/tts50.json，与其同目录日志匹配，不使用用户转写内容推断标签。

## 重现与恢复命令

本机生成最终候选（不改原始训练权重目录）：

```sh
vendor/py/bin/python tools/wake-tune-container.py temp/analysis/wake-train/d96-v5/model.binn temp/analysis/wake-tune-r34/l1-basemin0.binn --min-self-loop 0
vendor/py/bin/python tools/wake-tune-container.py temp/analysis/wake-train/l2-d160/model.binn temp/analysis/wake-tune-r34/l2-basemin0.binn --min-self-loop 0
vendor/py/bin/python tools/wake-tune-container.py temp/analysis/wake-tune-r34/l2-basemin0.binn temp/analysis/wake-tune-r34/l2-min0-step3.binn --fst-json temp/analysis/wake-train/l2-d160/fst-decoded.json --keyword-step-weight 3
sh tools/build-wake-model-probe.sh
```

设备测试需先暂停仅Duplex以避免双引擎内存压力，上传小probe与候选至短暂/tmp路径；经已配置的安全SSH认证运行以下远端命令，并将Mac既有PCM作为SSH stdin，不上传整段PCM文件：

```sh
timeout -t 45 /lib/ld-linux-armhf.so.3 /tmp/wake-tune-r34/probe --model1 /tmp/wake-tune-r34/l1.binn --model2 /tmp/wake-tune-r34/l2.binn --pcm16le -
```

恢复步骤：停止Duplex；将Mac private/deploy-recovery/r34/的两个原模型上传小staging；先分别核对de899046…和5945e591…，再覆盖/data/lx06-agent/models/d96-v5.binn、l2-d160.binn并chmod0600/sync；核对正式文件完整SHA256；通过当前受保护stdin流程启动start.sh 3600，确认key_loaded/factory ready和采集；删除staging。不要在设备留下历史模型或revision备份。此恢复只保留了可核对文件与步骤，本轮没有真的回滚验收。
