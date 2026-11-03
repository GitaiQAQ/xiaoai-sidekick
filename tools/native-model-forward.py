"""Reference forward pass of the factory wake model on macOS (numpy).

Usage: native-model-forward.py <16k mono s16 wav> [mdata_wakeup_mobile|mdata_wakeup_mobile_clone]
Needs am-decoded.json from native-model-am.py and the extracted blobs.
Network math follows the soil loaders (affine W is out x in row-major, normalize
uses 1/sqrt(sumsq*target_rms/dim) with floor 1.355e-20, final logsoftmax).
Feature front end is Kaldi-style MFCC from the conf blob. Constants read statically
from RiceFeatureInitialize 0x5af10 (frame 400/shift 160/fft 512, povey window type 3,
preemph 0.97, remove DC, lifter 22, dither default 0.0 @0x58df8) and RiceLoadModelBin
(CMVN state +0x3c0 only exists when the container has a "cmvn" key; factory models
have none, so features go to the network raw). Not yet compared to device output.
Viterbi follows the decoded FST (transition-id -> pdf = phone-1, Kaldi HMM).
"""
from pathlib import Path
import json,struct,sys,wave
import numpy as np

R=Path(__file__).resolve().parents[1];O=R/'temp/analysis/models'

def mfcc(pcm,sr=16000,ceps=40,bins=40,low=20.0,high=-200.0):
    x=pcm.astype(np.float64);fl,sh,n=400,160,512
    if len(x)<fl:raise ValueError('audio too short')
    T=1+(len(x)-fl)//sh;fr=np.stack([x[i*sh:i*sh+fl] for i in range(T)])
    fr-=fr.mean(1,keepdims=True);fr[:,1:]-=0.97*fr[:,:-1];fr[:,0]*=0.03
    fr*=np.power(0.5-0.5*np.cos(2*np.pi*np.arange(fl)/(fl-1)),0.85)
    p=np.abs(np.fft.rfft(fr,n))**2
    hi=sr/2+high if high<=0 else high;mel=lambda f:1127*np.log(1+f/700)
    ml,mh=mel(low),mel(hi);d=(mh-ml)/(bins+1);fm=mel(np.arange(n//2+1)*sr/n);fb=np.zeros((bins,n//2+1))
    for b in range(bins):
        l,c,r=ml+b*d,ml+(b+1)*d,ml+(b+2)*d
        fb[b]=np.where((fm>l)&(fm<=c),(fm-l)/(c-l),np.where((fm>c)&(fm<r),(r-fm)/(r-c),0))
    e=np.log(np.maximum(p@fb.T,np.finfo(np.float32).eps))
    k=np.arange(ceps)[:,None];j=np.arange(bins)[None]
    dct=np.sqrt(2/bins)*np.cos(np.pi/bins*(j+0.5)*k);dct[0]=np.sqrt(1/bins)
    c=e@dct.T;c*=1+11*np.sin(np.pi*np.arange(ceps)/22)
    return c.astype(np.float32)

def forward(feats,net,blob):
    fw=np.frombuffer(blob,'<f4');x=feats
    for c in net['components'][1:]:
        t=c['type']
        if t==7:x=np.concatenate([np.repeat(x[:1],c['left_context'],0),x,np.repeat(x[-1:],c['right_context'],0)])
        elif t==6:
            o=c['offsets'];lo,hi=-min(o),max(o);T=len(x)-lo-hi
            x=np.concatenate([x[lo+k:lo+k+T] for k in o],1)
        elif t==8:
            W=fw[c['weight_word']:c['weight_word']+c['in']*c['out']].reshape(c['out'],c['in']);b=fw[c['bias_word']:c['bias_word']+c['out']]
            x=x@W.T+b
        elif t==9:x=np.maximum(x,0)
        elif t==17:x=x/np.sqrt(np.maximum((x*x).sum(1,keepdims=True)*c['target_rms']/c['size'],1.3552527e-20))
        elif t==19:x=x-x.max(1,keepdims=True);x=x-np.log(np.exp(x).sum(1,keepdims=True))
        else:raise ValueError(c['name'])
    return x

def viterbi(loglik,fst,priors,scale=0.1):
    """Best path cost of reaching the keyword output label 4, per frame (lower is better)."""
    am=scale*(loglik-priors);S=fst['states'];inf=np.inf;cost=np.full(S,inf);cost[fst['start']]=0;best=[]
    for t in range(len(am)):
        new=np.full(S,inf)
        for st in fst['graph']:
            if cost[st['state']]==inf:continue
            for a in st['arcs']:
                if a['input']==0:continue
                pdf=(a['input']+1)//2-1;v=cost[st['state']]+a['weight']-am[t,pdf]
                if v<new[a['next']]:new[a['next']]=v
        for st in fst['graph']:
            for a in st['arcs']:
                if a['input']==0 and new[st['state']]+a['weight']<new[a['next']]:new[a['next']]=new[st['state']]+a['weight']
        cost=new;best.append((cost[9]-cost[1]) if np.isfinite(cost[9]) else inf)
    return np.array(best)

if __name__=='__main__':
    wav=sys.argv[1];model=sys.argv[2] if len(sys.argv)>2 else 'mdata_wakeup_mobile'
    with wave.open(wav) as w:
        if w.getframerate()!=16000 or w.getnchannels()!=1 or w.getsampwidth()!=2:raise SystemExit('need 16k mono s16')
        pcm=np.frombuffer(w.readframes(w.getnframes()),'<i2')
    pcm=np.concatenate([np.zeros(8000,np.int16),pcm,np.zeros(8000,np.int16)])
    d=O/model;net=json.loads((d/'am-decoded.json').read_text());blob=(d/'root.list.3.am.bin').read_bytes()
    fst=json.loads((d/'fst-decoded.json').read_text())
    prior=json.loads((d/'decoded.json').read_text())['model']['list'][4]['prior']['decoded_container']['log_priors']
    y=forward(mfcc(pcm),net,blob);post=np.exp(y)
    print(model,'frames',len(y),'argmax pdf sequence (collapsed):',[int(k) for i,k in enumerate(y.argmax(1)) if i==0 or k!=y[i-1].argmax()])
    print(' mean posterior per pdf',np.round(post.mean(0),3).tolist())
    margin=viterbi(y,fst,np.array(prior));print(' min keyword-vs-garbage cost margin',round(float(margin.min()),3),'at frame',int(margin.argmin()))
