from pathlib import Path
import json,re
from elftools.elf.elffile import ELFFile
from elftools.elf.sections import SymbolTableSection
from capstone import Cs, CS_ARCH_ARM, CS_MODE_ARM, CS_MODE_THUMB, CS_MODE_LITTLE_ENDIAN
ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'temp/rootfs-factory-system0'
OUT=ROOT/'temp/analysis'
FILES=['usr/bin/mipns-xiaomi','usr/bin/pns_ubus_helper','usr/lib/libxaudio_engine.so','usr/bin/mico_aivs_lab']
pattern=re.compile(r'wake|wuw|keyword|callback|xaudio_init|xaudio_process|event_notify|aec|speech_message',re.I)
for name in FILES:
    with (BASE/name).open('rb') as f:
        elf=ELFFile(f)
        symbols=[]
        for section in elf.iter_sections():
            if isinstance(section,SymbolTableSection):
                for s in section.iter_symbols():
                    if s.name:
                        symbols.append(dict(name=s.name,address=s['st_value'],size=s['st_size'],kind=s['st_info']['type'],binding=s['st_info']['bind'],section=s['st_shndx']))
        tag=name.replace('/','_')
        (OUT/(tag+'-symbols.json')).write_text(json.dumps(symbols,indent=2),encoding='utf-8')
        selected=[s for s in symbols if pattern.search(s['name'])]
        (OUT/(tag+'-wake-symbols.txt')).write_text('\n'.join(f"{s['address']:08x} {s['size']:6d} {str(s['section']):>5} {s['name']}" for s in selected),encoding='utf-8')
        disasm=[]
        for s in selected:
            if s['kind']!='STT_FUNC' or s['section']=='SHN_UNDEF' or not s['size'] or s['size']>12000:continue
            addr=s['address'] & ~1
            section=elf.get_section(s['section'])
            offset=addr-section['sh_addr']
            data=section.data()[offset:offset+s['size']]
            md=Cs(CS_ARCH_ARM,(CS_MODE_THUMB if s['address']&1 else CS_MODE_ARM)|CS_MODE_LITTLE_ENDIAN)
            disasm.append('\nFUNCTION '+s['name']+f' @{addr:08x}')
            for i in md.disasm(data,addr):disasm.append(f'{i.address:08x}  {i.bytes.hex():12s} {i.mnemonic:8s} {i.op_str}')
        (OUT/(tag+'-wake-disassembly.txt')).write_text('\n'.join(disasm),encoding='utf-8')
        print(name,len(symbols),'symbols;',len(selected),'wake-related')
