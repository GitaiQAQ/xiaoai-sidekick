# Contributing

感谢你愿意改进 XiaoAI Sidekick。这个项目直接操作真实音箱的固件、分区、音频设备和云端账号，贡献时请把**可恢复性**和**隐私**放在功能数量之前。

开始前请阅读：

- [README.md](README.md)：项目边界、当前能力和已知限制；
- [AGENTS.md](AGENTS.md)：设备写入、凭据和记录规范；
- [.agents/notes/README.md](.agents/notes/README.md)：文档生命周期、分类和门禁；
- [SECURITY.md](SECURITY.md)：安全问题与敏感数据的处理方式。

## 可以贡献什么

- LX06 以外型号的**只读**兼容性调查；
- 音乐播放下的误唤醒负样本、AEC 改进和可复现实验；
- 长歌单的 `trackIds` / 分批详情方案；
- 米家、红外等能力的固定接口设计与安全评审；
- 构建、部署、回滚和回归脚本；
- 文档修正、错误复现和现场日志整理。

不接受：

- 把任意 shell、原始 ubus 透传或未审核设备动作包装成模型工具；
- 绕过音源授权、DRM、试听限制或账号权限；
- 提交 API Key、网易云 Cookie、SSH 密码、个人录音、设备分区镜像；
- 未经说明的固件写入、OTA 修改或“永久开启”类改动。

## 开发环境

设备端程序是 ARM32/glibc 2.25 原生 C，Web 控制台是静态 HTML/CSS/JS。主要构建脚本在 `tools/`，使用 Zig 0.13 交叉编译，并链接设备自带 glibc。

Windows PowerShell 是当前主要开发环境：

```powershell
powershell -NoProfile -File tools/Build-NativeGlibc.ps1 -Web
powershell -NoProfile -File tools/Build-NativeGlibc.ps1 -Duplex
```

产物在 `build/`，构建后应通过 ELF 架构和依赖检查。工具链恢复、分析脚本和 macOS 运行方式见 [tools/README.md](tools/README.md)。

macOS/Linux 目前需要手动准备 Zig、curl 7.55.1 头文件和设备 `libc-2.25.so` / `libdl-2.25.so`；请把可重复的脚本作为 PR 的一部分，不要只提交本地产物。

## 修改设备前

1. 先读相关报告和[操作日志](logs/operations.md)，确认你要改的文件、进程和回滚路径。
2. 备份将被替换的文件；写入前核对 SHA256，写入后读回核对。
3. 说明会影响哪些原厂进程（`mipns-xiaomi`、`mico_aivs_lab`、`mediaplayer`、Mesh/米家等）。
4. 测试必须可收尾：停止自己启动的语音会话，关闭测试服务，删除临时二进制和 FIFO，确认没有持续计费连接。
5. 不要为了“清理空间”删除原厂文件或未知目录。

## 提交前检查

- [ ] 改动范围单一，包含必要的回滚说明；
- [ ] 已在本机或设备上执行对应测试，并记录命令、输出和限制；
- [ ] 没有新增明文凭据、Cookie、个人录音或设备特定回读；
- [ ] 文档区分**实测**、**公开资料**和**推断**；
- [ ] 更新了 `logs/operations.md`；有阶段性结论时补充报告或上下文；
- [ ] Web/API 改动检查了 Host、Origin、CSRF、长度和超时边界；
- [ ] 设备写入类改动在 PR 中写清分区、文件、哈希和恢复步骤；
- [ ] 新增依赖写明来源、版本和许可证。

## 提交信息

建议使用简短的祈使句，说明“做了什么”和“为什么”：

```text
Add local 302 stream refresh for Netease queues

Queue local stable URLs and resolve CDN links when the factory player
opens each track. Keeps long queues independent of signed-URL expiry.
```

一个提交只做一件事。不要在同一提交里混入个人设备快照、格式化整份历史日志或无关的唤醒实验。

## 文档规范

- 参考文档放在 `.agents/notes/reference/`，逐轮证据放在 `.agents/notes/reports/`；
- 文件名使用小写 kebab-case（中文报告保留既有命名），报告以 `# Report:` 开头；
- 新报告必须写清环境、命令、结果、限制和未验证项；
- 修改共享状态或部署后，同步 `.agents/notes/reference/context.md` 与 `logs/operations.md`；
- 不要删除历史报告来“让 README 干净”；README 只做入口，历史留在报告里。

## 安全

不要在公开 Issue/PR 中贴出设备序列号、局域网完整地址、Key、Cookie、二维码内容或完整原始日志。需要提供日志时，先删去认证信息、URL 查询串和个人数据。安全问题请按 [SECURITY.md](SECURITY.md) 私下报告。
