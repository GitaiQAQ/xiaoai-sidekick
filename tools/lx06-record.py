"""Record the LX06 8-channel capture stream to the Mac for wake-word training data.

Usage:
  lx06-record.py record <name> <seconds>   stream arecord over SSH into private/inputs/recordings/
  lx06-record.py convert <name>            48k S32 x8 raw -> 16k s16 8ch wav + 6-mic mix wav

Reads the shared dsnoop PCM "Capture" (same as src/native_capture.h): ch0-5 microphones,
ch6 playback reference. Nothing is written on the device; arecord streams to stdout.
SSH auth comes from the caller's SSH_ASKPASS/agent; no password lives here.
Raw is kept so AEC can be re-run offline with the reference channel.
"""
from pathlib import Path
import json,os,subprocess,sys,time,wave
import numpy as np

R=Path(__file__).resolve().parents[1];D=R/'private/inputs/recordings/wake-corpus'
HOST=os.environ.get('LX06_HOST','192.168.100.210')
SSH=['ssh','-T','-o','ConnectTimeout=8','-o','HostKeyAlgorithms=+ssh-rsa',
     '-o',f"UserKnownHostsFile={os.environ.get('LX06_KNOWN_HOSTS','/tmp/lx06_known_hosts')}",f'root@{HOST}']
SR,CH=48000,8

def record(name,seconds):
    D.mkdir(parents=True,exist_ok=True);raw=D/f'{name}.raw';err=D/f'{name}.arecord.txt'
    if raw.exists():raise SystemExit(f'{raw} exists')
    cmd=f'arecord -D Capture -c {CH} -r {SR} -f S32_LE -t raw -d {int(seconds)}'
    t0=time.time()
    with open(raw,'wb') as o,open(err,'wb') as e:rc=subprocess.call(SSH+[cmd],stdout=o,stderr=e)
    n=raw.stat().st_size//(4*CH)
    meta={'name':name,'device_pcm':'Capture','rate':SR,'channels':CH,'format':'S32_LE','requested_s':seconds,
          'frames':n,'seconds':n/SR,'wall_s':round(time.time()-t0,2),'ssh_rc':rc,
          'overrun':b'overrun' in err.read_bytes().lower(),'started':time.strftime('%Y-%m-%dT%H:%M:%S',time.localtime(t0))}
    (D/f'{name}.json').write_text(json.dumps(meta,indent=1));print(json.dumps(meta))

def lowpass(taps=127,cut=7600/SR):
    n=np.arange(taps)-(taps-1)/2;h=2*cut*np.sinc(2*cut*n)*np.blackman(taps);return h/h.sum()

def convert(name):
    x=np.fromfile(D/f'{name}.raw','<i4');x=x[:len(x)//CH*CH].reshape(-1,CH).astype(np.float64)/2**31
    h=lowpass();y=np.stack([np.convolve(c,h,'same')[::3] for c in x.T],1)
    peak=float(np.abs(y).max());q=lambda v:np.clip(np.round(v*32767),-32768,32767).astype('<i2')
    def save(path,a):
        with wave.open(str(path),'wb') as w:w.setnchannels(a.shape[1] if a.ndim>1 else 1);w.setsampwidth(2);w.setframerate(16000);w.writeframes(q(a).tobytes())
    save(D/f'{name}.8ch16k.wav',y);save(D/f'{name}.mix16k.wav',y[:,:6].mean(1))
    db=lambda v:round(20*np.log10(np.sqrt((v*v).mean())+1e-12),1)
    print(json.dumps({'name':name,'seconds':len(y)/16000,'peak':round(peak,4),'rms_dbfs':[db(c) for c in y.T]}))

if __name__=='__main__':
    if len(sys.argv)==4 and sys.argv[1]=='record':record(sys.argv[2],float(sys.argv[3]))
    elif len(sys.argv)==3 and sys.argv[1]=='convert':convert(sys.argv[2])
    else:raise SystemExit(__doc__)
