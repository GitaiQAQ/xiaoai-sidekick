# Report: MFCC-DTW Wake-Word Prototype and Device Evaluation

本报告记录早期 MFCC + dynamic time warping（DTW）模板检测器在 LX06 上的实现与测试。它是历史实验，不代表当前唤醒实现或推荐配置。

**结论：** 该原型可在注册录音回放中判醒，也能通过真实 Key 建立一次语音会话；但持续有人说话时 20 秒内出现 8 次误唤醒。注册样本与其他连续语音的距离分布重叠，因此调阈值不能同时保证召回和误触发率。本轮数据只覆盖一台设备和有限时长，不能据此估计一般误唤醒率。

## 检测器设计与结论

- 已实现纯 C、无新增依赖（仅 libc/libdl/libpthread，和原客户端一致）的离线唤醒词检测，术语`豆包豆包`，
  唤醒前不联网、不建立会话、不播放。
- 特征：25 ms/400 点帧、10 ms 步长、Hamming 窗、自写迭代 radix-2 512 点 FFT、24 个 mel 滤波器（0–8000 Hz）、
  log、DCT-II 得 13 维，加一阶差分共 26 维；距离为欧氏，DTW 用 Sakoe-Chiba 20 % 带宽，按路径帧数归一化。
- 模板由 `/tmp/doubao_enroll.wav` 的 8 段录音离线生成，3 个模板写进 `src/kws_templates.h`（C 数组，运行时不读文件）。
- **实测**（设备上 `--kws-score`）：8 段中剔除 300 ms 和 3180 ms 两段；剩余 6 段两两 DTW 选中 0/1/3 三段
  （780/640/850 ms）作模板，簇内最大距离 11.3680，阈值 = 1.3 × 11.3680 = **14.7784**；
  4 段"豆包豆包"取在阈值之下（0.0001 / 8.2515 / 11.3680 / 14.0157），
  530 ms 弱段 16.2833 与并成一段的 2290 ms 段（长度门限直接拒绝）在阈值之上。
- **实测**：整段 22 s 滑窗扫描得到 5 次唤醒，全部落在说话处；录音尾部 4 s 噪声窗最低 16.8997，不误触发。
- **实测（安静期）**：现场 30 s 待机（`--kws-live`）142 窗，仅 2 窗进入打分，最低 18.9865，0 次误唤醒；
  15 s 现场采样（`--kws-capture`）67 窗、16 窗打分，最低 14.4438，0 次判醒。
- **实测（有人说话时）**：另一次 20 s 现场运行里房间持续有人声，38 个窗进入打分、最低 10.5620，
  连续两窗低于阈值出现 8 次 → **会误唤醒**。这是本轮最重要的负面结果，见第 6 节。
- **实测**：`--kws-wav` 回放链路在 1854 ms 判醒，前滚 4800 采样（300 ms）就绪；无 Key 时停在
  `wake_detected no_key_stdin detection_only=1`，未联网。
- **实测**：`--kws` 待机 12 s 的运行里，房间在约 5 s 处有说话声被判醒（dist 13.51/14.20），因 stdin 无 Key 立即停在 `detection_only`，未联网；`--kws` 的每一轮"待机→会话→回到待机"在同一进程内循环（会话结束打印 `stage=standby_resumed`）。

## 实现文件

| 文件 | 作用 |
| --- | --- |
| `src/native_kws.h` | 头文件式实现：无 libm 的 cos/log/sqrt、MFCC、DTW、端点裁剪、滑窗检测器、WAV 打分 |
| `src/kws_templates.h` | 生成物：3 个模板的 26 维特征、长度、偏移、阈值（`LX_KWS_TEMPLATE_*`） |
| `src/native_duplex_client.c` | 在原有实时客户端上增加 `--kws-score/--kws-live/--kws-capture/--kws-wav/--kws` 模式 |
| `tools/kws-build-templates.py` | 从设备 dump 生成 `src/kws_templates.h` |
| `tools/Run-NativeProbe.sh` | 新增 kind：`kws-score`、`kws-live`、`kws-capture`、`kws-replay` |
| `tools/Build-NativeGlibc.sh` | 增加 `-ffp-contract=off`，保证主机与设备打分结果逐位可比 |

