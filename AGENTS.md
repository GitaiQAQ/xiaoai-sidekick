# 后续代理工作指引

先阅读 README.md、.agents/notes/reference/context.md、logs/operations.md、.agents/notes/reference/sources.md。
目标是机内 Agent，省去外部常开服务器，保留米家和网关功能。
每次有实质进展后更新上下文和操作日志，区分实测、公开资料和推断。
不要重复安装驱动。不要打印或提交账号、密钥、设备认证数据。
backups/ 作为原始资料保留，不直接编辑。对镜像做修改请输出到 firmware/。
工具输出 success 不等于实际读到命令结果，文件长度匹配不等于已验证可恢复。
当前system0已刷SSH补丁并验证登录；最新状态以.agents/notes/reference/context.md和报告为准。新的设备写入前应形成具体可审查的镜像与恢复方案。


用户已要求音箱不保留历史部署备份：部署回滚材料保存并校验到 Mac 的 private/deploy-recovery/，不要再创建 /data/lx06-agent/revision*-original；设备临时 staging 验收后清理。此规则不影响 backups/ 原始资料保留。
