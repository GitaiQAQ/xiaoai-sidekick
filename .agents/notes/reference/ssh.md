# Reference: SSH 已验证

设备 IP：192.168.100.210（DHCP，可能改变）
硬件：LX06
ROM：1.94.13
内核：Linux 4.9.61 aarch64

连接命令：
```powershell
& 'C:\Windows\System32\OpenSSH\ssh.exe' -o HostKeyAlgorithms=+ssh-rsa -o UserKnownHostsFile=C:/Users/gitai/Desktop/lx06-agent/tools/known_hosts root@192.168.100.210
```

补丁默认登录方式见上游文档，未改密码。SSH 登录和只读版本检查成功。
