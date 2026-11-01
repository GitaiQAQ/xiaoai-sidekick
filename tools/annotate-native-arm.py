from pathlib import Path
import re,json,sys
from elftools.elf.elffile import ELFFile
from capstone import *
from capstone.arm import *
ROOT=Path(__file__).resolve().parents[1]; OUT=ROOT/'temp/analysis'
for file in (sys.argv[1:] or ['usr/lib/libxaudio_engine.so','usr/bin/mipns-xiaomi','usr/bin/pns_ubus_helper','usr/lib/libmdspeech.so']):
 with (ROOT/'temp/rootfs-factory-system0'/file).open('rb') as f:
  elf=ELFFile(f); symbols={}; wanted=[]
  for sec in elf.iter_sections():
   if sec['sh_type'] in ('SHT_SYMTAB','SHT_DYNSYM'):
    for s in sec.iter_symbols():
     if s['st_value']:symbols[s['st_value']&~1]=s.name
     if s['st_info']['type']=='STT_FUNC' and s['st_size'] and re.search(r'wake|wuw|keyword|callback|Paddy|Init|Process|RiceLoadModelBin|mdspeech_|create_xaudio|init_system',s.name,re.I):wanted.append(s)
  plt=elf.get_section_by_name('.plt'); rel=elf.get_section_by_name('.rel.plt')
  if plt and rel:
   syms=elf.get_section(rel['sh_link'])
   relocations={r['r_offset']:syms.get_symbol(r['r_info_sym']).name for r in rel.iter_relocations()}
   pm=Cs(CS_ARCH_ARM,CS_MODE_ARM);pm.detail=True;pm.skipdata=True; ip=None; entry=None
   for i in pm.disasm(plt.data(),plt['sh_addr']):
    if i.id==0:continue
    ops=i.operands
    if i.id==ARM_INS_ADD and len(ops)>=3 and ops[0].reg==ARM_REG_R12:
     word=int.from_bytes(i.bytes,'little'); imm=word&255; rot=((word>>8)&15)*2
     imm=((imm>>rot)|(imm<<(32-rot)))&0xffffffff if rot else imm
     if ops[1].reg==ARM_REG_PC:ip=i.address+8+imm;entry=i.address
     elif ops[1].reg==ARM_REG_R12 and ip is not None:ip=(ip+imm)&0xffffffff
    elif i.id==ARM_INS_LDR and len(ops)==2 and ops[0].reg==ARM_REG_PC and ops[1].mem.base==ARM_REG_R12 and ip is not None:
     got=ip+ops[1].mem.disp
     if got in relocations:symbols[entry]=relocations[got]+'@plt'
     ip=None
  sections=[(sec['sh_addr'],sec.data()) for sec in elf.iter_sections() if sec['sh_type']!='SHT_NOBITS' and sec['sh_flags']&2]
  def vbytes(addr,n):
   for start,data in sections:
    if start<=addr<start+len(data):
     return data[addr-start:addr-start+n]
   return b''
  def describe(addr):
   if addr in symbols:return symbols[addr]
   data=vbytes(addr,140).split(b'\0')[0]
   if len(data)>4 and all(32<=b<127 for b in data):return repr(data.decode())
   return ''
  if 'libxaudio' not in file and 'libmdspeech' not in file:
   text=elf.get_section_by_name('.text'); spans=[('TEXT',text['sh_addr'],text.data(),False)]
  else:
   spans=[]
   for s in wanted:
    if isinstance(s['st_shndx'],int):spans.append((s.name,s['st_value']&~1,vbytes(s['st_value']&~1,s['st_size']),bool(s['st_value']&1)))
  output=[]
  for name,addr,blob,thumb in spans:
   md=Cs(CS_ARCH_ARM,CS_MODE_THUMB if thumb else CS_MODE_ARM);md.detail=True; md.skipdata=True; regs={}
   output.append('\nFUNCTION '+name+f' @{addr:08x}')
   for ins in md.disasm(blob,addr):
    
    if ins.id==0:continue
    note=''; ops=ins.operands; pc=ins.address+(4 if thumb else 8)
    if ins.mnemonic in ('bl','b','blx') and ops and ops[0].type==ARM_OP_IMM:note=describe(ops[0].imm)
    try:
     if ins.id==ARM_INS_LDR and len(ops)==2 and ops[0].type==ARM_OP_REG and ops[1].type==ARM_OP_MEM and ops[1].mem.base==ARM_REG_PC:
      data=vbytes(pc+ops[1].mem.disp,4)
      if len(data)==4:regs[ops[0].reg]=int.from_bytes(data,'little');note=hex(regs[ops[0].reg])+' '+describe(regs[ops[0].reg])
     elif ins.id==ARM_INS_ADD and len(ops)==3 and ops[0].type==ARM_OP_REG:
      a=pc if ops[1].type==ARM_OP_REG and ops[1].reg==ARM_REG_PC else regs.get(ops[1].reg) if ops[1].type==ARM_OP_REG else None
      b=ops[2].imm if ops[2].type==ARM_OP_IMM else regs.get(ops[2].reg) if ops[2].type==ARM_OP_REG else None
      if a is not None and b is not None:regs[ops[0].reg]=(a+b)&0xffffffff;note=hex(regs[ops[0].reg])+' '+describe(regs[ops[0].reg])
     elif ins.id in (ARM_INS_MOV,ARM_INS_MOVW) and len(ops)==2 and ops[1].type==ARM_OP_IMM:regs[ops[0].reg]=ops[1].imm
     elif ins.id==ARM_INS_MOVT and len(ops)==2 and ops[1].type==ARM_OP_IMM:regs[ops[0].reg]=(regs.get(ops[0].reg,0)&65535)|(ops[1].imm<<16)
    except (IndexError,KeyError):pass
    output.append(f'{ins.address:08x} {ins.mnemonic:8s} {ins.op_str:38s} ; {note}')
  tag=file.replace('/','_');(OUT/(tag+'-annotated.txt')).write_text('\n'.join(output),encoding='utf-8')
  (OUT/(tag+'-address-labels.json')).write_text(json.dumps({hex(k):v for k,v in symbols.items()},indent=2),encoding='utf-8')
  print(file,'PLT',hex(plt['sh_addr']) if plt else None,plt['sh_size'] if plt else None,'relocs',rel.num_relocations() if rel else None)







