"""Paired float/S16 audit on a fixed TRAIN-only augmentation subset.

Does not train or read dev/s3/s2b audio for scoring. The training source loader
uses s1 listening labels and s2 music background; homophones are excluded.
Outputs per-example metadata and scores, never transcripts or authentication data.
"""
from pathlib import Path
import argparse,collections,hashlib,importlib.util,json,time
import numpy as np

R=Path(__file__).resolve().parents[1]
def module():
 s=importlib.util.spec_from_file_location('wake_train',R/'tools/wake-train.py');m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m

def summary(rows):
 out={}
 for level in ('all','low','normal'):
  rr=rows if level=='all' else [x for x in rows if x['level']==level]
  if not rr:continue
  q=lambda k:{'median':float(np.median([x[k] for x in rr])),'p95':float(np.percentile([x[k] for x in rr],95)),'max':float(max(x[k] for x in rr))}
  out[level]={'examples':len(rr),'active_dbfs_range':[min(x['active_dbfs'] for x in rr),max(x['active_dbfs'] for x in rr)],
   'mfcc_active_mae':q('mfcc_active_mae'),'mfcc_active_max':q('mfcc_active_max'),'mfcc_c0_mae':q('mfcc_c0_mae'),
   'logposterior_active_mae':q('logposterior_active_mae'),'frame_argmax_change':q('frame_argmax_change'),'active_argmax_change':q('active_argmax_change'),'margin_min_delta_abs':q('margin_min_delta_abs'),
   'acceptance_flips':sum(x['float_accept']!=x['s16_accept'] for x in rr),
   'by_group':{k:{'n':sum(x['group']==k for x in rr),'float_accepted':sum(x['float_accept'] for x in rr if x['group']==k),'s16_accepted':sum(x['s16_accept'] for x in rr if x['group']==k),'flips':sum(x['float_accept']!=x['s16_accept'] for x in rr if x['group']==k)} for k in sorted({x['group'] for x in rr})}}
 return out

def main():
 ap=argparse.ArgumentParser();ap.add_argument('--per-group',type=int,default=12);ap.add_argument('--repeats',type=int,default=2);ap.add_argument('--seed',type=int,default=3501);ap.add_argument('--model',default='temp/analysis/wake-train/d96-v5');ap.add_argument('--out',default='temp/analysis/wake-pcm-audit');a=ap.parse_args()
 t0=time.time();m=module();d=R/a.model;out=R/a.out;out.mkdir(parents=True,exist_ok=True)
 src,bg=m.sources();groups=collections.defaultdict(list)
 for s in src:
  if s['split']!='train' or s['sub']=='homophone':continue
  if s['real'] and s['kind']=='pos':groups['real_pos'].append(s)
  elif not s['real'] and s['kind']=='pos':groups['tts_pos'].append(s)
  elif not s['real'] and s['sub']=='hardneg':groups['train_hardneg'].append(s)
  elif not s['real'] and s['sub']=='neg':groups['train_neg'].append(s)
 select=np.random.default_rng(a.seed);picked=[]
 for g,ss in sorted(groups.items()):
  ss=sorted(ss,key=lambda s:s['id']);ii=select.choice(len(ss),min(a.per_group,len(ss)),replace=False)
  picked.extend((g,ss[int(i)]) for i in sorted(ii))
 net=m.Net(96);net.load_state_dict(m.torch.load(d/'net.pt',map_location='cpu',weights_only=True));net.eval()
 fst=json.loads((d/'fst-decoded.json').read_text());prior=np.array(json.loads((d/'prior.json').read_text()));rng=np.random.default_rng(a.seed+1);rows=[]
 for j,(g,s) in enumerate(picked):
  assert s['split']=='train' and s['sub']!='homophone'
  for repeat in range(a.repeats):
   y,pre,r=m.augment(s['pcm'],rng,bg,s['real']);z=np.rint(np.clip(y,-32768,32767)).astype(np.int16)
   f=m.F.mfcc(y);q=m.F.mfcc(z);assert f.shape==q.shape
   # Source activity is used only to locate the speech region after pad/speed.
   e=m.frame_db(s['pcm']);source_active=e>max(e.max()-30,-100)
   mask=np.zeros(len(f),bool);base=source_active[np.minimum((np.arange(int(len(source_active)/r))*r).astype(int),len(source_active)-1)]
   n=min(len(base),len(mask)-pre);mask[pre:pre+n]=base[:n]
   if not mask.any():mask[:]=True
   ey=m.frame_db(y);db=float(10*np.log10(np.mean(10**(ey[mask]/10))))
   p=m.posteriors(net,f);pq=m.posteriors(net,q);mf=m.F.viterbi(p,fst,prior);mq=m.F.viterbi(pq,fst,prior);finite=np.isfinite(mf)&np.isfinite(mq)
   delta=np.abs(f-q);dp=np.abs(p-pq);change=p.argmax(1)!=pq.argmax(1)
   row={'group':g,'id':s['id'],'split':s['split'],'repeat':repeat,'real':s['real'],'source_pcm_sha256':hashlib.sha256(s['pcm'].tobytes()).hexdigest(),'aug_float_sha256':hashlib.sha256(y.tobytes()).hexdigest(),'quant_s16_sha256':hashlib.sha256(z.tobytes()).hexdigest(),'frames':len(f),'active_dbfs':db,'level':'low' if db<-50 else 'normal','mfcc_active_mae':float(delta[mask].mean()),'mfcc_active_max':float(delta[mask].max()),'mfcc_c0_mae':float(delta[mask,0].mean()),'mfcc_all_mae':float(delta.mean()),'logposterior_active_mae':float(dp[mask].mean()),'frame_argmax_change':float(change.mean()),'active_argmax_change':float(change[mask].mean()),'float_margin_min':float(mf.min()),'s16_margin_min':float(mq.min()),'margin_min_delta_abs':float(abs(mf.min()-mq.min())),'margin_frame_delta_mae':float(np.abs(mf[finite]-mq[finite]).mean()),'float_accept':bool(mf.min()<0),'s16_accept':bool(mq.min()<0)}
   rows.append(row)
  print(f'audited {j+1}/{len(picked)} {g}',flush=True)
 result={'config':vars(a),'model_sha256':hashlib.sha256((d/'model.binn').read_bytes()).hexdigest(),'level_definition':'low active RMS <-50dBFS; source activity mapped by augmentation pad/speed, all other levels normal','elapsed_s':time.time()-t0,'summary':summary(rows),'rows':rows}
 (out/'audit.json').write_text(json.dumps(result,indent=2));print(json.dumps(result['summary'],indent=2));print('elapsed_s',result['elapsed_s'])
if __name__=='__main__':main()
