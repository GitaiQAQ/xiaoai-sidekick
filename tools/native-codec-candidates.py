from pathlib import Path
from elftools.elf.elffile import ELFFile
import re,json
R=Path(__file__).resolve().parents[1];B=R/'temp/rootfs-factory-system0';O=R/'temp/analysis'
output=[]
for name in ['usr/bin/mico_aivs_lab','usr/lib/libaivs_sdk.so','usr/lib/libmico-common.so','usr/lib/libxaudio_engine.so']:
 blob=(B/name).read_bytes(); strings=[x.decode('ascii') for x in re.findall(rb'[\x20-\x7e]{5,}',blob) if re.search(rb'opus|codec|encoder',x,re.I)]
 with (B/name).open('rb') as f:
  e=ELFFile(f);d=e.get_section_by_name('.dynsym');syms=[s.name for s in d.iter_symbols() if re.search('opus|codec|encod',s.name,re.I)]
 output.append({'file':name,'symbols':syms,'strings':strings})
 print(name)
 for s in strings[:30]:print(' ',s[:160])
(O/'native-codec-candidates.json').write_text(json.dumps(output,indent=2),encoding='utf-8')
