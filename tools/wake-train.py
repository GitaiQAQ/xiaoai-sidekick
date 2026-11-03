"""Train a "豆包豆包" wake model in the factory Rice/soil format on macOS.

Usage: run this script with Python that has torch installed. The local
training environment may live outside the checkout; see tools/README.md.
  wake-train.py all [--level 1|2] [--dim 96] [--out temp/analysis/wake-train/d96]
  wake-train.py eval --out <dir> [--verify <level-2 dir>]
--level 2 takes the factory level-2 (mdata_wakeup_mobile_clone) container as template: same
graph with forward weight 4.0, normally trained with --dim 160.
Graph keeps the factory shape: 10 pdfs, pdf0 silence, pdf3 garbage, keyword
d ou b ao d ou b ao as position-dependent pdfs 1,2,4,5,6,7,8,9 so the second
"豆包" can use 40 frames of left context to reject a single "豆包".
--shared-keyword instead uses [1,2,4,5,1,2,4,5]: the two passes share
acoustics but retain separate graph states. Outputs 6..9 remain for BINN compatibility.
Data: TTS corpus (temp/analysis/tts-corpus; positives kept only if SenseVoice hears
豆包 twice, tone-only variants such as 逗包逗包 left out), host-AEC replays of the LX06
recordings (s1 positives via ASR text, s2 music residual as background).
s3-music-pos and s2b-music-silent are held out for evaluation.
Real-recording labels come from private/.../review/labels.csv (tools/wake-review.py)
when present, otherwise from the two ASR transcripts. Labels start from a uniform syllable split on pure keyword clips and are then
re-estimated twice by forced alignment with the current network; negatives the current
network accepts are mined after each pass and trained with extra copies.
"""
from pathlib import Path
import argparse,csv,importlib.util,json,math,struct,time,wave,zlib
import numpy as np,torch,torch.nn as nn

R=Path(__file__).resolve().parents[1];T=R/'tools';M=R/'temp/analysis/models/mdata_wakeup_mobile'
C=R/'private/inputs/recordings/wake-corpus';TTS=R/'temp/analysis/tts-corpus'
def mod(name,file):
    s=importlib.util.spec_from_file_location(name,T/file);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
F=mod('fwd','native-model-forward.py');P=mod('pack','native-model-pack.py');A=mod('am','native-model-am.py')

SIL,GARB=0,3
# Two keyword layouts, both 8 position states over 10 factory pdfs:
#   distinct: 8 different pdfs, each state has its own acoustics (previous default)
#   shared:   two "豆包" passes share the same 4 pdfs, so the FST alone still requires
#             two passes (8 arcs) while the acoustic model has no positional variants
KW_DISTINCT=[1,2,4,5,6,7,8,9];KW_SHARED=[1,2,4,5,1,2,4,5];KW=list(KW_DISTINCT)
PURE={'豆包豆包','豆包，豆包','豆包豆包！','豆包豆包？'}
def used_pdfs():return {SIL,GARB}|set(KW)
def wav(p):
    with wave.open(str(p)) as w:
        if w.getframerate()!=16000 or w.getnchannels()!=1:raise ValueError(p)
        return np.frombuffer(w.readframes(w.getnframes()),'<i2').astype(np.float32)
