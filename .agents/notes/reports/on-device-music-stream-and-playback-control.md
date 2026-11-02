# Report: 第十五轮：机内音乐流与原厂播放控制
> 历史验证记录：本文保留对应阶段的实验条件、数据和限制；其中设备进程、部署位置与恢复目录只描述当时状态，不是当前部署说明。当前操作边界见项目的有效 Reference 文档。

## 验证范围与结论

已实现原生C音乐适配器并在LX06验证HTTP WAV流的播放、暂停、继续、替换曲目和非播放状态。音频由音箱回环HTTP测试源实时生成，不靠电脑/NAS提供音频、不下载整曲。没有常驻代理、没有部署音乐账号服务或注册豆包音乐工具；网易云登录、会员音源、MP3/FLAC、HTTPS、拖动、自然结束、队列自动续播仍未完成。

用户明确接受播放URL写原厂日志，取消原先为隐藏地址而考虑的本地代理。最终直接把HTTP/HTTPS地址交原厂音乐接口；密码、Cookie、豆包密钥仍不进日志或播放参数。

## 播放接口与测试中发现的问题

直接分析本机原厂mediaplayer，mi_play_url 0x273b0显式syslog播放URL；parse_audio_player 0x2be60解析payload/audio_items/item_id/stream。原厂/usr/bin/mphelper实际用media="common"，不是common_mode；本机反汇编common_mode是特殊分支，不能凭名字猜用途。

第一次player_play_url/type0进入临时播报路径，流读取成功，但pause未达目标；清理时恢复了以前缓存的歌单。已立即用原厂mphelper pause暂停恢复的歌单，没有让其继续播放。第一份日志被后一次测试覆盖，不能虚构完整首测归档。

改为player_play_music，其music参数是包含payload、audio_type=MUSIC、play_behavior=REPLACE_ALL、audio_items的JSON字符串，stream.url为播放地址。控制使用player_play_operation，media=common，pause/play/stop映射。本机状态实测播放=1、非播放=2；stop也返回2、保留曲目，不能声称彻底清空队列。初始status3不同于本次stop结果。

测试替换了原厂缓存队列为测试单曲；之前上下文保存private/music-context-before-test.json，但不是完整可恢复队列，不能声称旧缓存歌单恢复。无音量调整，最终volume19、loop_type1恢复，播放处于非播放状态。

## 实现边界

- src/native_music.h：json-c动态加载，固定ubus子进程（不经shell），输出上限8192B、子进程超时；只返回状态/音量/循环/进度/时长数值，不向模型传回账号或原始上下文。直接URL最长900B，仅HTTP/HTTPS，拒绝控制字符/用户认证形式；使用JSON构造器转义参数。
- src/native_music_client.c：stdin JSON命令state/play/pause/resume/stop；PR_SET_DUMPABLE=0。--self-test只做参数拒绝测试，不播放。
- src/native_music_stream_test.c：静态musl回环HTTP测试源，55秒硬上限、127.0.0.1:18087、20秒低幅度WAV、支持基本Range，960B块按20ms实时发送，不是生产代理。
- tools/Build-NativeGlibc.ps1 -Music和native-library-elf.py支持新目标。原厂glibc显式loader运行，不安装依赖。
- tools/Test-NativeMusic.ps1：随机/tmp目录、上传SHA验证、只在非播放状态短测、清理本次播放与临时程序。未作持久部署。

## 验证证据

logs/native-music-stream-attempt2.txt保留第二轮stop状态假设失败。最终logs/native-music-stream-test.txt：
播放status1→暂停status2→继续status1（进度2550ms/时长20000ms）→替换曲目status1→stop后status2。
回环源收到两个音频请求，分别发送252480/100800B，日志fixture_stopped与temporary_test_removed=1。恢复播放后进度增加；暂停后的第一秒进度0→364ms，不能将其描述为严格零误差暂停。

最新程序SHA F09BDFD16180E69C9405F796B0C8D8AEF91B2033D2C33962C3D6790428E0FDF5，ARM32 hardfloat、仅libc/libdl、GLIBC2.4/2.17，显式原厂loader。最新变化仅增加自检入口；流控制实测对应上一构建BE62F904...。7个参数拒绝检查在实机通过，RSS峰值396KiB仅是自检进程，不代表包括原厂播放器/解码器的播放总预算。logs/native-music-validation.txt：原厂PID1282/2527不变，Mesh正常，最终无测试音频源/程序常驻。

## 复现命令与未完成事项

```powershell
.\tools\Build-NativeGlibc.ps1 -Music
.\vendor\zig-portable\zig-windows-x86_64-0.13.0\zig.exe cc -target arm-linux-musleabihf -mcpu=cortex_a53 -std=c11 -Wall -Wextra -Werror -Oz -static -s src/native_music_stream_test.c -o build/lx06-music-fixture-armhf
.\tools\Test-NativeMusic.ps1
```

下一步：账号登录与授权音源接口；在同一provider接口接网易云。先验证搜索/歌曲权限/一首会员完整播放，再谈完整会员能力。播放基础增加双曲目队列、seek和自然结束测试，接入豆包function call与TTS暂停恢复仲裁。不要将本次WAV回环成功外推为真实网易云服务已完成。
