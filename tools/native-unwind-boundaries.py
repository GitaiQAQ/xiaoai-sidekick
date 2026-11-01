from pathlib import Path
from elftools.elf.elffile import ELFFile
import struct,bisect,re,json
R=Path(__file__).resolve().parents[1];O=R/'temp/analysis'
for name,targets in [('usr/bin/mipns-xiaomi',[0x28a64,0x2677c,0x26d80,0x14c58,0x148c4])]:
 with (R/'temp/rootfs-factory-system0'/name).open('rb') as f:
  e=ELFFile(f);sec=e.get_section_by_name('.ARM.exidx');b=sec.data();starts=[]
  for n in range(0,len(b),8):
   v=struct.unpack_from('<I',b,n)[0]&0x7fffffff;v=v-(1<<31) if v&(1<<30) else v
   starts.append(sec['sh_addr']+n+v)
  starts=sorted(starts)
  report=[]; lines=(O/(name.replace('/','_')+'-annotated.txt')).read_text().splitlines()
  for target in targets:
   index=bisect.bisect_right(starts,target)-1;lo=starts[index];hi=starts[index+1] if index+1<len(starts) else target+4
   print(hex(target),'unwind range',hex(lo),hex(hi))
   report.append(f'\nUNWIND RANGE {lo:08x}-{hi:08x} contains {target:08x}')
   report.extend(line for line in lines if re.match('^[0-9a-f]{8} ',line) and lo<=int(line[:8],16)<hi)
  (O/'unwind-focused-functions.txt').write_text('\n'.join(report),encoding='utf-8')
