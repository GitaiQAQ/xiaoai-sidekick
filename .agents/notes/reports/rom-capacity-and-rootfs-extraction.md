# Report: ROM Capacity and Root Filesystem Extraction (Round 33)
> 历史验证记录：本文保留对应阶段的实验条件、数据和限制；其中设备进程、部署位置与恢复目录只描述当时状态，不是当前部署说明。当前操作边界见项目的有效 Reference 文档。

## SquashFS 有效长度与分区余量

本地 rom/LX06_1.94.13.squashfs SHA256 317a54909f12b63f22128965bf9005c06435d15f61c4202bb88c4524bf1e2ef4，文件长度与 SquashFS bytes_used 均为 37500699 B（35.76345 MiB）；SquashFS v4.0，XZ压缩，128KiB块，2137 inode。

通过SSH只读 /dev/mtd4 前96字节，当前SSH补丁ROM bytes_used=38449988 B（36.66877 MiB），2138 inode；40MiB槽=41943040 B，因此有效文件系统以外尾部余量3493052 B（3.33123 MiB）。这不是读取尾部并确认全为FF/00；仅准确计算槽容量与有效长度差，不保证全部NAND物理可写（坏块/镜像布局仍需核验）。挂载df约36.75MiB不是精确镜像有效长度；先前3.25MiB说法用df近似，现以头字段纠正。

## 原厂镜像解包结果

原有 temp/rootfs-factory-system0 仅24普通文件、12416377 B，是选择性分析材料，不是完整rootfs。使用vendor/py内dissect.squashfs 1.10完整遍历本地原厂镜像，1348普通文件全部解压读长度核验，逻辑合计89810617 B（85.65MiB），普通文件写入 temp/rootfs-factory-full；符号链接、设备节点和权限未重建，因此该目录只能分析，不能直接作刷机打包源。完整元数据统计 temp/analysis/rom-space.json，均忽略不提交。未打印文件正文，未编辑原始镜像或backups。

| 内容 | 解压逻辑大小MiB |
|---|---:|
| /usr | 70.86 |
| /lib | 12.79 |
| /bin | 1.17 |
| /etc | 0.64 |
| /sbin | 0.20 |
| /init | 0.00 |
| /rom | 0.00 |

主要目录：/usr/lib 33.54MiB、/usr/bin 19.86MiB、内核模块7.51MiB、/usr/share/xiaomi 6.00MiB。均为未压缩逻辑大小，不能据此推断删除能回收等量ROM；驱动、原厂语音、米家/Mesh组件未判定可删除。

## Sidekick 文件的容量初筛

当前Duplex123876 B + 一级639597 B + 二级1678701 B =2442174 B（2.32904MiB）。直接加到当前ROM bytes_used的算术上界为40892162 B（38.9978MiB），距40MiB约1050878 B（1.0022MiB）。这是不计新文件压缩、元数据和重打包参数差异的初筛，不是已产出候选镜像。Web、网页、脚本还需计入；固定内容转入ROM可减少/data占用，凭据/状态仍放/data。

结论：无需先删原厂组件，现有尾部余量从容量上有机会容纳程序和模型。真正打包必须从匹配当前补丁版本且保留权限/链接/设备节点的完整源开始，实际mksquashfs大小、启动/依赖、分区及坏块、恢复方案验收后再判断。未重打包、未刷写、未扩大/data。
