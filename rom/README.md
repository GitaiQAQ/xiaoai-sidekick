# ROM 与复现材料

给别人复现用的固件与上游源码。**不需要小爱音箱**：解包这里的 squashfs、跑 `tools/`，即可得到与设备一致的分析结果。

## 内容

| 文件 | 说明 | 入库 |
|---|---|---|
| `LX06_1.94.13.squashfs` | 1.94.13 原厂 rootfs，37.5 MB | 否（公开链接，按需下载） |
| `LX06_1.94.13_patched.squashfs` | 上游打过 SSH 补丁的 rootfs，38.5 MB | 否（公开链接，按需下载） |
| `open-xiaoai-main.zip` | 上游源码归档 | 否（公开链接，按需下载） |
| `upstream-main-commit.json` | 归档的内容哈希固定点 | **是** |
| `SHA256SUMS` | 上述文件的校验值 | **是** |
| `upstream/open-xiaoai-main/` | 解压后的上游源码 | 否（可用 zip 重建） |

原则：**有公开链接的字节不重复入库**。仓库只保存来源、校验值和获取脚本；本地下载的文件留在 `rom/` 但被 `.gitignore` 忽略。如果上游链接失效，再考虑把镜像放到自己的 release 里，并更新 `Fetch-Inputs.ps1` 的地址。

```
LX06_1.94.13.squashfs
  https://github.com/idootop/open-xiaoai/releases/download/LX06_1.94.13/LX06_1.94.13.squashfs
  SHA256 317a54909f12b63f22128965bf9005c06435d15f61c4202bb88c4524bf1e2ef4
LX06_1.94.13_patched.squashfs
  https://github.com/idootop/open-xiaoai/releases/download/LX06_1.94.13/LX06_1.94.13_patched.squashfs
  SHA256 3fbcfcd8a2130f5421d0bc3a3b22cb5a3de59b5d3678a3a14b23541aa9474225
```

重新下载并校验（文件已存在且匹配时跳过）：

```powershell
powershell -NoProfile -File tools/Fetch-Inputs.ps1
```

## 为什么没有整机分区镜像

`boot0`、`boot1`、`system0`、`system1` 的 `.img` 是从**本机读出的分区**，放在 `private/inputs/readbacks/`，不随仓库发布：

- 厂商没有公开整机镜像的下载地址；拿到完整分区需要一台同型号设备并读取 NAND。
- 复现我们的分析只需要 rootfs，`LX06_1.94.13.squashfs` 已覆盖；`system0` 的内容与之对应。
- 分区回读可能带设备特定状态，不适合公开。

如果需要复现刷机流程，使用上游的补丁说明与 squashfs，或自行从设备读取；本仓库不提供整机镜像。

## 解包

```powershell
# 需要 unsquashfs 或 7zip 的 squashfs 支持
unsquashfs -d temp/rootfs-factory-system0 LX06_1.94.13.squashfs
unsquashfs -d temp/rootfs-patched LX06_1.94.13_patched.squashfs
```

解包产物属于可重建内容，统一放在 `temp/`，不提交。随后：

```powershell
powershell -NoProfile -File tools/Compare-Rootfs.ps1
tools\python-analysis\python.exe tools\analyze-native-wakeup.py
```

## 许可

固件版权归小米；上游源码遵循其仓库许可。这里保存的是社区已公开发布的镜像副本，仅用于兼容性分析与个人设备维护；对外分发前请自行确认许可与当地法律。
