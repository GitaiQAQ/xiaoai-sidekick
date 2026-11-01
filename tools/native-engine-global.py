from pathlib import Path
from elftools.elf.elffile import ELFFile
R=Path(__file__).resolve().parents[1]
with (R/'temp/rootfs-factory-system0/usr/lib/libxaudio_engine.so').open('rb') as f:
 e=ELFFile(f);rel=e.get_section_by_name('.rel.dyn');syms=e.get_section(rel['sh_link'])
 for r in rel.iter_relocations():
  if r['r_offset']==0x425120:
   s=syms.get_symbol(r['r_info_sym']);print('engine global GOT',hex(r['r_offset']),s.name,'ELF address',hex(s['st_value']),'size',s['st_size'])
