from pathlib import Path
from elftools.elf.elffile import ELFFile
from capstone import *
import json,sys
R=Path(__file__).resolve().parents[1];name=sys.argv[1];wanted=set(sys.argv[2:]);O=R/'temp/analysis'
labels=json.loads((O/(name.replace('/','_')+'-address-labels.json')).read_text())
with (R/'temp/rootfs-factory-system0'/name).open('rb') as f:
 e=ELFFile(f);out=[]
 for s in e.get_section_by_name('.dynsym').iter_symbols():
  if s.name not in wanted or not isinstance(s['st_shndx'],int):continue
  sec=e.get_section(s['st_shndx']);a=s['st_value']&~1;blob=sec.data()[a-sec['sh_addr']:a-sec['sh_addr']+s['st_size']]
  md=Cs(CS_ARCH_ARM,CS_MODE_THUMB if s['st_value']&1 else CS_MODE_ARM);md.skipdata=True
  out.append(f'FUNCTION {s.name} @{a:08x}')
  for i in md.disasm(blob,a):
   note=''
   if i.mnemonic in ('bl','b','blx') and i.op_str.startswith('#0x'):note=labels.get(i.op_str[1:],'')
   out.append(f'{i.address:08x} {i.mnemonic:8s} {i.op_str:35s} ; {note}')
 (O/(name.replace('/','_')+'-focused.txt')).write_text('\n'.join(out),encoding='utf-8')
