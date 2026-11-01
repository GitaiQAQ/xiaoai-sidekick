from pathlib import Path
from elftools.elf.elffile import ELFFile
r=Path(__file__).resolve().parents[1]
with (r/'temp/rootfs-factory-system0/usr/lib/libxaudio_engine.so').open('rb') as f:
 e=ELFFile(f)
 for s in e.get_section_by_name('.dynsym').iter_symbols():
  if s.name=='mdata_wakeup_mobile':
   sec=e.get_section(s['st_shndx']);b=sec.data()[s['st_value']-sec['sh_addr']:s['st_value']-sec['sh_addr']+700]
   for i in range(0,len(b),32):print(hex(i),b[i:i+32].hex(),repr(b[i:i+32]))
