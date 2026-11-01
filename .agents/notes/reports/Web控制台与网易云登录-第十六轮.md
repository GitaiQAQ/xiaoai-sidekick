# Report: Web 控制台与网易云账号接入（2026-11-01）

交付入口：http://192.168.100.210:18080/ 。原生ARM服务、静态页面和网易云适配均在音箱内，不依赖NAS、Node或外部音乐API服务。

## 已实测

- 页面展示资源、Mesh、助手阶段、灯效、播放器和最近操作；支持助手启停、音量、直接音源、暂停/继续/停止、网易云登录及搜索播放。
- 用户实际打开页面扫码并确认，账号验证 authenticated=1、API code=200、vip_type=110。会话文件 root:root、0600、725字节。重启Web后状态session_saved，刷新账号恢复logged_in，无需再次扫码。未测试设备断电重启。
- 用户指定《左手指月》，搜索后播放萨顶顶原版（536096151），请求音质exhigh。接口拒绝带freeTrialInfo的试听音源。原厂播放器总时长230544毫秒，进度由7469增长至110309毫秒，用户确认“正常播放”。未等整首播完，未核实实际码率，不能把请求音质当成实际返回音质。
- Web重启没有中断歌曲。Web RSS约1780–1808KiB；语音启动时可用RAM约39720KiB，/data剩7860KiB，Mesh运行标记true。
- HTTP验证通过：真实状态、4项资源、路径穿越拒绝、未知方法及ID回显、错误来源/token拒绝、SSE快照、不暴露凭据字段。日志：logs/web-http-validation.txt。已有登录时跳过新二维码申请。
- 观察到页面启动的自有助手进程和阶段；收尾通过voice.stop停止付费语音会话，歌曲继续。Web及网易云登录保留。

## 实现与交付文件

HTTP+JSON v1统一method/params/id，SSE每次发送快照后关闭、约4秒重连。定义见.agents/notes/reference/control-protocol.md；暂不引入Protobuf/MessagePack，未来Unix Socket复用JSON封装。

src/native_web_console.c复用原厂libjson-c/libcurl/libcrypto和播放器ubus，固定方法，不提供任意shell。src/native_netease.h实现eapi、二维码、账号验证、会话保存/本机退出、搜索、授权音源。TLS校验开启，请求有长度和时间上限。逐曲检查权限，不绕过会员、版权或地区限制。

Web SHA256：C8F0DF425177F6CD8B0CC464FA2F806C8898AC24D5A45DEB505F9BBB948F2AF5。
语音 SHA256：788F884C0CB3661488453D4308FD1391E1C9310C39B3099BE52F4C3E19480B54。
ELF32 ARM，依赖libc/libdl，使用原厂动态加载器。部署/撤回见.agents/notes/reference/web-deploy-scope.md。本轮未改固件、原厂服务或启动项。

## 尚未完成

- 这是登录、搜索、授权单曲播放首版，不等于完整网易云客户端；收藏歌单、推荐、歌词、队列、自动续播尚未实现。无损FLAC、实际码率、会员到期时间未验证。
- 本机退出已实现，但未退出刚登录的真实账号，以保留用户会话。
- 豆包tools尚未接入控制方法；米家只有运行状态展示，尚未实现页面/模型控制设备。
- 语音新增阶段快照和待机超时续期；长期待机及音乐环境误唤醒仍需测试。本轮发现活跃收音并收尾，不推断触发原因。
- 无开机自启；Web重启后点击账号刷新恢复验证；豆包Key仅在Web内存，设备重启后需重新安全注入。
- 自动审批拒绝启动本机浏览器，未做自动截图检查；真实页面打开、扫码和HTTP检查已完成。

