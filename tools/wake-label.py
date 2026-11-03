"""Segment and transcribe wake-corpus recordings on macOS (no device access).

Usage: wake-label.py <session> [...]   (sessions under private/inputs/recordings/wake-corpus)
       wake-label.py tts                  (SenseVoice text for every TTS clip -> temp/analysis/tts-corpus/asr.csv)
Reads <session>.aec16k.wav (host Speex AEC replay), finds energy segments, runs two
ASR models from vendor/tts-models/asr on each (paraformer-zh-small -> text,
SenseVoice int8 -> text2; they fail on different clips under music), and writes
<session>.segments.csv: start_s,end_s,peak_dbfs,text,text2. Labels are ASR output,
not human-checked; anything used for training should be reviewed by listening.
"""
from pathlib import Path
import csv,sys,wave
import numpy as np
import sherpa_onnx

R=Path(__file__).resolve().parents[1];C=R/'private/inputs/recordings/wake-corpus'
A=R/'vendor/tts-models/asr/sherpa-onnx-paraformer-zh-small-2024-03-09'
SV=R/'vendor/tts-models/asr/sherpa-onnx-sense-voice-zh-en-ja-ko-yue-int8-2024-07-17'

def segments(x,sr=16000,win=0.02,gap=0.35,pad=0.15,min_len=0.25,margin_db=12):
    n=int(sr*win);e=10*np.log10(np.maximum((x[:len(x)//n*n].reshape(-1,n).astype(np.float64)**2).mean(1),1e-3)/32768**2)
    floor=np.percentile(e,20);on=e>floor+margin_db;out=[];i=0
    while i<len(on):
        if not on[i]:i+=1;continue
        j=i
        while j<len(on) and (on[j] or on[j:j+int(gap/win)].any()):j+=1
        if (j-i)*win>=min_len:out.append((max(0,i*win-pad),min(len(x)/sr,j*win+pad),float(e[i:j].max())))
        i=j
    return out,floor

if __name__=='__main__':
    rec=sherpa_onnx.OfflineRecognizer.from_paraformer(paraformer=str(A/'model.int8.onnx'),tokens=str(A/'tokens.txt'),num_threads=4)
    sv=sherpa_onnx.OfflineRecognizer.from_sense_voice(model=str(SV/'model.int8.onnx'),tokens=str(SV/'tokens.txt'),language='zh',use_itn=False,num_threads=4)
    def asr(r,pcm):
        st=r.create_stream();st.accept_waveform(16000,pcm);r.decode_stream(st);return st.result.text
    if sys.argv[1:]==['tts']:
        import soundfile;T=R/'temp/analysis/tts-corpus'
        rows=[r for r in csv.DictReader(open(T/'manifest.csv',encoding='utf-8')) if r['label']!='neg']
        with (T/'asr.csv').open('w',newline='',encoding='utf-8') as f:
            w=csv.writer(f);w.writerow(['file','text2'])
            for r in rows:w.writerow([r['file'],asr(sv,soundfile.read(str(T/r['file']),dtype='float32')[0])])
        print('tts clips',len(rows));sys.exit()
    for name in sys.argv[1:]:
        with wave.open(str(C/f'{name}.aec16k.wav')) as w:x=np.frombuffer(w.readframes(w.getnframes()),'<i2')
        segs,floor=segments(x);rows=[]
        for s,e,pk in segs:
            pcm=x[int(s*16000):int(e*16000)].astype(np.float32)/32768
            rows.append((round(s,2),round(e,2),round(pk,1),asr(rec,pcm),asr(sv,pcm)))
        with (C/f'{name}.segments.csv').open('w',newline='',encoding='utf-8') as f:
            w=csv.writer(f);w.writerow(['start_s','end_s','peak_dbfs','text','text2']);w.writerows(rows)
        print(name,'floor',round(float(floor),1),'segments',len(rows),'with text',sum(1 for r in rows if r[3]))
