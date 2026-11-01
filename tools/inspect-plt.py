from pathlib import Path
from elftools.elf.elffile import ELFFile
from capstone import *
from capstone.arm import *
p=Path(__file__).resolve().parents[1]
with (p/'temp/rootfs-factory-system0/usr/lib/libxaudio_engine.so').open('rb') as f:
 e=ELFFile(f);s=e.get_section_by_name('.plt');m=Cs(CS_ARCH_ARM,CS_MODE_ARM);m.detail=True
 for i in list(m.disasm(s.data(),s['sh_addr']))[:32]:print(hex(i.address),i.mnemonic,i.op_str)
