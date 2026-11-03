"""Group HUMAN pos fragments without reading unlabelled gap audio.
Fixed old TTS/negative/background examples are regenerated and cached. Paired
BG5/BG0 variants share all augmented PCM/features, seed and effective batch16.
Existing ASR is support metadata, never a replacement for HUMAN labels.
"""
from pathlib import Path
import argparse,copy,csv,hashlib,importlib.util,json,time,re
import numpy as np
R=Path(__file__).resolve().parents[1]
s=importlib.util.spec_from_file_location('bd',R/'tools/wake-boundary-train.py');BD=importlib.util.module_from_spec(s);s.loader.exec_module(BD)

def group_rows(rows,session,full_pcm):
 groups=[]
 for row in sorted(rows,key=lambda r:float(r['start_s'])):
  a,b=float(row['start_s']),float(row['end_s'])
  if groups and a-groups[-1]['end_s']<.5:groups[-1]['end_s']=b;groups[-1]['members'].append(row)
  else:groups.append({'session':session,'start_s':a,'end_s':b,'members':[row]})
 for g in groups:
  start=int(g['start_s']*16000);end=int(g['end_s']*16000);pcm=np.zeros(end-start,np.float32);holes=[];last=start
  for row in g['members']:
   a,b=int(float(row['start_s'])*16000),int(float(row['end_s'])*16000)
   if a>last:holes.append([last/16000,a/16000])
   pcm[a-start:b-start]=full_pcm[a:b];last=b
  counts=[[re.sub('[^\u4e00-\u9fff]','',row.get(k,'')).count('豆包') for k in ('text','text2')] for row in g['members']]
  g.update(id=f"{session}-group@{g['start_s']:.2f}",pcm=pcm,kind='pos',sub='real',real=True,split='train',text='',zero_holes=holes,zero_seconds=sum(b-a for a,b in holes),member_intervals=[[float(r['start_s']),float(r['end_s'])] for r in g['members']],member_count=len(g['members']),eligible_bg0=len(g['members'])>1 or any(min(c)>=4 for c in counts),asr_keyword_counts=counts)
 return groups

