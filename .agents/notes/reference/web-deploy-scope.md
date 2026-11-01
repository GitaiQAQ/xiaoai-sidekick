# Reference: Web 控制台部署范围

第十七轮增加网易云榜单（最多80榜单/每榜前20首），豆包FC固定音乐方法与私有Unix Socket `/data/lx06-agent/control.sock`（0600，父目录0700）。更新自有Web/语音二进制及app.js/app.css，不改原厂服务。部署前将四项旧文件保存到设备自有目录revision17-original，停止自有语音和Web后原子替换；撤回时同样先停止两服务，再恢复该目录对应文件。保留netease.session，不删除账号会话。音乐播放采用短期内存票据，语音确认结束后提交，无需URL代理。

在 `/data/lx06-agent/` 增加 `lx06-web-armhf` 和 `web/` 静态文件；更新自有语音程序以提供 `/tmp/lx06-agent-state.json`。不改原厂固件、系统服务或启动项，不停止米家、网关或原厂语音进程。

Web 监听音箱 18080 端口，仅用于局域网。API 校验实际本机 IP 的 Host、同源 Origin 和随机 CSRF token；无跨域许可。命令只允许固定方法和参数，不提供任意 shell、文件读取或原始账号信息。

豆包 Key 可通过 SSH stdin 载入 Web 进程内存，用于用户从页面启动助手；不写配置或日志。网易云扫码成功后，在音箱自有目录写入 0600 的 `netease.session`，退出登录清除；API 只返回昵称、会员类型和验证状态，不返回 Cookie。

部署前保存现有自有语音程序到 `build/revision16-original/`。更新时先停止自有 Web 和语音程序，逐文件 SHA 验证后替换。撤回：停止 Web，恢复保存的语音二进制，仅删除本轮新增的明确文件；网易云会话先从页面退出登录。无需刷固件。不会为了释放空间删除原厂文件。

测试会覆盖 HTTP framing、Host/Origin/token 拒绝、状态/事件 API、二维码获取、实际扫码、会话恢复和退出、音乐请求。测试语音助手必须有界并收尾；可交付持续运行的 Web 页面，不能留下未结束的付费语音会话。






## Revision 18 deployment scope

Updated device files: `/data/lx06-agent/lx06-web-armhf`, `lx06-duplex-armhf`, and `web/index.html`, `web/app.js`, `web/app.css`. Before deployment, the current versions were copied to `/data/lx06-agent/revision18-original/`; `netease.session` was preserved. Sidekick wake/listening was stopped for the replacement, then Web restarted as PID 3196. Sidekick voice remains stopped; factory `mipns-xiaomi` and `mico_aivs_lab` remain running.

The new queue loads at most 20 Netease chart or user-playlist tracks per request. Restore by stopping the Sidekick Web/voice processes and copying the matching files from `revision18-original`; keep the Netease session file. Audio stays direct to the factory player without an audio relay.


The final saved-session refresh adjustment was rebuilt and deployed after taking `/data/lx06-agent/revision19-original/`. Final Web SHA256: `E45B9CB98F49E50E8D21DBE7F6F3D43FBF99D47074C62BA4688F049518071F2A`; final Web PID: 3446. Duplex binary and browser assets were unchanged in this last deployment.
