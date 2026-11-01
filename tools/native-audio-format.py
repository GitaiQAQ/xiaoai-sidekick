from pathlib import Path
from elftools.elf.elffile import ELFFile
from capstone import Cs,CS_ARCH_ARM,CS_MODE_ARM
from capstone.arm import ARM_INS_LDR,ARM_OP_MEM,ARM_REG_PC
import json
r=Path(__file__).resolve().parents[1];o=r/'temp/analysis'
labels=json.loads((o/'usr_lib_libxaudio_engine.so-address-labels.json').read_text())
wanted={'AudioRecord_Init','AudioRecord_Read','AudioRecord_ReadEx','AudioRecord_get_FrameBufferLengthInByte',
        'xaudio_recorder_thread','xaudio_dnn_thread','xaudio_wakeup_thread','xaudio_vad_thread',
        'create_xaudio','init_system','notify_asr_length'}
with (r/'temp/rootfs-factory-system0/usr/lib/libxaudio_engine.so').open('rb') as f:
    e=ELFFile(f)
    sections=[(s['sh_addr'],s.data()) for s in e.iter_sections() if s['sh_type']!='SHT_NOBITS' and s['sh_flags']&2]
    def read(a,n):
        for start,b in sections:
            if start<=a<start+len(b): return b[a-start:a-start+n]
        return b''
    out=[]
    for s in e.get_section_by_name('.dynsym').iter_symbols():
        if s.name not in wanted or not s['st_size']:continue
        a=s['st_value'];md=Cs(CS_ARCH_ARM,CS_MODE_ARM);md.detail=True;md.skipdata=True
        out.append(f'\nFUNCTION {s.name} @{a:08x} size={s["st_size"]}')
        for i in md.disasm(read(a,s['st_size']),a):
            if not i.id:continue
            note=''
            if i.mnemonic in ('b','bl','blx') and i.op_str.startswith('#0x'):
                note=labels.get(i.op_str[1:],'')
            if i.id==ARM_INS_LDR and len(i.operands)==2 and i.operands[1].type==ARM_OP_MEM and i.operands[1].mem.base==ARM_REG_PC:
                raw=read(i.address+8+i.operands[1].mem.disp,4)
                if len(raw)==4:
                    val=int.from_bytes(raw,'little');note=hex(val)
                    text=read(val,120).split(b'\0')[0]
                    if text and all(32<=x<127 for x in text):note+=' '+repr(text.decode())
            out.append(f'{i.address:08x} {i.mnemonic:8s} {i.op_str:38s} ; {note}')
    (o/'native-audio-format-disassembly.txt').write_text('\n'.join(out),encoding='utf-8')
    print('audio format disassembly lines',len(out))
