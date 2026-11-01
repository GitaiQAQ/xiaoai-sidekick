from pathlib import Path
import struct
r=Path(__file__).resolve().parents[1]/'temp/analysis/models'
for s in ['mdata_wakeup_mobile','mdata_wakeup_mobile_clone']:
 for name in ['root.list.2.fst.bin','root.list.3.am.bin','root.list.4.prior.bin']:
  b=(r/s/name).read_bytes()
  print(s,name,len(b),'head',b[:80].hex(),'tail',repr(b[-100:]))