不引入 libm：`sin/cos/log/sqrt` 自行实现（host 上与 libm 对比：cos 误差 3.4e-6、log 1.1e-6、sqrt 相对 1.2e-7）。
MFCC 逐帧结果与独立 Python 直接 DFT 参考实现一致到 1e-4（见 `build/check_mfcc.py`，开发期工具）。

### 检测参数（`src/native_kws.h`）

- 帧 400 点 / 跳 160 点；FFT 512；mel 24（0–8000 Hz）；倒谱 13 + 差分 13。
- 端点裁剪：滑动窗内按 dB 峰值 −15 dB、且不低于窗内底噪 +6 dB 取连通段，允许 150 ms 的音节间隙合并。
- 噪声门：窗内 dB 的 10 % 分位作为运行底噪（快速下降、约 4 s 慢升），峰值需高出底噪 12 dB（或峰值 ≥38 dB）。
- 长度门：跨度与模板长度相差超过 ±25 % 的模板直接跳过。
- DTW：`band = 0.2 × 模板帧数 + 1.5`，代价 `D/(na+nb)`。
- 判定：连续 2 个窗（步长 0.2 s）低于阈值即唤醒；唤醒时携带最近 300 ms 前滚音频。

距离实际使用的维度经过实测调整：不去均值（CMN 关闭）以避免把绝对谱形抹平；c0（帧能量）只保留
"去均值后的包络 ×0.25"，既保留四音节节奏又不让能量差主导距离。这一组参数是在设备现场 45 s 待机出现
15 次误唤醒后改出来的（见第 6 节），属于实测结论而非理论推导。

## 模板生成流程

1. 设备 `--kws-score --dump`（实际命令 `--kws-wav`/`--kws-score` 的 dump 变体由主机侧同一份代码产生）：
   对 8 段计算特征并打印 `dump segment=<i> frames=<n>` 与每帧 26 个浮点。
2. `tools/kws-build-templates.py build/enroll-dump.txt`：读取 C 侧给出的 `cluster=`/`intra_max=`/`proposed_threshold=`，
   把对应段的特征写成 C 数组。
3. 重新编译 `lx06-duplex-armhf`；模板在二进制内，运行时不读文件。

主机与设备使用同一份 dump：设备端 `--kws-score` 重算的分段/距离与主机完全一致（到 4 位小数），说明数据来自同一份录音
（`/tmp/doubao_enroll.wav` 704044 B，SHA256 3f9a42ec…e7ebe，与 `private/inputs/recordings/doubao_enroll.wav` 一致）。

## 设备自检结果（`logs/native-kws-score-runtime.txt`）

```
wav=/tmp/doubao_enroll.wav seconds=22.00 samples=352000 rate=16000
templates=3 header_threshold=14.7784 intra_max=11.3680
segments=8
segment 1 start=590ms dur=780ms span=740ms frames=74 peak=43.7dB kept=1
segment 2 start=3640ms dur=640ms span=650ms frames=65 peak=40.1dB kept=1
segment 3 start=6470ms dur=2290ms span=2420ms frames=242 peak=39.9dB kept=1
segment 4 start=9040ms dur=850ms span=860ms frames=86 peak=37.8dB kept=1
segment 5 start=10370ms dur=300ms rejected=too_short
segment 6 start=11010ms dur=880ms span=790ms frames=79 peak=43.9dB kept=1
segment 7 start=12650ms dur=3180ms rejected=too_long
segment 8 start=17400ms dur=530ms span=620ms frames=62 peak=31.4dB kept=1
dtw[0][1]=8.2515   dtw[0][2]=14.9361  dtw[0][3]=11.3680  dtw[0][5]=14.0157  dtw[0][7]=16.2833
dtw[1][2]=16.7301  dtw[1][3]=11.3352  dtw[1][5]=14.2944  dtw[1][7]=16.2638
dtw[2][3]=12.3685  dtw[2][5]=16.9639  dtw[2][7]=14.2637
dtw[3][5]=13.2108  dtw[3][7]=13.5811  dtw[5][7]=15.7889
cluster=0,1,3 intra_max=11.3680 proposed_threshold=14.7784
verdict segment=0 dur=780ms dist=0.0001  result=below
verdict segment=1 dur=640ms dist=8.2515  result=below
verdict segment=2 dur=2290ms dist=-1.0000 result=above     (跨度 2420ms，长度门限拒绝)
verdict segment=3 dur=850ms dist=11.3680 result=below
verdict segment=5 dur=880ms dist=14.0157 result=below
verdict segment=7 dur=530ms dist=16.2833 result=above
scan_result wakes=5 first_wake_ms=1820 windows=102 best=0.0001
```

