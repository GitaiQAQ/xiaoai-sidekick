from pathlib import Path
from elftools.elf.elffile import ELFFile
r=Path(__file__).resolve().parents[1]
with (r/'temp/rootfs-factory-system0/usr/lib/libjson-c.so.2.0.2').open('rb') as f:
 e=ELFFile(f);names={s.name for s in e.get_section_by_name('.dynsym').iter_symbols() if s['st_value']}
 wanted=['json_tokener_parse','json_object_object_get_ex','json_object_get_type','json_object_get_string','json_object_put','json_object_new_object','json_object_new_array','json_object_new_string','json_object_new_int','json_object_object_add','json_object_array_add','json_object_array_get_idx','json_object_to_json_string']
 for name in wanted:print(name, name in names)
