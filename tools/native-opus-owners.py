from pathlib import Path
from elftools.elf.elffile import ELFFile
import re,json
R=Path(__file__).resolve().parents[1];B=R/'temp/rootfs-factory-system0';O=R/'temp/analysis';rows=[]
for path in (B/'usr/lib').glob('*.so*'):
 try:
  with path.open('rb') as f:
   if f.read(4)!=b'\x7fELF':continue
   f.seek(0);e=ELFFile(f);d=e.get_section_by_name('.dynsym')
   if not d:continue
   for s in d.iter_symbols():
    if re.search('opus_encoder|opus_encode$|opus_decoder|Codec.*[Ee]ncod|[Aa]udio[Ee]ncoder',s.name):rows.append(dict(file=str(path.relative_to(B)),name=s.name,address=hex(s['st_value']),size=s['st_size'],defined=s['st_shndx']!='SHN_UNDEF'))
 except Exception:pass
(O/'opus-symbol-owners.json').write_text(json.dumps(rows,indent=2),encoding='utf-8')
for r in rows[:60]:print(r)
