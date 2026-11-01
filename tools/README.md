# Tools

生成、部署、检查与逆向脚本。脚本是材料可再生产的一部分：`temp/` 里的产物都能在这里找到生成器。

## 目录

- `local/`：设备 SSH 凭据（`ssh-askpass.cmd`）与 `known_hosts`。**不发布**，已在 `.gitignore` 中排除。
- 本目录只放脚本与说明；第三方工具链在 `vendor/`（Zig、嵌入式 Python、7zip、刷机工具、下载包），整棵目录不入库，按下面的“工具链恢复”重建。
- 脚本可随仓库保存；公开前按下面的检查清单处理设备地址与凭据。

## 工具链恢复

```powershell
# 构建：Zig 0.13.0
Expand-Archive vendor\downloads\zig-windows-x86_64-0.13.0.zip -DestinationPath vendor\zig-portable -Force

# 分析：嵌入式 Python + capstone/pyelftools
Expand-Archive vendor\downloads\python-3.12.8-embed-amd64.zip -DestinationPath vendor\python-analysis -Force
# 把两个 wheel 解压到 vendor\python-analysis\Lib\site-packages\
Expand-Archive vendor\downloads\capstone-5.0.3-py3-none-win_amd64.whl -DestinationPath vendor\python-analysis\Lib\site-packages -Force
Expand-Archive vendor\downloads\pyelftools-0.31-py3-none-any.whl -DestinationPath vendor\python-analysis\Lib\site-packages -Force
```

版本取自 `temp/analysis/capstone-package.json` 与 `pyelftools-package.json`。`7zip-portable` 与刷机工具只用于解包/刷机，构建与分析不需要。

## 脚本分类

| 类别 | 脚本 | 用途 |
|---|---|---|
| 输入获取 | `Fetch-Inputs.ps1` | 下载公开固件 squashfs 并按记录的 SHA256 校验 |
| 构建 | `Build-NativeGlibc.ps1`、`Build-NativeProbe.ps1` | ARM/glibc 原生程序构建与 ELF 检查 |
| 原厂唤醒取证 | `Build-WakeProbes.ps1`、`native-wake-dependencies.py` | 构建固定字段只读探针与隔离引擎探针；核对固件导入符号所有者。引擎探针须经原厂 loader 运行，未接生产服务 |
| 设备操作 | `Invoke-LX06.ps1`、`Manage-LX06Service.ps1`、`Manage-LX06Web.ps1`、`Invoke-DoubaoSmoke.ps1`、`Invoke-ArkSmoke.ps1` | SSH 执行、语音/Web 服务安装与启动、实时会话冒烟 |
| 唤醒 | `kws-build-templates.py`、`kws-tune.ps1`、`Check-KwsReplay.py` | 模板生成、阈值调优、回放评分 |
| 分析 | `analyze-native-wakeup.py`、`annotate-native-arm.py`、`dump-native-functions.py`、`native-*.py` | 从 `temp/rootfs-*` 提取符号、反汇编、协议/模型证据，写入 `temp/analysis/` |
| 对比 | `Compare-Rootfs.ps1` | 对比原厂与补丁根文件系统，写 `temp/manifests/` |
| 检查 | `Check-LocalCredentials.py`、`Check-WebConsole.py`、`Check-NeteaseCharts.py` | 凭据存在性（不打印值）、控制台与音乐接口回归 |

分析脚本用仓库内的嵌入式 Python 运行（含 capstone/pyelftools）：

```powershell
vendor\python-analysis\python.exe tools\analyze-native-wakeup.py
```

## 输入与输出

| 脚本 | 读 | 写 |
|---|---|---|
| `native-*.py` | `temp/rootfs-factory-system0/`、`private/inputs/live/` | `temp/analysis/` |
| `Compare-Rootfs.ps1` | `temp/rootfs-factory-system0/`、`rootfs-patched/` | `temp/manifests/` |
| `kws-build-templates.py` | 录音与构建产物 | 模板头文件 / 分析输出 |
| `Check-*.py` | 设备与本地配置 | 控制台输出，不落盘 |

`live-*.bin` 是输入，`live-*.json` 是从它们解析出的派生结果；后者在 `temp/analysis/`。

## 发布检查

- `tools/local/` 永不发布。
- 其他脚本含内网地址 `192.168.100.210`、设备路径 `/data/lx06-agent/`；公开版本应把这些提为参数或示例值。
- `Invoke-*.ps1` 从 `private/credentials.json` 或 stdin 读取凭据，不要在脚本里新增明文。
- 构建脚本引用 `tools/zig-portable`、`vendor/python-analysis`；这两个目录体积较大，公开仓库可选择只保留下载说明。

## 约定

1. 新脚本读写 `rom/`、`private/`、`temp/` 的路径统一从仓库根解析，避免相对路径随 cwd 失效。
2. 只读诊断不要改设备状态；写入类脚本先备份，并在报告里记录哈希与回滚位置。
3. 生成脚本保持 verify-don't-generate 风格：校验类脚本只报告，不改写产物。
