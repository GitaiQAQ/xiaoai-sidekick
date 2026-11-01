from pathlib import Path
import re
r=Path(__file__).resolve().parents[1];o=r/'temp/analysis'
ranges=[('usr_bin_mico_aivs_lab-annotated.txt',0x229a4,0x229c4),('usr_lib_libaivs_sdk.so-annotated.txt',0x18ef58,0x18f118),('usr_lib_libaivs_sdk.so-annotated.txt',0x18f1e4,0x18f200)]
out=['Raw instructions are authoritative; generated constant comments are heuristic.','GOT references checked separately in sdk-codec-got-references.json.']
for filename,a,b in ranges:
 out.append(f'\n{filename}: {a:#x}..{b:#x}')
 for line in (o/filename).read_text(encoding='utf-8').splitlines():
  if re.match(r'^[0-9a-f]{8} ',line) and a<=int(line[:8],16)<b:out.append(line)
(o/'codec-boundary-excerpts.txt').write_text('\n'.join(out),encoding='utf-8')