滑窗扫描（1.6 s 窗、0.2 s 步）判醒时刻：1820 ms、4420 ms、11820 ms、13620 ms、16420 ms，全部落在录音中的说话区间内；
录音尾部（17.9 s 之后）的噪声窗最低 16.8997，高于阈值，不外唤醒。

## 现场负样本与回放结果

| 运行 | 结果 |
| --- | --- |
| `--kws-capture 15`（安静期，峰值 25.0–41.6 dB） | 67 窗，16 窗打分，最低 14.4438，0 次判醒 |
| `--kws-live 30`（安静期待机） | 142 窗，2 窗打分，最低 18.9865，0 次误唤醒 |
| `--kws-live 20`（房间持续有人说话） | 92 窗，38 窗打分，最低 10.5620，**8 次误唤醒** |
| `--kws-wav /tmp/doubao_enroll.wav` | 1646 ms 首个满窗 dist=0.0001，1854 ms 连续第 2 窗判醒；前滚 4800 采样；无 Key → `wake_detected no_key_stdin detection_only=1` |
| `--kws 12`（待机 12 s，无 Key） | 5.06 s 处被室内说话声判醒（dist 13.51/14.20），停在 `detection_only`；未联网、未建会话 |
| `--kws-wav` + 真实 Key（`Invoke-DoubaoSmoke -Duplex -KwsWav`） | 1849 ms 判醒（dist 0.0001）→ upgrade=101 → session_ready=1 → preroll_uploaded=4800 → 1494 帧上行 → 正常关闭，`session=pass` |
| `--duplex 8`（重构后直连复查） | session_ready=1，398 帧上行，`session=pass` |

每次运行都核对：`mipns-xiaomi`=1282、`mico_aivs_lab`=2527 不变，临时二进制已删除，无 `/tmp/lx06-agent-*` 残留。
`/tmp/doubao_enroll.wav` 是本项目的注册录音，保留用于复跑自检；不需要时可删除。

误唤醒样本里被当成唤醒词的是其它说话声（跨度 570–910 ms，峰值 33–45 dB），距离 10.56–14.73，
和注册录音里最弱的一遍（530 ms，16.28）甚至是同样量级——说明**当前特征/模板无法把"其它人话"和"豆包豆包"分开**。

## 误唤醒、漏唤醒与适用边界

- 阈值 **14.7784**（= 1.3 × 簇内最大距离 11.3680），写在 `src/kws_templates.h`。
- 运行时可覆盖：`--kws <秒> <阈值>`、`--kws-wav <wav> <阈值>`，用于现场调；日志会打印每个窗的
  `peak/floor/noise/span/dist/hits`。
- 已实测的风险：弱段（530 ms，峰值 31.4 dB）距离 16.28 高于阈值，会被漏检；880 ms 段 14.02 只比阈值低 0.76，
  余量很小。注册录音本身噪声较大（底噪 20–23 dB，最弱一段信噪比约 12 dB），同一词的多遍之间距离就有 8–14，
  而现场其他人声在 10.5–15，**两者分布重叠**：阈值调高会漏检，调低会误唤醒。
