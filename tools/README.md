# Tools

生成、部署、检查与逆向脚本。脚本是材料可再生产的一部分：`temp/` 里的产物都能在这里找到生成器。

## 目录

- `local/`：设备 SSH 凭据（`ssh-askpass.cmd`）与 `known_hosts`。**不发布**，已在 `.gitignore` 中排除。
- 构建与分析工具安装在系统中，脚本从 PATH 查找 Zig 与 Python。curl 7.55.1 头文件是 ARM 交叉编译所需的源码接口，保存在 `third_party/curl-7.55.1/` 并附许可证。
- 脚本可随仓库保存；公开前按下面的检查清单处理设备地址与凭据。

## 系统工具安装

```powershell
# 构建：项目脚本使用 Zig 0.13.0
winget install --id zig.zig --exact --version 0.13.0

# 分析：系统 Python 与 Python 模块
winget install --id Python.Python.3.12 --exact --version 3.12.10
python -m pip install --user capstone==5.0.3 pyelftools==0.31

# 可选：解包归档
winget install --id 7zip.7zip --exact
```

`capstone` 与 `pyelftools` 版本取自 `temp/analysis/capstone-package.json` 和 `pyelftools-package.json`。7-Zip 仅用于解包，不是构建依赖；专用刷机工具也不参与构建或分析。Git 已由系统安装（`Git.Git`），无需重复安装。项目不再使用 `vendor/` 目录。

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

分析脚本使用 PATH 中的系统 Python（含 capstone/pyelftools）：

```powershell
python tools\analyze-native-wakeup.py
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
- 构建脚本从 PATH 查找 Zig 与 Python；curl 头文件使用 `third_party/curl-7.55.1/include`。

## 约定

1. 新脚本读写 `rom/`、`private/`、`temp/` 的路径统一从仓库根解析，避免相对路径随 cwd 失效。
2. 只读诊断不要改设备状态；写入类脚本先备份，并在报告里记录哈希与回滚位置。
3. 生成脚本保持 verify-don't-generate 风格：校验类脚本只报告，不改写产物。
