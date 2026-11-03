# Report: Wake group label supervision

2026-11-03。本轮只在Mac审计已有文本、组级训练和离线验证。没有设备访问、ARM回放、部署、新录音或云音频上传。完成四个3-epoch候选，仍未达到35/35。所有新指标都是Mac辅助，不是Paddy flag1或独立泛化100%。

## 文本证据：没有证明半词误标

人工pos保持权威，ASR只作支持，不自动改labels.csv或把未知词改成豆包。去标点后只计精确豆包；同音异字、噪声和ASR遗漏均归不明确。

| 集合 | 双ASR完整两遍支持 | 单ASR完整两遍支持 | 可靠半词支持 | 不明确 |
|---|---:|---:|---:|---:|
| S3 58片段 | 6 | 7 | 0 | 45 |
| S1最短20源 | 0 | 1 | 0 | 19 |

S1不明确19中，有1源仅一份ASR出现一次豆包，另一份没有；不能据此断定音频为半词。S3 45个无精确词也不能否定人工pos。因此此前半词被强制完整的假设未获可靠文本证明。

具体64个不明确源及start/end在temp/analysis/wake-group-labels/semantic-review.csv，既有文本支持字段在semantics.json。review列为空待人工核对，不是已修正标签。tools/wake-group-semantics.py可重算，不造转写。

## 组构造、零音和BG5

严格按原gap<0.5分组，不改35组/匹配窗。S1 89pos变60组，其中18多片段，20短路径源只有10个被融合。S3 58pos变35组，其中6多片段（18/27/28/29/34/35），均属于旧ARM已确认7组。因此融合不是原18个无L1单片段组的直接修复对象。

组音频只复制已标pos区间；未标原音频不复制，间隙填等长合成零音：S1 5.96秒、S3 4.36秒。增强再加入既有train背景（S1安静残余/S2音乐），不使用S3未标gap或S2b作梯度。group-manifest.json记录member区间和zero holes。完整八状态组监督仍是假设，不是人工音素金标准，也不证明每片段纯完整两遍。

BG5是完整八状态之后再次重复的背景要求，不是两个豆包之间的词内停顿。本轮不改变推理图或放宽八状态内部停顿。BG5组全部重复前至少5帧；adaptive-BG0仅时标多片段或双ASR支持至少4个精确豆包的组允许直接完整重复/任意BG。实际资格24组（S1 18/S3 6），其余71组保持BG5。

每状态4至40帧、包络余量5、全零400样本回BG不变；低于-85dBFS只统计不直接删KW。BG0实际改动L1的2/24组84帧，L2的5/24组239帧；S3仅第27组有变化。原18个L1漏检组没有BG0标签改动。本次范围没有BG0召回收益，不能推广成所有快速连说都不受BG5影响。

## 固定训练与结果

两级各BG5/BG0三轮，从同v5/l2原基线开始。S16、seed3503增强/3504批次、Adam1e-4、hinge1、clipnorm1、逻辑batch16/micro8。微批按有效/BG帧加权，合成loss差2.38e-7、最大梯度差4.05e-6。

按旧配置重生成并缓存TTS/负例/背景，旧strong样例数/帧数可重现；旧完整feature未保存，不声称与过去原数组逐字节核证。组正例改变padding、上下文和随机序号对应，strong对比不是完全单变量。BG5/BG0之间整个MFCC feature SHA相同，主对照只改变监督重复规则。

所有指标同S16、相同dev身份/顺序/seed7、margin<0、gap100帧聚合、中点原[start-0.3,end+0.5]，不扩大窗、不把多事件充句。一级FST2.25/二级4；另只比较固定已部署二级3参考。S3已是训练/拟合集；S2b不梯度但作调参否决，S2是训练背景。

| 模型 | 句命中 | 音乐S2b/S2 | devPos | devHard | devNeg | 最长s |
|---|---|---|---|---|---|---|
| l1-bg5 | 18/35 | 0/0 | 26/38 | 27/191 | 1/70 | 5.93 |
| l1-adaptive-bg0 | 18/35 | 0/0 | 26/38 | 27/191 | 1/70 | 5.93 |
| l2-bg5 | 9/35 | 0/0 | 13/38 | 3/191 | 0/70 | 9.40 |
| l2-adaptive-bg0 | 9/35 | 0/0 | 13/38 | 4/191 | 0/70 | 9.45 |

旧strong原图L1 22/35、near34/191、最长13.78s；L2 6/35、near5/191、最长9.43s，两级音乐0。本轮L1更低18/35但事件更短/near更少，L2更高9/35。不是35/35，也不能把差异唯一归因语义融合。

| L2固定cost3参考 | 句命中 | 音乐S2b/S2 | devPos | devHard |
|---|---|---|---|---|
| bg5 | 13/35 | 0/0 | 23/38 | 10/191 |
| adaptive-bg0 | 13/35 | 0/0 | 23/38 | 10/191 |

旧strong L2 cost3同为13/35，但音乐1/0、near11/191；组级L2 cost3音乐0/0、near10/191，是有限Mac负例上的改进。BG0原图near还多1次，优先保留简单BG5作后续对照。

## 原35组矩阵

旧ARM只来自历史r34。S1/S2是旧strong；G1/G2是组级BG5原图；R2是组级BG5二级3参考。完整四候选matrix.csv保留。

