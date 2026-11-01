# Reference: 原厂 speech 会话的短时元数据观察

代码：src/native_speech_observer.c。目的为验证真实会话类型/时序；不是双唤醒或豆包接管程序。

## 已验证的前置条件

实机 /proc 映射确认 mico_aivs_lab PID2527 的 TID2601 正阻塞在 ARM recvfrom(292)、FD8；该 FD 的 socket inode8402 与 /tmp/mico_aivs_lab/usock/speech.usock 一致。TID2602/FD9 是 common.usock，不能混用。TID/FD 必须在重启后重新发现，程序本身也校验原厂可执行文件与 FD 对应的准确 speech socket 路径，拒绝其他对象。

同一台音箱上合成进程实验已验证：ARM32寄存器集72字节；r12=0/1分别区分系统调用入口/出口；读取一条15B合成流消息时只读取12B协议元数据，跳过3B合成载荷，目标收到的数据未改变。另已验证观察器正常到时 detach 后目标继续收包，以及观察器被SIGKILL后目标继续收包。

入口/出口标记的初始依据为Linux4.9官方 arch/arm64/kernel/ptrace.c 的 tracehook_report_syscall，随后在实机合成进程验证。不能因用户态为ARM32而使用arch/arm32内核假设。

## 本轮真实观察范围与撤回

先观察上述已定位线程5秒。程序仅PTRACE_SEIZE/INTERRUPT/SYSCALL/GETREGSET/DETACH，不设置寄存器或内存，不启用EXITKILL。它以process_vm_readv逐字节读取Protobuf字段键、长度和需要的标量，字节字段内容只移动偏移、不读取。日志只保存类型、数量与长度，不保存音频、对话文本、会话ID或供应方字符串。

观察器会造成被观察线程在系统调用边界的短暂停顿，因此属于短时诊断，不是最终常驻音频接入方式。只挂会话通信线程，不挂麦克风/AEC/DNN线程。原厂socket路径、回调函数和会话报文不被重写，不安装服务或开机项。

二进制仍按本地/远端SHA256核对后临时执行，内置1～60秒期限，SIGINT/TERM/HUP也触发detach。结束检查目标TracerPid=0、State无跟踪停止、原厂PID及Mesh状态，再清除临时二进制。正常/观察器异常退出恢复已由合成进程验证；不能推导为任何系统故障都能恢复，也不能用空闲窗口代替真实唤醒回归。

构建：tools/Build-NativeProbe.ps1。运行：tools/Run-NativeProbe.ps1 -Kind observer-test；真实窗口使用 -Kind observer -ThreadId 2601 -SocketFd 8 -Seconds 5。请通过当前会话采用的scriptblock方式调用，避免Windows脚本执行策略差异。

实测已完成：用户准备好后开启60秒真实唤醒窗口，65条上行消息解析成功、错误0、音频字段累计长度157440B、元数据读取961B。用户确认正常唤醒并回答。结束TracerPid0、线程正常睡眠、原厂PID和Mesh正常；无临时文件。日志native-observer-user-wakeup-runtime.txt及对应health.txt。随后版本新增stream_packets/min_stream/max_stream和首条流消息日志；该版本已重新通过合成/恢复测试，未重做用户窗口，因此不能将新增统计当作前次窗口已取得的数据。