def align_group(m,lp,prior,pcm,allow_zero):
 if not allow_zero:return BD.align_bounded(m,lp,prior,True,pcm)
 # Same bounded chain, optional direct full-chain repeat (zero BG frames).
 # A one-state BG-after supports any positive-length pause as well.
 scores=lp-prior;bg=np.maximum(scores[:,0],scores[:,3]);T=len(lp);H=40;K=8;minimum=4;after=1+K*H;N=after+1;neg=-1e18;e=m.frame_db(pcm);act=np.flatnonzero(e>e.max()-30);lo=max(0,int(act[0])-5);hi=min(T,int(act[-1])+6);kw=scores[:,m.KW].copy();kw[:lo]=neg;kw[hi:]=neg
 d=np.full(N,neg);d[0]=0;bp=np.zeros((T,N),np.int16)
 for t in range(T):
  nxt=np.full(N,neg);parents=np.arange(N,dtype=np.int16);nxt[0]=d[0]+bg[t];dk=d[1:after].reshape(K,H);nk=nxt[1:after].reshape(K,H);pk=parents[1:after].reshape(K,H);nk[:,1:]=dk[:,:-1]+kw[t,:,None];pk[:,1:]=np.arange(1,after).reshape(K,H)[:,:-1];starts=np.empty(K);arg=np.empty(K,np.int16);starts[0]=d[0];arg[0]=0
  if d[-1]>starts[0]:starts[0]=d[-1];arg[0]=N-1
  lastage=int(np.argmax(dk[-1,minimum-1:]))+minimum-1
  if dk[-1,lastage]>starts[0]:starts[0]=dk[-1,lastage];arg[0]=1+(K-1)*H+lastage
  ages=np.argmax(dk[:-1,minimum-1:],1)+minimum-1;starts[1:]=dk[:-1][np.arange(K-1),ages];arg[1:]=1+np.arange(K-1)*H+ages;nk[:,0]=starts+kw[t];pk[:,0]=arg
  nxt[after]=dk[-1,lastage]+bg[t];parents[after]=1+(K-1)*H+lastage
  if d[-1]+bg[t]>nxt[-1]:nxt[-1]=d[-1]+bg[t];parents[-1]=N-1
  d=nxt;bp[t]=parents
 terminals=list(range(after,N))+list(range(1+(K-1)*H+minimum-1,after));end=max(terminals,key=lambda q:d[q]);assert d[end]>neg/2
 path=[]
 for t in range(T-1,-1,-1):path.append(end);end=int(bp[t,end])
 path=np.array(path[::-1]);lab=np.where(scores[:,0]>=scores[:,3],0,3).astype(np.int64);mask=(path>=1)&(path<after);lab[mask]=np.array(m.KW)[(path[mask]-1)//H];zero=np.all(np.lib.stride_tricks.sliding_window_view(pcm,400)[::160]==0,axis=1);lab[zero]=np.where(scores[zero,0]>=scores[zero,3],0,3);return lab

def fit(m,net,ex,prior,epochs):
 m.torch.manual_seed(3504);opt=m.torch.optim.Adam(net.parameters(),lr=1e-4,weight_decay=1e-5);rng=np.random.default_rng(3504);pt=m.torch.tensor(prior,dtype=m.torch.float32)[None,:,None]
 for ep in range(epochs):
  order=rng.permutation(len(ex));start=time.time();tot=0;n=0
  for off in range(0,len(order),16):
   logical=[ex[i] for i in order[off:off+16]];valid=sum(len(l) for _,l in logical);bgcount=sum(int(((l==0)|(l==3)).sum()) for _,l in logical);opt.zero_grad();loss_total=0
   for moff in range(0,len(logical),8):
    batch=logical[moff:moff+8];T=max(len(f) for f,_ in batch);X=np.stack([m.padded(np.concatenate([f,np.repeat(f[-1:],T-len(f),0)])) for f,_ in batch]).transpose(0,2,1).astype(np.float32);Y=np.full((len(batch),T),-100,np.int64)
    for j,(_,l) in enumerate(batch):Y[j,:len(l)]=l
    yt=m.torch.from_numpy(Y);lp=net(m.torch.from_numpy(X));ce=m.nn.functional.nll_loss(lp,yt,ignore_index=-100);lik=lp-pt;mask=(yt==0)|(yt==3);delta=lik[:,m.KW].max(1).values-lik[:,[0,3]].max(1).values;hinge=m.torch.relu(delta[mask]).mean();loss=ce*(sum(len(l) for _,l in batch)/valid)+hinge*(int(mask.sum())/bgcount);loss.backward();loss_total+=float(loss.item())
   m.torch.nn.utils.clip_grad_norm_(net.parameters(),1.);opt.step();tot+=loss_total;n+=1
  print(f'epoch {ep+1}/{epochs} loss {tot/n:.4f} seconds {time.time()-start:.2f}',flush=True)
 return net

def main():
 ap=argparse.ArgumentParser();ap.add_argument('--epochs',type=int,choices=(1,2,3),default=3);ap.add_argument('--out',default='temp/analysis/wake-group-train');a=ap.parse_args();out=R/a.out;out.mkdir(parents=True,exist_ok=True);m=BD.FT.module();src,bg=m.sources();lookup={x['id']:x for x in src};hl=m.human();posrows={}
 for session in ('s1-pos-quiet','s3-music-pos'):
  rows=[r for r in csv.DictReader(open(m.C/f'{session}.segments.csv')) if hl.get((session,r['start_s']))=='pos'];posrows[session]=rows
  if session.startswith('s3'):
   full=m.wav(m.C/f'{session}.aec16k.wav')
   for r in rows:lookup['s3@'+r['start_s']]=dict(id='s3@'+r['start_s'],kind='pos',sub='real',real=True,split='train',text='',pcm=full[int(float(r['start_s'])*16000):int(float(r['end_s'])*16000)])
 groups=[]
 for session,rows in posrows.items():groups.extend(group_rows(rows,session,m.wav(m.C/f'{session}.aec16k.wav')))
 s3groups=[g for g in groups if g['session'].startswith('s3')];assert len(s3groups)==35
 ref=json.load(open(R/'temp/analysis/mfcc-audit/s3-35-matrix.json'));assert [(g['start_s'],g['end_s']) for g in s3groups]==[(g['start_s'],g['end_s']) for g in ref]
 manifest=[{k:v for k,v in g.items() if k not in ('pcm','members','text')} for g in groups];(out/'group-manifest.json').write_text(json.dumps(manifest,indent=2));print('groups',len(groups),'s3',len(s3groups),'eligibleBG0',sum(g['eligible_bg0'] for g in groups),'zero seconds',sum(g['zero_seconds'] for g in groups),flush=True)
 oldmanifest=json.load(open(R/'temp/analysis/wake-boundary-s3-strong/train-manifest.json'));oldss=[lookup[x['id']] for x in oldmanifest];assert all(x['split']=='train' for x in oldss);raw=m.F.mfcc;m.F.mfcc=lambda y:raw(np.rint(np.clip(y,-32768,32767)).astype(np.int16));dev=[x for x in src if x['split']=='dev'];results={};music_n=sum(1 for _ in range(0,len(bg['music'])-48000,24000))
 for level,D,name in ((1,96,'d96-v5'),(2,160,'l2-d160')):
  m.M=R/'temp/analysis/models'/('mdata_wakeup_mobile' if level==1 else 'mdata_wakeup_mobile_clone');base=R/'temp/analysis/wake-train'/name;net=m.Net(D);net.load_state_dict(m.torch.load(base/'net.pt',weights_only=True));net.eval();prior=np.array(json.load(open(base/'prior.json')));fst=json.load(open(base/'fst-decoded.json'));oldlabels={};hard={i for i,x in enumerate(oldss) if x['id'].startswith('s3@')}
  for i,x in enumerate(oldss):
   if x['kind']=='pos':
    pcm=x['pcm'] if x['real'] else x['pcm']*10**((-45-max(m.frame_db(x['pcm'])))/20)+np.random.default_rng(i).standard_normal(len(x['pcm']))*.5;pcm=np.rint(np.clip(pcm,-32768,32767)).astype(np.int16);oldlabels[i]=BD.align_bounded(m,m.posteriors(net,m.F.mfcc(pcm)),prior,x['real'],pcm)
   elif m.F.viterbi(m.posteriors(net,m.F.mfcc(x['pcm'])),fst,prior).min()<0:hard.add(i)
  copies={'pos':2,'neg':1,'hardneg':3,'real':2,'mined':2};oldex=m.build(oldss,bg,oldlabels,np.random.default_rng(3503),copies,hard);cached=[];offset=0;cached_ids=[]
  for i,x in enumerate(oldss):
   n=copies['real' if x['real'] else x['sub'] if x['sub'] in copies else x['kind']]+(2 if i in hard else 0)
   if not (x['real'] and x['kind']=='pos'):cached.extend(oldex[offset:offset+n]);cached_ids.append(x['id'])
   offset+=n
  assert offset==len(oldex)-music_n;cached.extend(oldex[-music_n:]*3);expected=(1378,370830) if level==1 else (1358,367610);assert (len(oldex)+music_n*2,sum(len(l) for _,l in oldex)+sum(len(l) for _,l in oldex[-music_n:])*2)==expected
  print('level',level,'cached original negative/TTS/background',len(cached),flush=True);reference_features=None
  for mode in ('bg5','adaptive-bg0'):
   labels={};audit=[]
   for i,g in enumerate(groups):
    pcm=np.rint(g['pcm']).astype(np.int16);allow=mode=='adaptive-bg0' and g['eligible_bg0'];labels[i]=align_group(m,m.posteriors(net,m.F.mfcc(pcm)),prior,pcm,allow);audit.append({'id':g['id'],'bg_min_frames':0 if allow else 5,'stats':BD.stats(m,labels[i],pcm)})
   hardgroup={i for i,g in enumerate(groups) if g['session'].startswith('s3')};gex=m.build(groups,bg,labels,np.random.default_rng(3503),{'real':2,'pos':2,'neg':1,'mined':2},hardgroup)[:-music_n];ex=cached+gex
   digest=hashlib.sha256(b''.join(f.tobytes() for f,_ in ex)).hexdigest()
   if reference_features is None:reference_features=digest
   else:assert digest==reference_features
   d=out/f'l{level}-{mode}';d.mkdir(exist_ok=True);(d/'alignment.json').write_text(json.dumps(audit,indent=2));print(level,mode,'examples',len(ex),'frames',sum(len(l) for _,l in ex),'shared_features',digest,flush=True);updated=fit(m,copy.deepcopy(net),ex,prior,a.epochs);result=BD.package(m,updated,base,d,ex);res=BD.evaluate(m,updated,base,d,dev);summary,matrix=BD.summarize(m,res,ref);result.update(summary);result['feature_sha256']=digest;result['examples']=len(ex);result['frames']=sum(len(l) for _,l in ex);(d/'matrix.json').write_text(json.dumps(matrix,indent=2));results[d.name]=result;(out/'results.json').write_text(json.dumps(results,indent=2));print(d.name,'groups',result['s3_groups'],'music',result['s2b-music-silent']['events'],result['s2-music-silent']['events'],'dev',result['dev'],flush=True)
if __name__=='__main__':main()
