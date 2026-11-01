from pathlib import Path
from elftools.elf.elffile import ELFFile
import struct,json,re
R=Path(__file__).resolve().parents[1];OUT=R/'temp/analysis'
f=(R/'temp/rootfs-factory-system0/usr/bin/mipns-xiaomi').open('rb'); e=ELFFile(f)
def data(addr,n):
 for s in e.iter_sections():
  if s['sh_type']!='SHT_NOBITS' and s['sh_addr']<=addr<s['sh_addr']+s['sh_size']:return s.data()[addr-s['sh_addr']:addr-s['sh_addr']+n]
 raise ValueError(hex(addr))
def u(a):return struct.unpack('<I',data(a,4))[0]
lines=(OUT/'usr_bin_mipns-xiaomi-annotated.txt').read_text(encoding='utf-8').splitlines()
addresses=[int(line[:8],16) if re.match(r'^[0-9a-f]{8} ',line) else None for line in lines]
cases=[{'downward_type':n,'target':hex(u(0x140dc+n*4))} for n in range(11)]
output=[]
for case in cases:
 a=int(case['target'],16);output.append(f"\nDOWNWARD_TYPE={case['downward_type']} TARGET={a:08x}")
 start=next(i for i,x in enumerate(addresses) if x==a)
 output.extend(lines[start:start+38])
(OUT/'downward-dispatch-cases.txt').write_text('\n'.join(output),encoding='utf-8')
(OUT/'downward-dispatch-table.json').write_text(json.dumps(cases,indent=2),encoding='utf-8')
# Save exact bounded excerpts around wire packet construction and send helper.
regions=[(0x14768,0x148d8),(0x14964,0x149d0),(0x14c0c,0x14c60),(0x14e84,0x14ecc),(0x15110,0x15164),(0x15404,0x15494),(0x156cc,0x1571c),(0x1593c,0x15984),(0x15bc4,0x15bf4)]
output=[]
for lo,hi in regions:
 output.append(f'\nREGION {lo:08x}-{hi:08x}')
 output.extend(line for line,a in zip(lines,addresses) if a is not None and lo<=a<hi)
(OUT/'wire-builder-excerpts.txt').write_text('\n'.join(output),encoding='utf-8')
print(json.dumps(cases))
print('\n'.join((OUT/'downward-dispatch-cases.txt').read_text().splitlines()[:42]))
