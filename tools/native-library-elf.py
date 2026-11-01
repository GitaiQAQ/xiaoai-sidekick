from pathlib import Path
from elftools.elf.elffile import ELFFile
import json, sys
p=Path(__file__).resolve().parents[1]
name=sys.argv[1] if len(sys.argv)>1 else 'lx06-library-probe-armhf'
if name not in ('lx06-web-armhf','lx06-music-armhf','lx06-library-probe-armhf','lx06-ark-client-armhf','lx06-afe-frame-armhf','lx06-duplex-armhf','lx06-kws-enroll-armhf'): raise ValueError('Unknown executable')
with (p/'build'/name).open('rb') as f:
 e=ELFFile(f);d=e.get_section_by_name('.dynamic');v=e.get_section_by_name('.gnu.version_r')
 r={'elfclass':e.elfclass,'machine':e['e_machine'],'flags':hex(e['e_flags']),'entry':hex(e['e_entry']),
    'interpreter':[s.get_interp_name() for s in e.iter_segments() if s['p_type']=='PT_INTERP'],
    'needed':[t.needed for t in d.iter_tags() if t.entry.d_tag=='DT_NEEDED'],
    'versions':[a.name for ver,aux in v.iter_versions() for a in aux] if v else []}
 r['run_mode']='explicit-factory-loader'
 r['loader']='/lib/ld-linux-armhf.so.3'
 if r['elfclass']!=32 or r['machine']!='EM_ARM' or not (e['e_flags'] & 0x400) or not e['e_entry']:
  raise RuntimeError('Wrong ARM ABI/entry point')
 allowed={'GLIBC_2.4','GLIBC_2.7','GLIBC_2.17'} if name in ('lx06-web-armhf','lx06-music-armhf','lx06-library-probe-armhf','lx06-afe-frame-armhf','lx06-duplex-armhf','lx06-kws-enroll-armhf') else {'GLIBC_2.4'}
 need={'libc.so.6','libdl.so.2'}
 if name=='lx06-duplex-armhf':need.add('libpthread.so.0')
 if set(r['needed'])!=need or any(x not in allowed for x in r['versions']):
  raise RuntimeError('Unexpected runtime dependencies or glibc version')
 if r['interpreter'] not in ([],[r['loader']]):raise RuntimeError('Unexpected ELF interpreter')
 (p/('temp/analysis/'+name+'-elf.json')).write_text(json.dumps(r,indent=2),encoding='utf-8');print(json.dumps(r,indent=2))
