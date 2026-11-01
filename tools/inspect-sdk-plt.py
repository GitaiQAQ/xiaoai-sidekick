from pathlib import Path
from elftools.elf.elffile import ELFFile
from capstone import *
p=Path(__file__).resolve().parents[1]
with (p/'temp/rootfs-factory-system0/usr/lib/libaivs_sdk.so').open('rb') as f:
 e=ELFFile(f);s=e.get_section_by_name('.plt');print(s['sh_addr'],s['sh_size']);m=Cs(CS_ARCH_ARM,CS_MODE_ARM)
 for i in m.disasm(s.data()[:112],s['sh_addr']):print(hex(i.address),i.mnemonic,i.op_str)
 print('relocations',[(hex(x['r_offset']),x['r_info_sym']) for x in list(e.get_section_by_name('.rel.plt').iter_relocations())[:5]])
