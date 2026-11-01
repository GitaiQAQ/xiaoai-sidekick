"""Regression for frame/feature offsets; replay enrollment and existing negatives.
This measures training-set replay, not independent wake accuracy.
"""
from pathlib import Path
import re,subprocess
root=Path(__file__).resolve().parents[1]
exe=root/'build/host-kws-r14.exe'
positive=subprocess.check_output([str(exe),str(root/'private/inputs/recordings/doubao_enroll.wav')],text=True)
(root/'logs/kws-revision14-enrollment.txt').write_text(positive,encoding='utf-8')
for segment,template in [(0,0),(1,1),(3,2),(5,3)]:
    m=re.search(rf'verdict segment={segment} .*dist=([0-9.]+) template=(\d+)',positive)
    assert m and float(m[1])<0.001 and int(m[2])==template,(segment,m)
rows=[]
for wav in sorted((root/'build/tts').glob('*.wav')):
    output=subprocess.check_output([str(exe),str(wav)],text=True)
    m=re.search(r'^scan_result .*$',output,re.M)
    assert m and 'wakes=0 ' in m[0],wav.name
    rows.append(wav.name+' '+m[0])
assert len(rows)==53,len(rows)
(root/'logs/kws-revision14-negatives.txt').write_text('\n'.join(rows)+'\n',encoding='utf-8')
print('template_offset_regression=pass templates=4 synthetic_negatives=53 wakes=0')
