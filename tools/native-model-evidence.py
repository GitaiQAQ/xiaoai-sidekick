from pathlib import Path
import json,re
from elftools.elf.elffile import ELFFile
R=Path(__file__).resolve().parents[1];B=R/'temp/rootfs-factory-system0';O=R/'temp/analysis'
result={}
with (B/'usr/lib/libxaudio_engine.so').open('rb') as f:
 e=ELFFile(f); d=e.get_section_by_name('.dynsym'); syms={s.name:s for s in d.iter_symbols()}
 for name in ['mdata_wakeup_mobile','mdata_wakeup_mobile_clone']:
  s=syms[name];sec=e.get_section(s['st_shndx']);off=s['st_value']-sec['sh_addr'];blob=sec.data()[off:off+s['st_size']]
  snippets=[]
  for m in re.finditer(rb'--keyword-seq',blob):snippets.append({'relative_offset':m.start(),'nearby_bytes':repr(blob[max(0,m.start()-20):m.start()+110])})
  result[name]={'elf_virtual_address':hex(s['st_value']),'size':s['st_size'],'header_hex':blob[:32].hex(),'keyword_snippets':snippets}
 rel=e.get_section_by_name('.rel.dyn'); linked=e.get_section(rel['sh_link'])
 result['model_got_relocations']=[{'got':hex(r['r_offset']),'symbol':linked.get_symbol(r['r_info_sym']).name} for r in rel.iter_relocations() if r['r_offset'] in [0x424d74,0x4250dc]]
owners={}; wanted={'mdspeech_create','mdspeech_wakeup_info_send','mdspeech_register_alsa_raw_read_callback','mdspeech_data_read','speech_message__upward_message__wakeup_latency_point_message__init'}
for path in list((B/'usr/lib').glob('*.so*'))+list((B/'lib').glob('*.so*')):
 try:
  with path.open('rb') as f:
   if f.read(4)!=b'\x7fELF':continue
   f.seek(0);e=ELFFile(f);dyn=e.get_section_by_name('.dynsym')
   if dyn:
    for s in dyn.iter_symbols():
     if s.name in wanted and s['st_shndx']!='SHN_UNDEF':owners[s.name]=str(path.relative_to(B))
 except Exception:pass
result['symbol_owners']=owners
(O/'native-model-evidence.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
print(json.dumps(result,indent=2))
