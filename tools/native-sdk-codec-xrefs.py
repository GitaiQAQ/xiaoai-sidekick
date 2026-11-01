from pathlib import Path
from elftools.elf.elffile import ELFFile
from capstone import Cs, CS_ARCH_ARM, CS_MODE_ARM
from capstone.arm import ARM_INS_LDR, ARM_OP_MEM, ARM_REG_PC
import json

r=Path(__file__).resolve().parents[1]
o=r/'temp/analysis'
with (r/'temp/rootfs-factory-system0/usr/lib/libaivs_sdk.so').open('rb') as f:
    e=ELFFile(f)
    base=next(t.entry.d_val for t in e.get_section_by_name('.dynamic').iter_tags()
              if t.entry.d_tag=='DT_PLTGOT')
    refs={}
    for sec in e.iter_sections():
        if sec['sh_type'] not in ('SHT_REL','SHT_RELA'): continue
        syms=e.get_section(sec['sh_link'])
        for rel in sec.iter_relocations():
            name=syms.get_symbol(rel['r_info_sym']).name
            if 'AivsConfig3Asr' in name and any(x in name for x in ['CODEC','OPUS','ENCODED']):
                refs[rel['r_offset']-base]=name
    text=e.get_section_by_name('.text'); data=text.data(); start=text['sh_addr']
    sections=[(s['sh_addr'],s.data()) for s in e.iter_sections()
              if s['sh_type']!='SHT_NOBITS' and s['sh_flags']&2]
    def read(addr,n):
        for a,b in sections:
            if a<=addr and addr+n<=a+len(b): return b[addr-a:addr-a+n]
        return b''
    md=Cs(CS_ARCH_ARM,CS_MODE_ARM);md.detail=True;md.skipdata=True
    out=[]
    for i in md.disasm(data,start):
        if i.id!=ARM_INS_LDR: continue
        ops=i.operands
        if len(ops)!=2 or ops[1].type!=ARM_OP_MEM or ops[1].mem.base!=ARM_REG_PC: continue
        lit=i.address+8+ops[1].mem.disp; raw=read(lit,4)
        val=int.from_bytes(raw,'little') if len(raw)==4 else -1
        if val in refs:
            out.append(dict(instruction=hex(i.address),literal=hex(lit),got_offset=hex(val),
                            symbol=refs[val],note='GOT offset literal; inspect following load and base register.'))
    (o/'sdk-codec-got-xrefs.json').write_text(json.dumps(out,indent=2),encoding='utf-8')
    lines=[]
    lite=Cs(CS_ARCH_ARM,CS_MODE_ARM)
    for x in out:
        if 'ENCODED' not in x['symbol'] and 'OPUS' not in x['symbol']: continue
        a=int(x['instruction'],16)
        lines.append('\n'+json.dumps(x))
        for i in lite.disasm(read(a-12,112),a-12): lines.append(f'{i.address:08x} {i.mnemonic:8s} {i.op_str}')
    (o/'sdk-encoder-config-xrefs.txt').write_text('\n'.join(lines),encoding='utf-8')
    print('codec GOT-literal xrefs',len(out))
