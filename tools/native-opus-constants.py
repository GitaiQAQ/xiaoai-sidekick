from pathlib import Path
from elftools.elf.elffile import ELFFile
import json,struct
R=Path(__file__).resolve().parents[1];O=R/'temp/analysis';result=[]
with (R/'temp/rootfs-factory-system0/usr/lib/libaivs_sdk.so').open('rb') as f:
 e=ELFFile(f)
 for s in e.get_section_by_name('.dynsym').iter_symbols():
  if 'OPUS_BITRATE_' in s.name and isinstance(s['st_shndx'],int):
   sec=e.get_section(s['st_shndx']);off=s['st_value']-sec['sh_addr'];raw=sec.data()[off:off+s['st_size']]
   result.append(dict(name=s.name,address=hex(s['st_value']),size=s['st_size'],raw=raw.hex(),u32=struct.unpack('<I',raw[:4])[0] if len(raw)>=4 else None))
(O/'native-opus-bitrate-constants.json').write_text(json.dumps(result,indent=2),encoding='utf-8');print(result)