nframes=lambda n:max(0,1+(n-400)//160)
def frame_db(x):
    n=nframes(len(x));idx=np.arange(400)[None]+160*np.arange(n)[:,None]
    return 10*np.log10(np.maximum((x[idx].astype(np.float64)**2).mean(1),1e-4)/32768**2)
def bg_labels(x):
    e=frame_db(x);return np.where(e>max(e.max()-30,-70),GARB,SIL).astype(np.int64)

# ---------------- data sources
# Same syllables as 豆包豆包 up to tone (SenseVoice hears most of them as 豆包豆包): never negatives.
TONE_ONLY={'逗包逗包','斗包斗包','豆宝豆宝','逗宝逗宝','斗宝斗宝','都包都包','都饱都饱'}
CONFUSABLE=set('豆包宝八吧巴保饱抱报爸把东兜斗逗走多对到道刀大套肉六叮当动周在快做过跳小上这正')
double=lambda t:t.replace('，','').count('豆包')>=2
HS=set()                                                                  # sessions covered by human labels
def human():
    """Listening labels from wake-review.py ({(session,start_s):'pos'|'neg'|'skip'}), empty if not exported yet.
    A session present in the file is authoritative: clips left unlabelled are skip (user: unclear or silent)."""
    global HS
    f=C/'review/labels.csv'
    if not f.exists():HS=set();return {}
    rows=list(csv.DictReader(open(f,encoding='utf-8')));HS={r['session'] for r in rows}
    return {(r['session'],r['start_s']):r['label'] for r in rows}
def real_kind(session,r,hl):
    t,t2=r['text'],r.get('text2','')
    if session in HS:return {'pos':'pos','neg':'neg'}.get(hl.get((session,r['start_s'])))
    if double(t) or double(t2):return 'pos'
    if t and not (set(t)|set(t2))&CONFUSABLE:return 'neg'
    return None                                                          # ASR-ambiguous, keep out of training
def sources():
    src=[];rows=list(csv.DictReader(open(TTS/'manifest.csv',encoding='utf-8')))
    asr={r['file']:r['text2'] for r in csv.DictReader(open(TTS/'asr.csv',encoding='utf-8'))} if (TTS/'asr.csv').exists() else {}
    for r in rows:
        if not (TTS/r['file']).exists():continue                        # corpus may still be regenerating
        if r['label']=='pos' and asr and not double(asr.get(r['file'],'')):continue   # TTS voice garbled the keyword
        dev=zlib.crc32(r['voice'].encode())%8==0;sub='homophone' if r['text'] in TONE_ONLY else r['label']
        src.append(dict(id=r['file'],kind='pos' if r['label']=='pos' else 'neg',sub=sub,text=r['text'],real=False,split='dev' if dev else 'train',pcm=wav(TTS/r['file'])))
    s1=wav(C/'s1-pos-quiet.aec16k.wav');keep=np.ones(len(s1),bool);hl=human()
    for r in csv.DictReader(open(C/'s1-pos-quiet.segments.csv',encoding='utf-8')):
        a,b=int(float(r['start_s'])*16000),int(float(r['end_s'])*16000);keep[a:b]=False
        kind=real_kind('s1-pos-quiet',r,hl)
        if kind:src.append(dict(id=f's1@{r["start_s"]}',kind=kind,sub='real',text=r['text'],real=True,split='train',pcm=s1[a:b]))
    bg={'quiet':s1[keep],'music':wav(C/'s2-music-silent.aec16k.wav')}
    return src,bg

# ---------------- augmentation (absolute levels: factory models use raw MFCC, no CMVN)
def rir(rng):
    rt=rng.uniform(0.15,0.7);n=int(rt*16000);t=np.arange(n)/16000
    h=rng.standard_normal(n)*np.exp(-6.9*t/rt)*10**(rng.uniform(-18,-6)/20);h[0]=1;return h/np.sqrt((h**2).sum())
def conv(x,h):
    n=len(x)+len(h)-1;m=1<<(n-1).bit_length();return np.fft.irfft(np.fft.rfft(x,m)*np.fft.rfft(h,m),m)[:len(x)]
def speed(x,r):
    """Resample by r (tempo and pitch together, like Kaldi speed perturbation)."""
    return np.interp(np.arange(0,len(x)-1,r),np.arange(len(x)),x)
def augment(x,rng,bg,real):
    """Returns (pcm, leading pad frames, speed ratio r); source frame i lands near output frame i/r."""
    x=x.astype(np.float64);r=1.0
    if not real:
        if rng.random()<0.6:r=rng.uniform(0.85,1.35);x=speed(x,r)          # s3 has ~0.7 s fast repeats
        if rng.random()<0.7:x=conv(x,rir(rng))
        if rng.random()<0.5:                                             # mild tilt, device mic / speex colouring
            a=rng.uniform(-0.6,0.6);x=np.append(x[0],x[1:]-a*x[:-1]) if a>0 else np.append(x[0],x[1:]+(-a)*x[:-1])
        e=frame_db(x);act=e>e.max()-30;lvl=10*np.log10((10**(e[act]/10)).mean())
        x*=10**((rng.uniform(-62,-30)-lvl)/20)
    else:x*=10**(rng.uniform(-6,6)/20)
    pre,post=160*int(rng.integers(20,80)),160*int(rng.integers(20,60))
    y=np.concatenate([np.zeros(pre),x,np.zeros(post)])
    b=bg['music'] if rng.random()<0.55 else bg['quiet'];s=int(rng.integers(0,len(b)-len(y)))
    y+=b[s:s+len(y)]*10**(rng.uniform(-6,4)/20)
    return np.clip(y,-32768,32767).astype(np.float32),pre//160,r

# ---------------- network: soil chain = conv1d stack (splice offsets -> kernel/dilation)
class Net(nn.Module):
    def __init__(s,D=96):
        super().__init__();s.D=D
        s.l=nn.ModuleList([nn.Conv1d(40,D,5),nn.Conv1d(D,D,1),nn.Conv1d(D,D,1),nn.Conv1d(D,D,4,dilation=3),nn.Conv1d(D,D,1),nn.Conv1d(D,D,1),
                           nn.Conv1d(D,D,5,dilation=9),nn.Conv1d(D,D,1),nn.Conv1d(D,D,1)]);s.o=nn.Conv1d(D,10,1)
    def forward(s,x):                                                     # x: B x 40 x (T+49), already edge-padded
        for c in s.l:x=torch.relu(c(x));x=x/torch.sqrt(torch.clamp((x*x).sum(1,keepdim=True)/s.D,min=1.3552527e-20))
        return torch.log_softmax(s.o(x),1)
def padded(f):return np.concatenate([np.repeat(f[:1],40,0),f,np.repeat(f[-1:],9,0)])

def export(net):
    """Soil component list identical to the factory level-1 layout, dims from net.D."""
    ref=json.loads((M/'am-decoded.json').read_text());comps=[];W={};convs=list(net.l)+[net.o];k=0
    for c in ref['components']:
        c={x:y for x,y in c.items() if x not in('weight_word','bias_word')}
        if c['type']==6:c['in']=40 if c['index']==2 else net.D;c['out']=c['in']*len(c['offsets'])
        if c['type']==17:c['size']=net.D
        if c['type']==8:
            m=convs[k];k+=1;w=m.weight.detach().double().numpy();c['out']=w.shape[0];c['in']=w.shape[1]*w.shape[2]
            W[c['index']]=(w.transpose(0,2,1).reshape(w.shape[0],-1).astype(np.float32),m.bias.detach().numpy().astype(np.float32))
        comps.append(c)
    net_json={'components':comps};blob=A.emit(net_json,{i:(w.tolist(),b.tolist()) for i,(w,b) in W.items()})
    parsed=A.parse(blob);return parsed,blob

# ---------------- alignment
def align(lp,prior,loop):
    """Forced alignment: bg* (K1..K8 bg*)+ ; bg emits max(sil,garbage). Returns frame pdf labels."""
    s=lp-prior;bgsc=np.maximum(s[:,SIL],s[:,GARB]);T=len(s);N=10            # states: 0 bg-before, 1..8 kw, 9 bg-after
    em=np.concatenate([bgsc[:,None],s[:,KW],bgsc[:,None]],1);NEG=-1e18
    d=np.full(N,NEG);d[0]=em[0,0];d[1]=em[0,1];bp=np.zeros((T,N),np.int8)
    for t in range(1,T):
        prev=np.full(N,NEG);arg=np.arange(N)
        cand=[(0,0),(1,0),(1,1)]+[(i,i) for i in range(2,9)]+[(i,i-1) for i in range(2,9)]+[(9,8),(9,9)]+([(1,9)] if loop else [])
        for dst,src in cand:
            if d[src]>prev[dst]:prev[dst]=d[src];arg[dst]=src
        d=prev+em[t];bp[t]=arg
    st=9 if d[9]>=d[8] else 8;path=[st]
    for t in range(T-1,0,-1):st=bp[t,st];path.append(st)
    path=path[::-1];out=np.empty(T,np.int64)
    for t,q in enumerate(path):out[t]=KW[q-1] if 1<=q<=8 else (SIL if s[t,SIL]>=s[t,GARB] else GARB)
    return out
def neg_labels(s):
    """Garbage on speech. Syllables a hard negative shares with the start of 豆包豆包 (豆, 豆包, 豆包豆)
    really are the keyword's first syllables, so they get those keyword pdfs (uniform split, like
    uniform_labels); this teaches that 豆包 after silence is the first half, never the second.
    The first differing syllable onwards is garbage. In shared mode these labels
    describe acoustics only; completing both passes is enforced by the graph."""
    lab=bg_labels(s['pcm']);t=''.join(ch for ch in s['text'] if '\u4e00'<=ch<='\u9fff')
    k=next((i for i,(a,b) in enumerate(zip(t,'豆包豆包')) if a!=b),min(len(t),4))
    if s['sub']=='hardneg' and k:
        e=frame_db(s['pcm']);act=np.flatnonzero(e>e.max()-30)
        if len(act):
            a,b=act[0],act[-1]+1;edges=np.linspace(a,b,len(t)+1).astype(int)
            for i in range(k):
                u,v=edges[i],edges[i+1];m=u+max(1,int(0.3*(v-u)));lab[u:m]=KW[2*i];lab[m:v]=KW[2*i+1]
    return lab
def uniform_labels(x):
    e=frame_db(x);act=np.where(e>e.max()-30)[0];lab=bg_labels(x)
    if len(act)<16:return None
    a,b=act[0],act[-1]+1;edges=np.linspace(a,b,5).astype(int)
    for i in range(4):
        s,t=edges[i],edges[i+1];m=s+max(1,int(0.3*(t-s)));lab[s:m]=KW[2*i];lab[m:t]=KW[2*i+1]
    return lab

# ---------------- training
def build(src,bg,labels,rng,copies,hard=()):
    """hard: indices of negatives the previous network accepted, trained copies['mined'] extra times."""
    ex=[]
    for i,s in enumerate(src):
        if s['split']!='train' or s['sub']=='homophone':continue
        lab=labels.get(i) if s['kind']=='pos' else None
        if s['kind']=='pos' and lab is None:continue
        n=copies['real' if s['real'] else s['sub'] if s['sub'] in copies else s['kind']]+(copies['mined'] if i in hard else 0)
        for _ in range(n):
            y,pre,r=augment(s['pcm'],rng,bg,s['real']);f=F.mfcc(y);L=bg_labels(y)
            base=lab if lab is not None else neg_labels(s)
            if r!=1.0:base=base[np.minimum((np.arange(int(len(base)/r))*r).astype(int),len(base)-1)]
            seg=L[pre:pre+len(base)];base=base[:len(seg)]
            keep=(base!=SIL)&(base!=GARB) if lab is not None or s['sub']=='hardneg' else base!=SIL
            seg[keep]=base[keep];ex.append((f,L))
    m=bg['music'];step=48000
    for a in range(0,len(m)-step,step//2):ex.append((F.mfcc(m[a:a+step]),bg_labels(m[a:a+step])))
    return ex

def train(ex,D,epochs,seed=0,log=print):
    torch.manual_seed(seed);net=Net(D);opt=torch.optim.Adam(net.parameters(),1e-3,weight_decay=1e-5)
    sched=torch.optim.lr_scheduler.OneCycleLR(opt,3e-3,total_steps=epochs*math.ceil(len(ex)/32))
    rng=np.random.default_rng(seed)
    for ep in range(epochs):
        order=rng.permutation(len(ex));tot=n=acc=0;t0=time.time()
        for b in range(0,len(order),32):
            batch=[ex[i] for i in order[b:b+32]];Tm=max(len(f) for f,_ in batch)
            X=np.stack([padded(np.concatenate([f,np.repeat(f[-1:],Tm-len(f),0)])) for f,_ in batch]).transpose(0,2,1)
            Y=np.full((len(batch),Tm),-100);[Y.__setitem__((j,slice(0,len(l))),l) for j,(_,l) in enumerate(batch)]
            out=net(torch.from_numpy(X.astype(np.float32)));Yt=torch.from_numpy(Y)
            loss=nn.functional.nll_loss(out,Yt,ignore_index=-100);opt.zero_grad();loss.backward();opt.step();sched.step()
            k=(Yt!=-100);tot+=loss.item()*k.sum().item();n+=k.sum().item();acc+=((out.argmax(1)==Yt)&k).sum().item()
        log(f'  epoch {ep+1}/{epochs} loss {tot/n:.4f} frame-acc {acc/n:.3f} {time.time()-t0:.0f}s')
    return net

def posteriors(net,f):
    with torch.no_grad():return net(torch.from_numpy(padded(f).T[None].astype(np.float32)))[0].T.numpy()

def priors_of(ex,used=None):
    c=np.bincount(np.concatenate([l[l>=0] for _,l in ex]),minlength=10).astype(np.float64)+1
    lp=np.log(c/c.sum())
    if used is not None:
        unused=[p for p in range(10) if p not in used]
        if unused:
            # A never-labelled pdf only has the +1 smoothing count, so log(1/N) is far
            # below every used pdf.  Subtracting such a prior boosts its acoustic
            # likelihood (lp-prior) by ~10 nats.  Clamp it to the rarest used pdf so an
            # unused output cannot win a comparison by prior alone.
            lp[unused]=lp[sorted(used)].min()
    return lp

def realign(net,src,prior):
    labels={}
    for i,s in enumerate(src):
        if s['kind']!='pos' or s['split']!='train':continue
        f=F.mfcc(s['pcm']) if s['real'] else F.mfcc(s['pcm']*10**((-45-max(frame_db(s['pcm'])))/20)+np.random.default_rng(i).standard_normal(len(s['pcm']))*0.5)
        labels[i]=align(posteriors(net,f),prior,loop=s['real'])
    return labels

def kw_fst():
    """Factory level-1 graph with the keyword chain relabelled to pdfs KW."""
    fst=json.loads((M/'fst-decoded.json').read_text());g=[dict(final=n['final'],arcs=[dict(a) for a in n['arcs']]) for n in fst['graph']]
    chain=[(0,2)]+[(i,i+1) for i in range(2,9)]
    for k,(st,dst) in enumerate(chain):
        for a in g[st]['arcs']:
            if a['next']==dst:a['input']=2*(KW[k]+1)
        for a in g[dst]['arcs']:
            if a['next']==dst:a['input']=2*(KW[k]+1)-1
    return g,fst['start']
def mine(net,src,prior,bg,rng,thr=0.0):
    """Train negatives whose margin under one fresh augmentation falls below thr."""
    g,start=kw_fst();fst=dict(start=start,states=10,graph=[dict(state=i,**n) for i,n in enumerate(g)]);hard=set()
    for i,s in enumerate(src):
        if s['kind']!='neg' or s['split']!='train' or s['sub']=='homophone':continue
        y=augment(s['pcm'],rng,bg,s['real'])[0]
        if F.viterbi(posteriors(net,F.mfcc(y)),fst,prior).min()<thr:hard.add(i)
    return hard

# ---------------- packaging
def package(net,prior,labels_all,out):
    parsed,am=export(net);ref=json.loads((M/'decoded.json').read_text())['model']['list']
    conf=(M/'root.list.0.conf.bin').read_bytes();tm=P.parse_transition_model((M/'root.list.1.model.bin').read_bytes())
    lab=np.concatenate(labels_all);lab=lab[lab>=0];lp=[0.0]
    runs=np.diff(np.flatnonzero(np.diff(np.r_[-1,lab,-1])!=0));vals=lab[np.r_[0,np.cumsum(runs)[:-1]]]
    obs=[max(1.5,runs[vals==pdf].mean()) for pdf in range(10) if (vals==pdf).any()]
    fallback=float(np.median(obs)) if KW==KW_SHARED and obs else 2.0 # preserve distinct default; shared unused pdfs get typical duration
    for pdf in range(10):                                                 # self-loop prob from mean run length
        dur=max(1.5,runs[vals==pdf].mean()) if (vals==pdf).any() else fallback;lp+=[math.log(1-1/dur),math.log(1/dur)]
    tm['logprobs']=lp
    g,start=kw_fst();fst=dict(start=start);fb=P.fst_blob(g,start)
    blob=P.container(conf,P.transition_model(tm),fb,am,P.prior_blob(prior,40,9))
    out.mkdir(parents=True,exist_ok=True);(out/'model.binn').write_bytes(blob);(out/'root.list.3.am.bin').write_bytes(am)
    (out/'am-decoded.json').write_text(json.dumps(parsed));(out/'fst-decoded.json').write_text(json.dumps(dict(start=fst['start'],states=10,graph=[dict(state=i,**n) for i,n in enumerate(g)])))
    (out/'prior.json').write_text(json.dumps([float(x) for x in prior]));torch.save(net.state_dict(),out/'net.pt')
    top,_=P.load(blob);assert {k for d in top['list'] for k in d}=={'conf','model','fst','am','prior'}
    return blob

# ---------------- evaluation with the numpy reference forward on the exported blob
def margins(out,pcm):
    net=json.loads((out/'am-decoded.json').read_text());am=(out/'root.list.3.am.bin').read_bytes()
    fst=json.loads((out/'fst-decoded.json').read_text());prior=np.array(json.loads((out/'prior.json').read_text()))
    return F.viterbi(F.forward(F.mfcc(pcm),net,am),fst,prior)
def events(m,thr,gap=100):
    hit=np.flatnonzero(m<thr);ev=[]
    for t in hit:
        if not ev or t-ev[-1][1]>gap:ev.append([t,t])
        else:ev[-1][1]=t
    return ev
def evaluate(out,src=None,log=print):
    res={}
    if src is not None:                                                   # TTS dev voices, one fixed augmentation each
        rng=np.random.default_rng(7);m=wav(C/'s2b-music-silent.aec16k.wav');bg={'quiet':m,'music':m}
        dv=[(s['sub'],s['text'],float(margins(out,augment(s['pcm'],rng,bg,False)[0]).min()),s['id']) for s in src if s['split']=='dev']
        res['dev']=dv
    for name in ('s3-music-pos','s2b-music-silent','s2-music-silent'):
        res[name]=margins(out,wav(C/f'{name}.aec16k.wav')).tolist()
    (out/'eval.json').write_text(json.dumps(res));report(out,log)
def report(out,log=print,verify=None):
    """verify: level-2 output dir; a level-1 event then counts only if the level-2 margin drops
    below 0 within 1 s of it (dev clips: both minima), like the factory two-level cascade."""
    res=json.loads((out/'eval.json').read_text())
    if verify:
        r2=json.loads((Path(verify)/'eval.json').read_text());m2={d[3]:d[2] for d in r2.get('dev',[])}
        res['dev']=[d[:2]+[d[2] if m2.get(d[3],1e9)<0 else 1e9]+d[3:] for d in res.get('dev',[])]
        for k in ('s3-music-pos','s2b-music-silent','s2-music-silent'):res[k+'@2']=np.array(r2[k])
        log(f'cascade: level 1 {out} confirmed by level 2 {verify} at thr 0')
    def ev_of(k,thr):
        m=np.array(res[k]);ev=events(m,thr)
        return [e for e in ev if res[k+'@2'][max(0,e[0]-100):e[1]+100].min()<0] if verify else ev
    def seg(name):return list(csv.DictReader(open(C/f'{name}.segments.csv',encoding='utf-8')))
    hl=human();s3=seg('s3-music-pos');pos=[(float(r['start_s']),float(r['end_s'])) for r in s3 if real_kind('s3-music-pos',r,hl)=='pos']
    log(f's3 positives from {"listening labels" if any(k[0]=="s3-music-pos" for k in hl) else "ASR (paraformer or SenseVoice hears 豆包 twice)"}: {len(pos)}')
    log(f'{"thr":>6} {"devPos":>7} {"devHard":>8} {"devNeg":>7} | {"s3 hit":>7} {"s3 other":>8} | {"s2b FA":>6} {"s2 FA":>6} (s2 is train bg)')
    dev=res.get('dev',[])
    for thr in (-6,-4,-2,0,2,4):
        dp=[d[2] for d in dev if d[0]=='pos'];dh=[d[2] for d in dev if d[0]=='hardneg'];dn=[d[2] for d in dev if d[0]=='neg']
        ev=ev_of('s3-music-pos',thr);ts=[(a+b)/200 for a,b in ev]
        hit=sum(any(a-0.3<=t<=b+0.5 for t in ts) for a,b in pos);other=sum(not any(a-0.3<=t<=b+0.5 for a,b in pos) for t in ts)
        fa2b=len(ev_of('s2b-music-silent',thr));fa2=len(ev_of('s2-music-silent',thr))
        rate=lambda v:f'{np.mean(np.array(v)<thr):.3f}' if v else '-'
        log(f'{thr:>6} {rate(dp):>7} {rate(dh):>8} {rate(dn):>7} | {hit:>3}/{len(pos):<3} {other:>8} | {fa2b:>6} {fa2:>6}')
    m=np.array(res['s3-music-pos'])
    log('s3 events at thr 0 (start s:min margin): '+' '.join(f'{a/100:.1f}:{m[a:b+1].min():.1f}' for a,b in ev_of('s3-music-pos',0)))
    ho=[d[2] for d in dev if d[0]=='homophone']
    if ho:log(f'homophones (tone-only, unlabelled) accepted at thr 0: {np.mean(np.array(ho)<0):.3f} of {len(ho)}')

def main():
    ap=argparse.ArgumentParser();ap.add_argument('cmd',choices=['all','eval']);ap.add_argument('--dim',type=int,default=96)
    ap.add_argument('--epochs',type=int,default=12);ap.add_argument('--out',default=None)
    ap.add_argument('--level',type=int,choices=[1,2],default=1);ap.add_argument('--verify',default=None)
    ap.add_argument('--real-copies',type=int,default=8)
    ap.add_argument('--shared-keyword',action='store_true',
                    help='B layout: both 豆包 passes share pdfs [1,2,4,5,1,2,4,5]; FST still has 8 arcs')
    ap.add_argument('--smoke',type=int,default=0,
                    help='short pipeline test: keep <=N train sources per kind/split and 1 epoch per iteration')
    a=ap.parse_args()
    global M
    if a.level==2:M=M.with_name('mdata_wakeup_mobile_clone')
    KW[:]=KW_SHARED if a.shared_keyword else KW_DISTINCT
    out=Path(a.out) if a.out else R/f'temp/analysis/wake-train/d{a.dim}';out.mkdir(parents=True,exist_ok=True)
    logf=open(out/'train.log','a');log=lambda s:(print(s,flush=True),logf.write(s+'\n'),logf.flush())
    log(f'config level={a.level} dim={a.dim} epochs={a.epochs} real_copies={a.real_copies} '
        f'shared_keyword={a.shared_keyword} KW={KW} smoke={a.smoke}')
    if a.cmd=='eval':report(out,log,a.verify);return
    src,bg=sources();rng=np.random.default_rng(0)
    if a.smoke>0:
        from collections import defaultdict
        g=defaultdict(list)
        for s in src:
            if not s['real'] and s['split']=='train':g[(s['kind'],s['split'])].append(s)
        keep={id(s) for v in g.values() for s in v[::max(1,math.ceil(len(v)/a.smoke))]}
        src=[s for s in src if s['real'] or s['split']!='train' or id(s) in keep]
        log(f'smoke subset: sources {len(src)}')
    log(f'sources {len(src)}: '+json.dumps({f'{k}/{r}/{sp}':sum(1 for s in src if (s["kind"],s["real"],s["split"])==(k,r,sp)) for k in("pos","neg") for r in(False,True) for sp in("train","dev")}))
    labels={i:uniform_labels(s['pcm']) for i,s in enumerate(src) if s['kind']=='pos' and not s['real'] and s['split']=='train' and s['text'] in PURE}
    labels={i:l for i,l in labels.items() if l is not None};log(f'iter0 uniform-labelled pure clips {len(labels)}')
    copies={'pos':3,'neg':1,'hardneg':2,'real':a.real_copies,'mined':2};hard=set()
    for it in range(3):
        ex=build(src,bg,labels,rng,copies,hard);prior=priors_of(ex,used_pdfs())
        log(f'iter{it} examples {len(ex)} frames {sum(len(l) for _,l in ex)} prior {np.round(prior,2).tolist()}')
        if it==0 and a.shared_keyword:
            un=[p for p in range(10) if p not in used_pdfs()]
            log(f'  shared pdfs {KW}; unused {un} prior {np.round(prior[un],2).tolist()} '
                f'(clamped to rarest used {prior[sorted(used_pdfs())].min():.2f})')
        ep=1 if a.smoke else (a.epochs if it==2 else max(4,a.epochs//2))
        net=train(ex,a.dim,ep,seed=it,log=log)
        if it<2:
            labels=realign(net,src,prior);log(f'  realigned {len(labels)} positive clips')
            new=mine(net,src,prior,bg,rng);hard|=new
            log(f'  mined {len(new)} accepted negatives (total {len(hard)}): '+json.dumps(dict(__import__('collections').Counter(src[i]['text'] for i in new).most_common(12)),ensure_ascii=False))
    blob=package(net,prior,[l for _,l in ex],out);log(f'packed {out/"model.binn"} {len(blob)} bytes')
    f=ex[0][0]
    ref=F.forward(f,json.loads((out/'am-decoded.json').read_text()),(out/'root.list.3.am.bin').read_bytes())
    log(f'export check max|torch-numpy| {np.abs(posteriors(net,f).astype(np.float64)-ref).max():.2e}')
    evaluate(out,src,log)

if __name__=='__main__':main()
