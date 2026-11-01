from pathlib import Path
from elftools.elf.elffile import ELFFile
from capstone import *
from capstone.arm import *
import json,struct
R=Path(__file__).resolve().parents[1];O=R/'temp/analysis'
f=(R/'temp/rootfs-factory-system0/usr/bin/mipns-xiaomi').open('rb');e=ELFFile(f)
def data(a,n):
 for s in e.iter_sections():
  if s['sh_type']!='SHT_NOBITS' and s['sh_addr']<=a<s['sh_addr']+s['sh_size']:return s.data()[a-s['sh_addr']:a-s['sh_addr']+n]
 return b''
md=Cs(CS_ARCH_ARM,CS_MODE_ARM);md.detail=True
cases=json.loads((O/'downward-dispatch-table.json').read_text())
for i,c in enumerate(cases):
 lo=int(c['target'],16);hi=int(cases[i+1]['target'],16) if i+1<len(cases) else 0x14654;notes=[]
 for ins in md.disasm(data(lo,hi-lo),lo):
  ops=ins.operands
  if ins.id==ARM_INS_LDR and len(ops)==2 and ops[1].type==ARM_OP_MEM and ops[1].mem.base==ARM_REG_PC:
   raw=data(ins.address+8+ops[1].mem.disp,4)
   if len(raw)!=4:continue
   ptr=struct.unpack('<I',raw)[0];raw=data(ptr,800).split(b'\0')[0]
   try:txt=raw.decode('utf-8')
   except UnicodeDecodeError:continue
   if len(txt)>6 and all(ch.isprintable() or ch in '\n\r\t' for ch in txt):notes.append(txt)
 c['strings']=notes
 print('TYPE',c['downward_type'],c['target'])
 for note in notes:
  if 'recv' in note or 'response' in note or 'stream' in note or 'speech' in note or 'finish' in note or 'tts' in note or 'enable' in note or 'asr' in note:print(' ',repr(note))
(O/'downward-dispatch-table.json').write_text(json.dumps(cases,ensure_ascii=False,indent=2),encoding='utf-8')
