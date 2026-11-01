# Reference: 机内适配基础代码与只读探针

这不是已经完成的豆包 Agent。`src/native_protocol.c` 提供恢复后本地协议的有界元数据解析，供后续会话观察和路由使用；`src/lx06_probe.c` 验证编译目标与设备运行能力。

解析器不分配堆内存，限制输入 64 KiB，检查 varint 溢出、截断、重复关键字段、方向与子消息冲突。输出只有方向、消息类型、流类型和音频长度；不复制音频、文字或会话标识。它不是完整 protobuf schema 校验器，未解析子消息的必填字段不在本轮校验范围。下行外层方向值 1 已从 mico_aivs_lab 0x35adc～0x35ae8 组包代码核对；尚待实机会话捕获交叉验证。

编译使用项目内 Zig 0.13.0，Windows 官方压缩包 SHA256：`d859994725ef9402381e557c60bb57497215682e355204d754ee3df75ee3c158`。原始下载元数据保存在 `temp/analysis/zig-download-index.json`。编译器不安装到系统或音箱。

## 本轮设备操作范围

1. Windows 上生成 ARM32 hard-float、静态链接的 Linux 二进制，保存本地 SHA256。
2. 只将该二进制写入音箱 `/tmp/lx06-agent-probe-<SHA256前12位>`，拒绝覆盖已有同名路径；校验远端 SHA256 后再运行。
3. `--self-test` 使用合成数据验证解析器；`--status` 只读取 uname、/proc/meminfo 和两个原厂 socket 的文件状态，不打开 socket、不读取麦克风、不连接云端。
4. 同一个远程 shell 用退出清理移除该精确临时路径，随后确认文件消失、原厂进程 PID 和 Mesh 状态。

不写闪存、/data、原厂配置或启动项，不停止服务、不接管唤醒。撤回就是移除这个临时文件；掉电也会清除 /tmp。后续会话路由、hook 或持久部署需要另行形成基于实测的方案。

另有 `src/unix_handover_test.c`，只在随机新建的 `/tmp/lx06-socket-test-XXXXXX` 目录创建四个合成 Unix datagram socket，验证路径移位、双向转发、发送者地址与恢复直连行为；结束关闭 socket、删除测试文件和该目录。它不访问原厂 socket。这是在原设备上验证后续无需重启原厂进程的可能接入机制，成功也不代表原厂应用一定支持在线移位（例如已 connect 的 socket 可能继续引用原 inode）。

Windows 主机编译首测遇到 Zig 生成 user32 导入库失败，未计为测试通过。默认改为直接生成 Linux ARM 静态二进制，协议测试在音箱执行；`-HostTest` 保留主机编译选项。

实测结果：ARM 探针 31056B，20 项合成协议检查通过；socket 实验 24156B，双向转发、发送者路径保持、撤回直连通过。日志见 logs/native-probe-runtime.txt、native-socket-test-runtime.txt。传输使用设备已有 printf 八进制转义，远端执行前校验 SHA256；base64 命令缺失的首次尝试已清理。

## 原厂动态库复用探针

src/native_library_probe.c仅加载现有libcurl、查询版本和符号，不调用网络或音频接口。生成ARM32/glibc2.25动态程序，使用原设备/lib/ld-linux-armhf.so.3。Zig自动生成glibc启动文件失败后，改用项目内最小ARM入口/C桥接，与本地原厂libc-2.25.so和libdl-2.25.so直接链接，不在设备替换任何库。这个入口没有应用构造函数支持，仅适用于当前无此需求的小程序，后续不能未经核对直接套用任意第三方C++应用。

Run-NativeProbe.ps1 -Kind libraries将其按相同哈希核对/临时清除流程执行，附带strace -s0网络系统调用核对。临时trace与程序一起清除，输出存logs/native-libraries-runtime.txt。仅库ABI和功能符号检查，不代表豆包鉴权或实时会话成功。

链接修正：当前产物没有PT_INTERP，必须由/lib/ld-linux-armhf.so.3显式执行。首次误直接执行导致本探针SIGSEGV，原厂进程未变、临时文件已清理；失败输出单独保存。现已加ELF架构、依赖、符号版本和显式loader执行模式校验。不要直接执行此动态产物。

默认库探针已实测成功，libcurl7.55.1/OpenSSL1.0.2l/c-ares1.13.0，初版峰值RSS936KiB、strace无网络调用。可选-Kind tls使用相同小程序--tls，只向Ark与openspeech两个固定公开HTTPS地址发HEAD，保持证书及主机名校验、禁重定向、5秒连接/10秒总超时；不带密钥、不发POST或音频、不打印响应正文。结果只证明传输/TLS，不证明模型调用。
