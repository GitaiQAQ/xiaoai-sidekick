# Reference: 本地凭据

将两类接口凭据填写在 `private/credentials.json`。该文件已加入 Git 忽略，不放入报告、日志或聊天；不会覆盖已有私有文件。`private/` 整目录不发布。

模板（空值示例，原 `config/credentials.example.json` 已并入本文件）：

```json
{
  "schema_version": 1,
  "ark": {
    "api_key": "",
    "model": ""
  },
  "realtime": {
    "api_key": ""
  }
}
```

- `ark.api_key`：方舟模型API Key，只填原始Key。
- `ark.model`：控制台可调用的模型ID或推理接入点ID，例如实际的ep编号；不能用聊天产品显示名代替。
- `realtime.api_key`：豆包语音控制台的语音API Key，与方舟Key分开填写。

当前实时实现主线选择官方Seeduplex3.0新Realtime事件协议，端点`wss://openspeech.bytedance.com/api/v3/duplex/realtime/dialogue`。如果现有控制台只有AppID/AccessKey/AppKey，或只开通旧版实时语音，请说明鉴权/服务版本，不能把这些字段直接当成API Key。旧版与3.0协议不同，需明确适配。

`tools/Check-LocalCredentials.py`只输出字段存在/格式标志，不输出Key、模型值或文件内容；它不验证云端权限。后续认证测试先用固定短文本并限制输出，不开启麦克风或上传环境音，实时语音测试另按实际接口实现。不要把私有文件整段粘贴到工具日志。

2026-11-01 13:40 状态：`realtime.api_key` 已填写并通过真实云端验证（101 升级 + session.created + session.closed），`ark` 段仍为空。检查脚本只输出标志。该 Key 曾直接出现在聊天记录中，建议验证后到控制台轮换；轮换只改私有文件即可。
