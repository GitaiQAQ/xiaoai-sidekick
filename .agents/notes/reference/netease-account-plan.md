# Reference: 网易云账号与会员音源接入

目标是机内原生客户端，不安装桌面音乐应用；复用原厂curl/crypto/json和播放器。用户允许播放URL进原厂日志，不因隐藏URL加代理。密码、Cookie和豆包Key不写日志。

实施顺序：
1. 按上游代码核对登录请求的当前加密协议、请求头和Cookie处理；先将加密/响应解析做离线向量测试，再只请求二维码登录key。
2. 在电脑临时展示网易云登录二维码，由用户用网易云App扫码确认。电脑只负责登录时显示，最终播放不依赖它。轮询有明确时限，不在聊天中贴Cookie/登录凭据。
3. 登录成功保留最小会话数据，机内0600私有文件、目录0700；如需持久化先明确保存位置与退出登录清理。不得把账号Cookie拼在歌曲URL上。
4. 查询账号登录/会员状态；对用户指定歌曲请求账号有权使用的音质，检查是否试听、缺权限或无URL。拿到URL不等于已验证完整会员权益。
5. 直接交原厂player_play_music播放，验证完整时长、首播、切歌、断流、过期地址刷新。先兼容格式，再逐项测FLAC/高采样率；不能笼统承诺官方客户端全部会员能力。
6. 加搜索、个人歌单和收藏，注册豆包音乐tools，实现账号→音源→播放真实反馈。

已核对的第三方一手源码（仅协议参考，非网易官方开放接口，也未接入实测）：
https://raw.githubusercontent.com/NeteaseCloudMusicApiEnhanced/api-enhanced/main/module/login_qr_key.js ：/api/login/qrcode/unikey，type3。
https://raw.githubusercontent.com/NeteaseCloudMusicApiEnhanced/api-enhanced/main/module/login_qr_check.js ：/api/login/qrcode/client/login，type3/key，Cookie会话。
https://raw.githubusercontent.com/NeteaseCloudMusicApiEnhanced/api-enhanced/main/module/song_url_v1.js ：/api/song/enhance/player/url/v1，当前使用xeapi。仅参考正常会员请求分支，不接其unblock分支。
https://raw.githubusercontent.com/NeteaseCloudMusicApiEnhanced/api-enhanced/main/module/vip_info.js ：VIP信息查询参考。

当前尚未生成可扫码的登录入口、没有网易云会话，也没有会员曲目实测。不能让用户现在盲目扫码或提供密码。
