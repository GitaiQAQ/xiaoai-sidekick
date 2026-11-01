from pathlib import Path
from elftools.elf.elffile import ELFFile
import re,json
R=Path(__file__).resolve().parents[1];B=R/'temp/rootfs-factory-system0';O=R/'temp/analysis';rows=[]
for name in ['usr/lib/libasound.so.2.0.0','usr/lib/libxaudio_engine.so','usr/lib/libmdspeech.so']:
 with (B/name).open('rb') as f:
  e=ELFFile(f);dyn=e.get_section_by_name('.dynsym')
  for s in dyn.iter_symbols():
   if re.search('opus|encod|codec|asr|mdspeech|record_data',s.name,re.I):rows.append(dict(file=name,name=s.name,address=hex(s['st_value']),size=s['st_size'],defined=s['st_shndx']!='SHN_UNDEF'))
  for t in e.get_section_by_name('.dynamic').iter_tags():
   if t.entry.d_tag=='DT_NEEDED':rows.append(dict(file=name,dependency=t.needed))
(O/'native-codec-symbols.json').write_text(json.dumps(rows,indent=2),encoding='utf-8')
for r in rows:
 if 'dependency'in r or 'opus'in r.get('name','').lower() or re.search('codec|encoder',r.get('name',''),re.I):print(r)