- 具体失败现象与原因（实测）：房间持续有人说话时 20 s 内 8 次误唤醒。原因是本方案只有"3 个模板 + MFCC/差分 +
  20 % 带宽 DTW"，对"任意一段 0.6–0.9 s 的连续语音"给出的距离和"豆包豆包"同量级；注册样本太少（4 段有效），
  且注册录音噪声大（最弱一段峰值只有 31 dB）。这不是阈值没调好，而是特征/模板层面的判别力不足。
- 曾经的另一处失败：最初用"CMN + 全部 26 维"时，设备现场 45 s 待机出现 15 次误唤醒（距离 12.6–15.2，阈值 17.72）。
  改为"不去均值 + c0 包络×0.25 + 运行底噪门"后，安静期降到 0 次；有人说话时仍有上述误唤醒。属实测对比。
- 下一步建议（未做）：① 在安静环境多录 5–10 遍（近场、正常语速），重新生成模板；② 提高分辨率
  （40 mel / 20 cepstra / 加二阶差分）；③ 把 `LX_KWS_MIN_HITS` 从 2 提到 3–4（`src/native_kws.h` 顶部宏，
  需要时就地改，代价是响应延迟多 0.2–0.4 s）；④ 如果要真正可靠，需要小模型（例如复用/替换原厂 DNN 唤醒模型），
  纯 DTW 模板匹配在 128 MB 设备上可以做"候选筛选"，不宜单独承担最终判决。
- 推断（未实测）：安静的房间里可用阈值 14.8；若房间常有电视/音乐，建议用 13.5 或重新录制更干净的注册录音
  （多录 5–10 遍、安静环境、正常距离），再跑 `tools/kws-build-templates.py` 重新选定簇与阈值。
- 未验证：KWS 模式下真正说一句话→收到回答→播放完→回到待机的完整语音闭环（本次回放测试没有提问，模型保持沉默）。

## 复现命令


设备端直接跑（二进制由 `tools\Build-NativeGlibc.sh -Duplex` 生成）：

```
/lib/ld-linux-armhf.so.3 <bin> --kws-score /tmp/doubao_enroll.wav
/lib/ld-linux-armhf.so.3 <bin> --kws-live 30
/lib/ld-linux-armhf.so.3 <bin> --kws-capture 15
/lib/ld-linux-armhf.so.3 <bin> --kws-wav /tmp/doubao_enroll.wav
/lib/ld-linux-armhf.so.3 <bin> --kws 60          # Key 从 stdin 读，唤醒后才建会话
```

## 证据文件

- `logs/native-kws-score-runtime.txt`：设备自检全文（8 段、15 个 DTW、簇、判决、102 个窗）
- `logs/native-kws-replay-runtime.txt`：回放判醒与前滚
- `logs/native-kws-capture-runtime.txt`：现场 15 s 负样本
- `logs/native-kws-live-runtime.txt`：现场 20 s 待机逐窗日志（含 8 次误唤醒的每一窗）
- `logs/native-kws-standby-runtime.txt`：`--kws 12` 待机→判醒→（无 Key）detection_only
- `logs/kws-session-runtime.txt`：回放判醒后用真实 Key 建会话、上传前滚、关闭
- `logs/duplex-runtime.txt`：重构后直连会话复查
- `temp/analysis/lx06-duplex-armhf-elf.json`：ELF 校验（ELF32 EM_ARM、无 PT_INTERP、仅 libc/libdl/libpthread）
- `private/inputs/recordings/doubao_enroll.wav`：注册录音副本（与设备同一 SHA256）

注：改用当前度量之前的对照（CMN + 全维 + 阈值 17.72，现场 45 s 出现 15 次误唤醒）是开发过程中在同一设备上观察到的，
其日志已被后续运行覆盖，未留档；表格中的数字来自当前度量下的最终运行。

## 证据状态与未验证事项

- 实测：分段、特征、DTW 距离、模板簇、阈值、设备判决、滑窗判醒时刻、现场误唤醒计数、设备进程与清理状态。
- 推断：安静环境下的误唤醒率、阈值 13.5 的取舍、其他说话人的表现（注册录音只有本人）。
- 未做：真实按下唤醒→云端回答的闭环；原厂唤醒/米家功能的联动回归；长时间（>30 s）现场统计。
