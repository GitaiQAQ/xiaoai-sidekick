"""Build a local listening page for labelling wake-corpus segments by ear (no device access).

Usage: wake-review.py [session ...]   (default s1-pos-quiet s3-music-pos)
Cuts every segment of <session>.segments.csv (wake-label.py) longer than 0.4 s out of
<session>.aec16k.wav, writes them to private/inputs/recordings/wake-corpus/review/<session>/,
and writes review/index.html. Open it in a browser, mark each clip, press Export and
save the file as review/labels.csv; wake-train.py then prefers those labels over ASR.
"""
from pathlib import Path
import csv,html,json,sys,wave

R=Path(__file__).resolve().parents[1];C=R/'private/inputs/recordings/wake-corpus';O=C/'review'

PAGE='''<!doctype html><meta charset="utf-8"><title>豆包豆包 标注</title>
<style>body{font:14px system-ui;margin:24px}table{border-collapse:collapse}td{padding:4px 8px;border-bottom:1px solid #eee}
tr.done{background:#f3fbf3}.asr{color:#888}button{margin-right:4px}</style>
<p>逐条播放，选“是豆包豆包”（含连说多遍）/“不是”（相近词、闲聊、歌声）/“听不清”。选择自动保存在本浏览器；全部完成后点 Export，保存为 <code>review/labels.csv</code>。</p>
<p><button id="export">Export labels.csv</button> <span id="count"></span></p>
<table id="t"></table>
<script>
const rows=__ROWS__,key='wake-review-labels',lab=JSON.parse(localStorage.getItem(key)||'{}'),t=document.getElementById('t');
const names={pos:'是豆包豆包',neg:'不是',skip:'听不清'};
function count(){document.getElementById('count').textContent=`${Object.keys(lab).length}/${rows.length} 已标注`}
for(const r of rows){
  const id=`${r.session}@${r.start}`,tr=t.insertRow();
  tr.innerHTML=`<td>${r.session.slice(0,2)} ${r.start}s</td><td><audio controls preload="metadata" src="${r.file}"></audio></td><td class="dur"></td><td class="asr">${r.asr}</td><td></td>`;
  tr.querySelector('audio').onloadedmetadata=e=>tr.querySelector('.dur').textContent=e.target.duration.toFixed(2)+'s';
  for(const [k,v] of Object.entries(names)){
    const b=document.createElement('label');b.innerHTML=`<input type="radio" name="${id}" ${lab[id]===k?'checked':''}> ${v} `;
    b.firstChild.onchange=()=>{lab[id]=k;localStorage.setItem(key,JSON.stringify(lab));tr.className='done';count()};tr.cells[4].append(b);
  }
  if(lab[id])tr.className='done';
}
count();
document.getElementById('export').onclick=()=>{
  const csv='session,start_s,label\\n'+Object.entries(lab).map(([id,v])=>id.replace('@',',')+','+v).join('\\n')+'\\n';
  const a=document.createElement('a');a.href=URL.createObjectURL(new Blob([csv],{type:'text/csv'}));a.download='labels.csv';a.click();
};
</script>
'''

if __name__=='__main__':
    rows=[]
    for name in sys.argv[1:] or ['s1-pos-quiet','s3-music-pos']:
        (O/name).mkdir(parents=True,exist_ok=True)
        with wave.open(str(C/f'{name}.aec16k.wav')) as w:x=w.readframes(w.getnframes())
        for r in csv.DictReader(open(C/f'{name}.segments.csv',encoding='utf-8')):
            a,b=float(r['start_s']),float(r['end_s'])
            if b-a<0.4:continue
            f=O/name/f'{r["start_s"]}.wav'
            with wave.open(str(f),'wb') as w:
                w.setnchannels(1);w.setsampwidth(2);w.setframerate(16000);w.writeframes(x[int(a*16000)*2:int(b*16000)*2])
            rows.append(dict(session=name,start=r['start_s'],file=f'{name}/{f.name}',asr=html.escape(f"{r['text']} / {r.get('text2','')}")))
    (O/'index.html').write_text(PAGE.replace('__ROWS__',json.dumps(rows,ensure_ascii=False)),encoding='utf-8')
    print('clips',len(rows),'page',O/'index.html')
