from pathlib import Path
import json,struct
r=Path(__file__).resolve().parents[1]
b=(r/'private/inputs/live/live-asr-cache-callbacks.bin').read_bytes()
if len(b)!=16:raise RuntimeError('Expected exactly 16 bytes')
v=struct.unpack('<4I',b)
d={'cache_elf_va':'0x4253f0','fields':{'callback_offset_0x18':hex(v[0]),'callback_offset_0x1c':hex(v[1]),'context_offset_0x20':hex(v[2]),'enable_offset_0x24':v[3]},'callback_call_elf_va':'0x1eaec','callback_bytes_at_call':1920}
(r/'private/inputs/live/live-asr-cache-callbacks.json').write_text(json.dumps(d,indent=2),encoding='utf-8');print(json.dumps(d,indent=2))
