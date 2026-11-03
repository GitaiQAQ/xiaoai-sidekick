"""Bounded paired float/S16 fine-tune from v5, TRAIN data only.
Preserves container conf/FST/prior/TransitionModel to isolate network adaptation.
Mac s3/music scores are diagnostic validation, never ARM acceptance proof.
"""
import argparse,copy,hashlib,json,math,time
from pathlib import Path
import numpy as np
import importlib.util
_spec=importlib.util.spec_from_file_location("audit",Path(__file__).with_name("wake-pcm-audit.py"));_audit=importlib.util.module_from_spec(_spec);_spec.loader.exec_module(_audit);module=_audit.module

R=Path(__file__).resolve().parents[1]
def fit(m,net,ex,epochs,seed):
 m.torch.manual_seed(seed);opt=m.torch.optim.Adam(net.parameters(),lr=1e-4,weight_decay=1e-5);rng=np.random.default_rng(seed)
 for ep in range(epochs):
  order=rng.permutation(len(ex));t=time.time();loss_sum=0;n=0
  for off in range(0,len(order),16):
   batch=[ex[i] for i in order[off:off+16]];T=max(len(f) for f,_ in batch)
   X=np.stack([m.padded(np.concatenate([f,np.repeat(f[-1:],T-len(f),0)])) for f,_ in batch]).transpose(0,2,1).astype(np.float32)
   Y=np.full((len(batch),T),-100,np.int64)
   for j,(_,lab) in enumerate(batch):Y[j,:len(lab)]=lab
   y=m.torch.from_numpy(Y);pred=net(m.torch.from_numpy(X));loss=m.nn.functional.nll_loss(pred,y,ignore_index=-100);opt.zero_grad();loss.backward();opt.step();loss_sum+=loss.item();n+=1
  print(f'epoch {ep+1}/{epochs} loss {loss_sum/n:.4f} seconds {time.time()-t:.2f}',flush=True)
 return net

def main():
 ap=argparse.ArgumentParser();ap.add_argument('--epochs',type=int,choices=range(1,5),default=3);ap.add_argument('--seed',type=int,default=3502);ap.add_argument('--base',default='temp/analysis/wake-train/d96-v5');ap.add_argument('--out',default='temp/analysis/wake-pcm-finetune');a=ap.parse_args();m=module();out=R/a.out;out.mkdir(parents=True,exist_ok=True);base=R/a.base;src,bg=m.sources();rng=np.random.default_rng(a.seed)
 train=[s for s in src if s['split']=='train' and s['sub']!='homophone'];ss=[s for s in train if s['real']]
 for kind,count in [('pos',48),('hardneg',64),('neg',32)]:
  candidates=sorted([s for s in train if not s['real'] and (s['kind']=='pos' and s['text'] in m.PURE if kind=='pos' else s['sub']==kind)],key=lambda s:s['id'])
  ids=rng.choice(len(candidates),min(count,len(candidates)),replace=False);ss.extend(candidates[int(i)] for i in sorted(ids))
 assert all(s['split']=='train' and s['sub']!='homophone' for s in ss)
 net=m.Net(96);net.load_state_dict(m.torch.load(base/'net.pt',weights_only=True,map_location='cpu'));net.eval();prior=np.array(json.loads((base/'prior.json').read_text()));labels=m.realign(net,ss,prior)
 (out/'train-manifest.json').write_text(json.dumps({'seed':a.seed,'epochs':a.epochs,'sources':[{'id':s['id'],'split':s['split'],'kind':s['kind'],'real':s['real'],'sub':s['sub']} for s in ss]},indent=2));print('sources',len(ss),'real positives',sum(s['real'] and s['kind']=='pos' for s in ss),'labels',len(labels),flush=True)
 raw_mfcc=m.F.mfcc;top,_=m.P.load((base/'model.binn').read_bytes());parts={k:v for d in top['list'] for k,v in d.items()};results={}
 for mode in ('float','s16'):
  m.F.mfcc=raw_mfcc if mode=='float' else lambda y:raw_mfcc(np.rint(np.clip(y,-32768,32767)).astype(np.int16))
  ex=m.build(ss,bg,labels,np.random.default_rng(a.seed+1),{'pos':2,'neg':1,'hardneg':1,'real':2,'mined':0});m.F.mfcc=raw_mfcc
  print(mode,'examples',len(ex),'frames',sum(len(l) for _,l in ex),flush=True);updated=fit(m,copy.deepcopy(net),ex,a.epochs,a.seed+2);d=out/mode;d.mkdir(exist_ok=True);parsed,am=m.export(updated);blob=m.P.container(parts['conf'],parts['model'],parts['fst'],am,parts['prior']);new,_=m.P.load(blob);newparts={k:v for n in new['list'] for k,v in n.items()};assert all(newparts[k]==parts[k] for k in ('conf','model','fst','prior'))
  (d/'model.binn').write_bytes(blob);(d/'root.list.3.am.bin').write_bytes(am);(d/'am-decoded.json').write_text(json.dumps(parsed));m.torch.save(updated.state_dict(),d/'net.pt')
  for name in ('prior.json','fst-decoded.json'):(d/name).write_bytes((base/name).read_bytes())
  f=ex[0][0];err=float(np.abs(m.posteriors(updated,f)-m.F.forward(f,parsed,am)).max());res={'export_maxerr':err,'sha256':hashlib.sha256(blob).hexdigest(),'bytes':len(blob),'examples':len(ex),'frames':sum(len(l) for _,l in ex)}
  for name in ('s3-music-pos','s2b-music-silent','s2-music-silent'):
   margins=m.F.viterbi(m.posteriors(updated,m.F.mfcc(m.wav(m.C/f'{name}.aec16k.wav'))),json.loads((d/'fst-decoded.json').read_text()),prior);(d/f'{name}.margins.json').write_text(json.dumps(margins.tolist()));res[name+'_events']=len(m.events(margins,0))
  results[mode]=res;print(json.dumps(res),flush=True)
 (out/'result.json').write_text(json.dumps(results,indent=2))
if __name__=='__main__':main()
