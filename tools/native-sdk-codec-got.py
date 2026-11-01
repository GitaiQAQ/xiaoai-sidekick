from pathlib import Path
from elftools.elf.elffile import ELFFile
import json
r=Path(__file__).resolve().parents[1]
with (r/'temp/rootfs-factory-system0/usr/lib/libaivs_sdk.so').open('rb') as f:
 e=ELFFile(f);base=next(t.entry.d_val for t in e.get_section_by_name('.dynamic').iter_tags() if t.entry.d_tag=='DT_PLTGOT');out={'got_base':hex(base),'references':[]}
 for sec in e.iter_sections():
  if sec['sh_type'] not in ('SHT_REL','SHT_RELA'):continue
  syms=e.get_section(sec['sh_link'])
  for rel in sec.iter_relocations():
   if rel['r_offset'] in [base+x for x in [0x178c,0x1040,0x1108,0x1148]]:
    out['references'].append({'address':hex(rel['r_offset']),'offset':hex(rel['r_offset']-base),'symbol':syms.get_symbol(rel['r_info_sym']).name})
 (r/'temp/analysis/sdk-codec-got-references.json').write_text(json.dumps(out,indent=2));print(json.dumps(out,indent=2))
