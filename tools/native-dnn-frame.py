"""Dump the AFE DNN queue/frame functions from the factory libxaudio_engine.so.

Read-only: every input is the extracted factory image under temp/. Output is
written next to the other wakeup analysis artifacts so the claims in
native-dnn-frame-layout.md can be re-checked from the same bytes.
"""
import json
from pathlib import Path

from capstone import Cs, CS_ARCH_ARM, CS_MODE_ARM
from capstone.arm import ARM_INS_LDR, ARM_OP_MEM, ARM_REG_PC
from elftools.elf.elffile import ELFFile

root = Path(__file__).resolve().parents[1]
out = root / 'temp/analysis'
labels = json.loads((out / 'usr_lib_libxaudio_engine.so-address-labels.json').read_text())
wanted = {
    'AFE_DnnProcessPostAgc', 'DnnWeProcessPostAgc_pass', 'DnnWeProcessPostAgc',
    'xaudio_wrapper_dnn', 'xaudio_create_dnn', 'xaudio_wrapper_dnn_init',
    'Xaudio_CreateElementQueue_s', 'Xaudio_ElementEnqueue_s',
    'Xaudio_ElementDequeue_s', 'Xaudio_QueryElementQueue_s',
    'xaudio_wakeup_thread', 'cache_data_save',
}
# 0x51c1c creates the two 0x500-element queues and the 0x200-byte scratch buffer.
anon = [(0x51c1c, 0xd4, 'dnn_postagc_object_ctor')]

with (root / 'temp/rootfs-factory-system0/usr/lib/libxaudio_engine.so').open('rb') as f:
    e = ELFFile(f)
    sections = [(s['sh_addr'], s.data()) for s in e.iter_sections()
                if s['sh_type'] != 'SHT_NOBITS' and s['sh_flags'] & 2]

    def read(a, n):
        for start, b in sections:
            if start <= a < start + len(b):
                return b[a - start:a - start + n]
        return b''

    regions = [(s['st_value'], s['st_size'], s.name)
               for s in e.get_section_by_name('.dynsym').iter_symbols()
               if s.name in wanted and s['st_size']] + anon
    regions.sort()
    lines = []
    for addr, size, name in regions:
        lines.append(f'\nFUNCTION {name} @{addr:08x} size={size}')
        md = Cs(CS_ARCH_ARM, CS_MODE_ARM)
        md.detail = True
        md.skipdata = True
        for i in md.disasm(read(addr, size), addr):
            if not i.id:
                continue
            note = ''
            if i.mnemonic in ('b', 'bl', 'blx') and i.op_str.startswith('#0x'):
                note = labels.get(i.op_str[1:], '')
            if (i.id == ARM_INS_LDR and len(i.operands) == 2
                    and i.operands[1].type == ARM_OP_MEM
                    and i.operands[1].mem.base == ARM_REG_PC):
                raw = read(i.address + 8 + i.operands[1].mem.disp, 4)
                if len(raw) == 4:
                    val = int.from_bytes(raw, 'little')
                    note = hex(val)
                    text = read(val, 96).split(b'\0')[0]
                    if text and all(32 <= x < 127 for x in text):
                        note += ' ' + repr(text.decode())
            lines.append(f'{i.address:08x} {i.mnemonic:8s} {i.op_str:38s} ; {note}')
    (out / 'native-dnn-frame-disassembly.txt').write_text('\n'.join(lines), encoding='utf-8')
    print('dnn frame disassembly lines', len(lines))