| 组 | start-end s | 旧ARM | S1 | S2 cost3 | G1 | G2 cost4 | R2 cost3 |
|---|---|---|---|---|---|---|---|
| 1 | 47.59-49.43 | - | yes | - | yes | - | - |
| 2 | 53.83-54.99 | - | yes | - | yes | - | - |
| 3 | 57.95-59.15 | - | yes | - | yes | - | - |
| 4 | 62.13-63.19 | - | - | - | - | - | - |
| 5 | 64.79-65.93 | - | - | - | - | - | - |
| 6 | 67.85-68.95 | - | yes | - | yes | - | - |
| 7 | 70.61-72.03 | - | yes | yes | yes | yes | yes |
| 8 | 73.55-74.77 | - | yes | - | - | - | - |
| 9 | 78.81-79.77 | - | - | - | - | - | - |
| 10 | 83.31-84.25 | - | yes | - | - | - | - |
| 11 | 86.53-87.79 | - | - | yes | - | yes | yes |
| 12 | 91.47-92.51 | - | - | - | - | - | - |
| 13 | 96.15-97.63 | - | yes | - | - | - | - |
| 14 | 98.67-99.31 | - | - | - | - | - | - |
| 15 | 100.95-102.77 | yes | yes | yes | yes | yes | yes |
| 16 | 103.95-105.89 | - | - | - | - | - | - |
| 17 | 108.01-110.53 | - | - | - | - | - | - |
| 18 | 111.27-127.35 | yes | yes | yes | yes | yes | yes |
| 19 | 128.03-129.25 | - | - | yes | - | - | yes |
| 20 | 129.79-131.25 | - | - | - | - | - | - |
| 21 | 132.49-133.77 | - | yes | - | yes | - | - |
| 22 | 134.37-136.17 | - | yes | - | yes | - | - |
| 23 | 137.47-139.01 | - | yes | - | yes | - | - |
| 24 | 139.59-141.87 | - | - | - | - | - | - |
| 25 | 142.77-151.85 | - | yes | - | - | - | - |
| 26 | 152.65-154.81 | - | - | - | - | - | - |
| 27 | 155.31-177.09 | yes | yes | yes | yes | yes | yes |
| 28 | 179.03-183.29 | yes | yes | yes | yes | yes | yes |
| 29 | 183.89-187.59 | yes | yes | yes | yes | yes | yes |
| 30 | 190.33-192.79 | - | yes | yes | yes | - | yes |
| 31 | 194.13-195.89 | - | yes | yes | yes | - | yes |
| 32 | 197.23-199.97 | - | yes | yes | yes | - | yes |
| 33 | 200.99-202.35 | - | - | - | - | - | - |
| 34 | 208.15-229.27 | yes | yes | yes | yes | yes | yes |
| 35 | 229.87-239.77 | yes | yes | yes | yes | yes | yes |

## 候选与下一步

未覆盖旧10候选。四新AM的完整模型在temp/analysis/wake-group-train：

- l1-bg5/model.binn: SHA256 c7b579e50d51a723b9adf1da0d71551bac89bcfde56e1510debd730d189b5f18; 639597 B; export error 8.58e-06.
- l1-adaptive-bg0/model.binn: SHA256 306927d6bb51023d664d2166231cd9c2aa27fda5f76a939749ade96ba4d1884b; 639597 B; export error 6.20e-06.
- l2-bg5/model.binn: SHA256 735f4d2fe76ea8d7b2e0314245573e96b180cffb0806ab570b51a2710fe8089a; 1678701 B; export error 5.25e-06.
- l2-adaptive-bg0/model.binn: SHA256 183869eb70a7ad8ce5f3ada2e24e9f6be8bcfa5408aa22a1875d9beb4f0e9039; 1678701 B; export error 5.25e-06.

Mac后续审查pair保留旧strong L1、采用新group BG5 L2；r34 conf/FST/prior/TransitionModel逐字节保持，只换AM。今晚不进行ARM或部署，不称Paddy通过。

- L1: temp/analysis/wake-group-labels/review-controls-r34/l1.binn; SHA256 3f065dbdafe1021a94db8afca0e7637f588d870a8de137b3da36a3f907492c93.
- L2: temp/analysis/wake-group-labels/review-controls-r34/l2.binn; SHA256 2a7188d058bfaf5fa655269d9ca9081e690f726acdff534c81e6d9a7245b9c19.

混合pair Mac同组交集11/35（不是ARM确认，也不能推断真实上下界）；dev双最小值正例20/38、near9/191、普通0/70。

现有文本不足以可靠修正64个不明确源。优先核对完整/半词/含词杂句语义，不自动造转写。继续本地训练更值得先量化KW目标帧正确类和BG解码竞争、完整路径余量，再考虑解码一致的正例路径目标；只压BG会压弱正例，本次BG0不是主要突破。八状态内部暂停与完整关键词重复后的BG5应分开。

## 复现和检查

tools/wake-group-semantics.py审计，tools/wake-group-train.py --epochs 3生成四候选。source role、冻结非AM字段、BINN字节往返、固定feature SHA、旧样例/帧数、微批梯度及py_compile均核对。后处理temp/analysis/wake-group-labels/report.py保存summary.json/matrix.csv/Mac review pair；label差在bg-gap-label-delta.json。

原labels.csv、原35组、旧候选与部署源码未改。旧边界报告只修Mac交集措辞，不写真实上下界。
