"""Bounded S16 boundary alignment and hard-background fine-tune experiment.
Only train sources enter labels/gradients; s3 remains known tuning validation.
Preserves factory BINN control/prior/FST/transition bytes, changes AM only.
"""
from pathlib import Path
import argparse,copy,hashlib,importlib.util,json,time
import numpy as np
R=Path(__file__).resolve().parents[1]
s=importlib.util.spec_from_file_location('ft',R/'tools/wake-pcm-finetune.py');FT=importlib.util.module_from_spec(s);s.loader.exec_module(FT)

def align_bounded(m,lp,prior,loop,pcm,minimum=4,maximum=40,gap=5):
 scores=lp-prior;bg=np.maximum(scores[:,0],scores[:,3]);T=len(lp);H=maximum;K=8;N=1+K*H+gap;after=1+K*H;neg=-1e18
 e=m.frame_db(pcm);act=np.flatnonzero(e>e.max()-30);lo=max(0,int(act[0])-5);hi=min(T,int(act[-1])+6);kw=scores[:,m.KW].copy();kw[:lo]=neg;kw[hi:]=neg
 d=np.full(N,neg);d[0]=0;bp=np.zeros((T,N),np.int16)
 for t in range(T):
  nxt=np.full(N,neg);parents=np.arange(N,dtype=np.int16);nxt[0]=d[0]+bg[t];dk=d[1:after].reshape(K,H);nk=nxt[1:after].reshape(K,H);pk=parents[1:after].reshape(K,H)
  nk[:,1:]=dk[:,:-1]+kw[t,:,None];pk[:,1:]=np.arange(1,after).reshape(K,H)[:,:-1]
  starts=np.empty(K);arg=np.empty(K,np.int16);starts[0]=d[0];arg[0]=0
  if loop and d[-1]>starts[0]:starts[0]=d[-1];arg[0]=N-1
  ages=np.argmax(dk[:-1,minimum-1:],1)+minimum-1;starts[1:]=dk[:-1][np.arange(K-1),ages];arg[1:]=1+np.arange(K-1)*H+ages
  nk[:,0]=starts+kw[t];pk[:,0]=arg
  age=int(np.argmax(dk[-1,minimum-1:]))+minimum-1;nxt[after]=dk[-1,age]+bg[t];parents[after]=1+(K-1)*H+age
  nxt[after+1:]=d[after:-1]+bg[t];parents[after+1:]=np.arange(after,N-1)
  if d[-1]+bg[t]>nxt[-1]:nxt[-1]=d[-1]+bg[t];parents[-1]=N-1
  d=nxt;bp[t]=parents
 terminals=list(range(after,N))+list(range(1+(K-1)*H+minimum-1,after));end=max(terminals,key=lambda i:d[i]);assert d[end]>neg/2,'bounded alignment infeasible'
 path=[]
 for t in range(T-1,-1,-1):path.append(end);end=int(bp[t,end])
 path=np.array(path[::-1]);lab=np.where(scores[:,0]>=scores[:,3],0,3).astype(np.int64);k=(path>=1)&(path<after);lab[k]=np.array(m.KW)[(path[k]-1)//H]
 # Only exactly all-zero S16 frames are background supervision; absolute low RMS is not enough.
 zero=np.all(np.lib.stride_tricks.sliding_window_view(pcm,400)[::160]==0,axis=1)
 lab[zero]=np.where(scores[zero,0]>=scores[zero,3],0,3)
 return lab

def stats(m,lab,pcm):
 e=m.frame_db(pcm);idx=np.flatnonzero(e>e.max()-30);a=max(0,idx[0]-5);b=min(len(e),idx[-1]+6);kw=np.isin(lab,m.KW);outside=kw.copy();outside[a:b]=False;starts=np.r_[0,np.flatnonzero(np.diff(lab)!=0)+1];ends=np.r_[starts[1:],len(lab)]
 runs=[{'pdf':int(lab[a]),'frames':int(b-a),'dbfs':float(10*np.log10(np.mean(10**(e[a:b]/10)))),'nearzero_frames':int((e[a:b]<-85).sum())} for a,b in zip(starts,ends) if lab[a] in m.KW]
 return {'kw_frames':int(kw.sum()),'outside_boundary':int(outside.sum()),'nearzero_kw':int((kw&(e<-85)).sum()),'runs':runs}

def fit_competition(m,net,ex,prior,epochs,seed,weight=.25):
 m.torch.manual_seed(seed);opt=m.torch.optim.Adam(net.parameters(),lr=1e-4,weight_decay=1e-5);rng=np.random.default_rng(seed);pt=m.torch.tensor(prior,dtype=m.torch.float32)[None,:,None]
 for ep in range(epochs):
  order=rng.permutation(len(ex));start=time.time();ce_sum=hinge_sum=n=0
  for off in range(0,len(order),16):
   batch=[ex[i] for i in order[off:off+16]];T=max(len(f) for f,_ in batch);X=np.stack([m.padded(np.concatenate([f,np.repeat(f[-1:],T-len(f),0)])) for f,_ in batch]).transpose(0,2,1).astype(np.float32);Y=np.full((len(batch),T),-100,np.int64)
   for j,(_,l) in enumerate(batch):Y[j,:len(l)]=l
   yt=m.torch.from_numpy(Y);lp=net(m.torch.from_numpy(X));ce=m.nn.functional.nll_loss(lp,yt,ignore_index=-100);lik=lp-pt;mask=(yt==0)|(yt==3);delta=lik[:,m.KW].max(1).values-lik[:,[0,3]].max(1).values;hinge=m.torch.relu(delta[mask]).mean();loss=ce+weight*hinge;opt.zero_grad();loss.backward();m.torch.nn.utils.clip_grad_norm_(net.parameters(),1.);opt.step();ce_sum+=float(ce.item());hinge_sum+=float(hinge.item());n+=1
  print(f'competition epoch {ep+1}/{epochs} ce {ce_sum/n:.4f} hinge {hinge_sum/n:.4f} seconds {time.time()-start:.2f}',flush=True)
 return net

def competition_audit(m,net,ex,prior):
 count=raw=corrected=0;deltas=[]
 for f,l in ex:
  mask=(l==0)|(l==3)
  if not mask.any():continue
  lp=m.posteriors(net,f);delta=lp[:,m.KW].max(1)-lp[:,[0,3]].max(1);lik=lp-prior;dc=lik[:,m.KW].max(1)-lik[:,[0,3]].max(1);count+=int(mask.sum());raw+=int((delta[mask]>0).sum());corrected+=int((dc[mask]>0).sum());deltas.extend(dc[mask].tolist())
 return {'bg_frames':count,'raw_kw_win_fraction':raw/count,'prior_corrected_kw_win_fraction':corrected/count,'corrected_delta_p95':float(np.percentile(deltas,95)),'corrected_delta_max':float(max(deltas))}

def package(m,net,base,d,ex):
 parsed,am=m.export(net);top,_=m.P.load((base/'model.binn').read_bytes());parts={k:v for n in top['list'] for k,v in n.items()};blob=m.P.container(parts['conf'],parts['model'],parts['fst'],am,parts['prior']);top,end=m.P.load(blob);new={k:v for n in top['list'] for k,v in n.items()};assert end==len(blob) and m.P.dump(top)==blob and all(new[k]==parts[k] for k in ('conf','model','fst','prior'))
 d.mkdir(parents=True,exist_ok=True);(d/'model.binn').write_bytes(blob);(d/'root.list.3.am.bin').write_bytes(am);(d/'am-decoded.json').write_text(json.dumps(parsed));m.torch.save(net.state_dict(),d/'net.pt')
 for name in ('prior.json','fst-decoded.json'):(d/name).write_bytes((base/name).read_bytes())
 f=ex[0][0];err=float(np.abs(m.posteriors(net,f)-m.F.forward(f,parsed,am)).max());assert err<2e-5
 return {'sha256':hashlib.sha256(blob).hexdigest(),'bytes':len(blob),'export_maxerr':err}

def evaluate(m,net,base,d,dev):
 fst=json.loads((base/'fst-decoded.json').read_text());prior=np.array(json.loads((base/'prior.json').read_text()));res={}
 for name in ('s3-music-pos','s2b-music-silent','s2-music-silent'):
  pcm=m.wav(m.C/f'{name}.aec16k.wav');v=m.F.viterbi(m.posteriors(net,m.F.mfcc(pcm)),fst,prior);res[name]=v.tolist()
 rng=np.random.default_rng(7);music=m.wav(m.C/'s2b-music-silent.aec16k.wav');bg={'quiet':music,'music':music};res['dev']=[]
 for x in dev:
  y=m.augment(x['pcm'],rng,bg,False)[0];v=m.F.viterbi(m.posteriors(net,m.F.mfcc(y)),fst,prior);res['dev'].append([x['sub'],float(v.min()),x['id']])
 (d/'eval.json').write_text(json.dumps(res));return res

def summarize(m,res,groups):
 out={};matrix=[]
 for name in ('s3-music-pos','s2b-music-silent','s2-music-silent'):
  ev=m.events(np.array(res[name]),0);out[name]={'events':len(ev),'max_event_seconds':max([(b-a+1)/100 for a,b in ev],default=0),'intervals_seconds':[[a/100,b/100] for a,b in ev]}
 mid=[(a+b)/200 for a,b in m.events(np.array(res['s3-music-pos']),0)]
 for g in groups:matrix.append({'id':g['id'],'start_s':g['start_s'],'end_s':g['end_s'],'mac_hit':any(g['start_s']-.3<=t<=g['end_s']+.5 for t in mid),'min_margin':float(np.array(res['s3-music-pos'])[max(0,int((g['start_s']-.3)*100)):int((g['end_s']+.5)*100)+1].min())})
 out['s3_groups']=sum(x['mac_hit'] for x in matrix);out['dev']={k:{'accepted':sum(x[1]<0 for x in res['dev'] if x[0]==k),'n':sum(x[0]==k for x in res['dev'])} for k in ('pos','hardneg','neg','homophone')};return out,matrix

def main():
 ap=argparse.ArgumentParser();ap.add_argument('--epochs',type=int,choices=range(1,5),default=3);ap.add_argument('--out',default='temp/analysis/wake-boundary');ap.add_argument('--include-s3',action='store_true');ap.add_argument('--s3-boost',action='store_true');ap.add_argument('--competition',action='store_true');ap.add_argument('--competition-weight',type=float,default=.25);ap.add_argument('--variants',choices=['both','bounded','bounded-hard'],default='both');a=ap.parse_args();out=R/a.out;out.mkdir(parents=True,exist_ok=True);m=FT.module();src,bg=m.sources();rng=np.random.default_rng(3502);train=[x for x in src if x['split']=='train' and x['sub']!='homophone'];ss=[x for x in train if x['real']]
 for kind,count in [('pos',48),('hardneg',64),('neg',32)]:
  pool=sorted([x for x in train if not x['real'] and (x['kind']=='pos' and x['text'] in m.PURE if kind=='pos' else x['sub']==kind)],key=lambda x:x['id']);ids=rng.choice(len(pool),min(count,len(pool)),False);ss.extend(pool[int(i)] for i in sorted(ids))
 if a.include_s3:
  import csv
  hl=m.human();pcm=m.wav(m.C/'s3-music-pos.aec16k.wav')
  for row in csv.DictReader(open(m.C/'s3-music-pos.segments.csv')):
   if hl.get(('s3-music-pos',row['start_s']))!='pos':continue
   start,end=int(float(row['start_s'])*16000),int(float(row['end_s'])*16000)
   ss.append(dict(id='s3@'+row['start_s'],kind='pos',sub='real',real=True,split='train',text='',pcm=pcm[start:end]))
  assert sum(x['id'].startswith('s3@') for x in ss)==58
 (out/'config.json').write_text(json.dumps({'arguments':vars(a),'source_seed':3502,'augmentation_seed':3503,'optimizer_seed':3504,'min_state_frames':4,'max_state_frames':40,'repeat_bg_frames':5,'competition_lambda':a.competition_weight if a.competition else 0,'optimizer':'Adam lr1e-4 weight_decay1e-5; competition clip_norm=1','s3_role':'training and known validation' if a.include_s3 else 'known tuning validation only','s2b_role':'tuning veto only; no gradients'},indent=2))
 assert len({x['id'] for x in ss})==len(ss) and all(x['split']=='train' for x in ss);(out/'train-manifest.json').write_text(json.dumps([{'id':x['id'],'split':x['split'],'kind':x['kind'],'sub':x['sub'],'real':x['real']} for x in ss],indent=2))
 raw=m.F.mfcc;m.F.mfcc=lambda y:raw(np.rint(np.clip(y,-32768,32767)).astype(np.int16));dev=[x for x in src if x['split']=='dev'];groups=json.loads((R/'temp/analysis/mfcc-audit/s3-35-matrix.json').read_text());results={}
 for level,D,name in [(1,96,'d96-v5'),(2,160,'l2-d160')]:
  m.M=R/'temp/analysis/models'/('mdata_wakeup_mobile' if level==1 else 'mdata_wakeup_mobile_clone');base=R/'temp/analysis/wake-train'/name;net=m.Net(D);net.load_state_dict(m.torch.load(base/'net.pt',weights_only=True));net.eval();prior=np.array(json.loads((base/'prior.json').read_text()));old=m.realign(net,ss,prior);labels={};audit=[]
  for i,x in enumerate(ss):
   if x['kind']!='pos':continue
   pcm=x['pcm'] if x['real'] else x['pcm']*10**((-45-max(m.frame_db(x['pcm'])))/20)+np.random.default_rng(i).standard_normal(len(x['pcm']))*.5
   pcm=np.rint(np.clip(pcm,-32768,32767)).astype(np.int16);lp=m.posteriors(net,m.F.mfcc(pcm));labels[i]=align_bounded(m,lp,prior,x['real'],pcm);audit.append({'id':x['id'],'real':x['real'],'old':stats(m,old[i],pcm),'bounded':stats(m,labels[i],pcm)})
  (out/f'l{level}-alignment.json').write_text(json.dumps(audit,indent=2));fst=json.loads((base/'fst-decoded.json').read_text());hard=set()
  for i,x in enumerate(ss):
   if x['kind']=='neg' and m.F.viterbi(m.posteriors(net,m.F.mfcc(x['pcm'])),fst,prior).min()<0:hard.add(i)
  print(f'level{level} sources {len(ss)} labels {len(labels)} mined train negatives {len(hard)}',flush=True)
  if a.s3_boost:
   assert a.include_s3
   hard|={i for i,x in enumerate(ss) if x['id'].startswith('s3@')}
  for variant in (('bounded','bounded-hard') if a.variants=='both' else (a.variants,)):
   copies={'pos':2,'neg':1,'hardneg':1 if variant=='bounded' else 3,'real':2,'mined':0 if variant=='bounded' else 2};ex=m.build(ss,bg,labels,np.random.default_rng(3503),copies,hard)
   if variant=='bounded-hard':
    music=ex[-sum(1 for _ in range(0,len(bg['music'])-48000,24000)):];ex+=music*2
   print(level,variant,'examples',len(ex),'frames',sum(len(l) for _,l in ex),flush=True);updated=fit_competition(m,copy.deepcopy(net),ex,prior,a.epochs,3504,a.competition_weight) if a.competition else FT.fit(m,copy.deepcopy(net),ex,a.epochs,3504);d=out/f'l{level}-{variant}';result=package(m,updated,base,d,ex);res=evaluate(m,updated,base,d,dev);summary,matrix=summarize(m,res,groups);result.update(summary);probe_ex=ex[::max(1,len(ex)//32)];result['competition_audit']={'prior':prior.tolist(),'baseline':competition_audit(m,net,probe_ex,prior),'candidate':competition_audit(m,updated,probe_ex,prior)};ce_path=R/f'temp/analysis/wake-boundary-nullguard/l{level}-bounded-hard/net.pt';
   if ce_path.exists():
    ce_net=m.Net(D);ce_net.load_state_dict(m.torch.load(ce_path,weights_only=True));result['competition_audit']['ce_candidate']=competition_audit(m,ce_net,probe_ex,prior)
   (d/'matrix.json').write_text(json.dumps(matrix,indent=2));results[d.name]=result;(out/'results.json').write_text(json.dumps(results,indent=2));print(d.name,json.dumps({k:v for k,v in result.items() if k not in ('s3-music-pos','s2b-music-silent','s2-music-silent')}),flush=True)
if __name__=='__main__':main()
