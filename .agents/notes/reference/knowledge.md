# Reference: 知识索引（给后续模型）

本项目按“规范、状态、专题、证据、日志”分层记录。接手时按顺序读：`.agents/notes/reference/context.md` → 本索引 → 对应专题 → 最近一轮报告。

`.agents/notes/README.md`（中文版 `README.zh.md`）是权威规范：Agent Notes 的布局与命名、六类封闭分类、何时写、文件内格式、归档与删除；以及 `reference/` 与 `reports/` 两个受控区域和门禁脚本。新增或移动记录前先读它。

## 目录结构

- `.agents/notes/{proposed,implemented,rejected,archived}/{class}/yyyy-mm-dd-topic-title.md`：Agent Notes，路径即状态与分类。
- `.agents/notes/reference/`：手册、协议、策略、部署范围、状态流水与阅读指南。
- `.agents/notes/reports/`：逐轮证据与验证记录。
- `.agents/notes/scripts/verify-agent-notes.ps1`：三者的门禁。

## 记录约定

1. 决定类内容写成 Agent Note：按 `README.md` 的格式落位，再跑门禁。
2. 每次实质进展，更新 `reference/context.md` 与 `logs/operations.md`，并在 `reports/` 留一份带证据的报告。
3. 区分**实测**、**反汇编/资料证据**、**推断**；未验证的明确标注。
4. 部署哈希、PID、回滚位置只记在 `logs/operations.md`；不记录账号、密钥、Cookie、设备认证数据。
5. 设备上的临时诊断文件用完即删；持久结论写进仓库。

## Agent Notes

| 状态 | 文件 | 主题 |
|---|---|---|
| implemented/architecture | `.agents/notes/implemented/architecture/2026-11-01-agent-tool-registry-and-loop.md` | 工具注册表、统一执行入口、provider/host 循环、会话预算 |
| implemented/feature | `.agents/notes/implemented/feature/2026-11-01-wake-voice-and-barge-in.md` | 唤醒误触与漏触、应答音色、回答音量、唤醒词打断、会话结束 |
| proposed/architecture | `.agents/notes/proposed/architecture/2026-11-01-speex-aec-integration.md` | 8 通道布局、原厂 Speex AEC 逆向、原型数值、接入方案 |

## 参考文档（reference/）

| 文件 | 内容 |
|---|---|
| `reference/context.md` | 系统状态、逐轮变更、未决问题 |
| `reference/audio-pipeline.md` | ALSA 拓扑、8 通道布局、Speex AEC、原型实测 |
| `reference/control-protocol.md` | 本机 socket、工具端点、会话预算 |
| `reference/kws.md` / `reference/kws-revision13.md` | 唤醒词检测的操作与历史版本 |
| `reference/tools-roadmap.md` | 工具接入顺序 |
| `reference/realtime-client.md` | 实时协议与客户端说明 |
| `reference/ssh.md`、`reference/credentials.md`、`reference/native-probe.md`、`reference/native-observer.md`、`reference/deploy-revision14.md`、`reference/web-deploy-scope.md` | 访问、凭据、探针、部署与撤回 |
| `reference/wake-routing.md`、`reference/netease-account-plan.md`、`reference/zig-architecture-design.md` | 方案与设计 |
| `reference/music-test-scope.md`、`reference/knowledge.md` | 测试范围与索引 |

## 逐轮报告（reports/）

| 轮次 | 文件 | 主题 |
|---|---|---|
| 18 | `.agents/notes/reports/播放器曲目信息与歌单队列-第十八轮.md` | 曲目信息、原厂队列、上一首/下一首 |
| 18 补丁 | `.agents/notes/reports/歌单列表大响应修复-第十九轮.md` | 歌单接口 728 KiB 响应与接收上限 |
| 20 | `.agents/notes/reports/Agent循环与工具注册表-第二十轮.md` | 工具注册表、provider/host 循环、会话截止 |
| 21 | `.agents/notes/reports/语音交互调优与回声消除逆向-第二十一轮.md` | 会话结束、应答音色、音量、唤醒策略、打断、AEC 逆向 |
| 26–28 | `.agents/notes/reports/本地302直链刷新与原生循环-第二十六至二十八轮.md` | 本机 302、600 首队列、原生 SEQUENCE 循环、直链过期修复 |
| 29 文档 | `.agents/notes/reports/开源文档入口重写-第二十九轮文档.md` | README、CONTRIBUTING、SECURITY 与开源入口整理 |

## 材料与工具

- `rom/`：公开固件与上游源码的获取入口。仓库只保存固定点、校验值与 `Fetch-Inputs.ps1`；有公开链接的字节不重复入库，本地缓存被忽略。
- `private/`：凭据、SSH、设备分区镜像/回读、唤醒录音、运行时快照；不入库、不发布。
- `temp/`：解包根文件系统、逆向分析产物、清单；随时可删、可由 `tools/` 重算。
- 规则见 `rom/README.md`、`private/README.md` 与 `tools/README.md`。

## 逆向资料

`temp/analysis/` 保存符号表、反汇编、函数边界、模型与协议证据：

- `usr_lib_libxaudio_engine.so-symbols.json` / `-address-labels.json`：AEC/录音相关导出符号与地址。
- `native-dnn-frame-layout.md`、`usr_bin_mipns-xiaomi-*.txt`：唤醒引擎的调用约定与帧布局。
- `tools/dump-native-functions.py`（配合 `vendor/python-analysis`）可按符号生成反汇编。

## 门禁

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .agents/notes/scripts/verify-agent-notes.ps1
```

检查生命周期与分类的封闭集合、Agent Note 的头部与章节骨架、`reference/` 的文件名与 `# Reference:` 头部、`reports/` 的 `# Report:` 头部，以及禁止的集中式 `INDEX.md`。只校验，不改写。
