# Private

设备特定与个人数据。**永不入库、不发布**，`.gitignore` 已排除整个目录。

| 路径 | 内容 | 为什么私有 |
|---|---|---|
| `credentials.json` | 豆包/方舟等 API Key | 泄露即风险 |
| `ssh/` | SSH askpass helper、`known_hosts` | 凭据 |
| `inputs/readbacks/` | `boot0.img`、`boot1.img`、`system0.img`、`system1.img`、`system0-patched-readback.img`、`system1-ssh-read-partial.img` | 从本机读出的分区镜像，含设备特定状态 |
| `inputs/recordings/doubao_enroll.wav` | 唤醒词录音 | 个人声纹 |
| `inputs/live/live-*.bin`、`live-system-selected.tar.gz` | 运行时内存/回调快照、设备文件抽样 | 可能含会话数据，且不可复现 |

公开的复现输入在 `rom/`；可重建的派生产物在 `temp/`。三者不要混用。

## 备份

这些文件不进入版本库，需要单独备份。恢复时按 `rom/README.md` 的公开固件重建 `temp/`，私有镜像只在回滚真机时使用。
