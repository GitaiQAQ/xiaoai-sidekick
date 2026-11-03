#!/usr/bin/env python3
"""Tune verified BINN configuration/FST costs; preserve AM/prior/model bytes."""
import argparse,hashlib,importlib.util,json
from pathlib import Path
s=importlib.util.spec_from_file_location('pack',Path(__file__).with_name('native-model-pack.py'));p=importlib.util.module_from_spec(s);s.loader.exec_module(p)
a=argparse.ArgumentParser();a.add_argument('base',type=Path);a.add_argument('output',type=Path);a.add_argument('--score-threshold',type=float);a.add_argument('--wait-frames',type=int);a.add_argument('--min-self-loop',type=int);a.add_argument('--keyword-step-weight',type=float);a.add_argument('--fst-json',type=Path);v=a.parse_args()
b=v.base.read_bytes();top,end=p.load(b);assert end==len(b)
parts={k:x for item in top['list'] for k,x in item.items()};conf,_=p.load(parts['conf'])
if v.score_threshold is not None:
 assert -10<=v.score_threshold<=10
 conf['online_conf']['--score-threshold']=v.score_threshold
if v.wait_frames is not None:
 assert 60<=v.wait_frames<=150
 conf['online_conf']['--wakeup-wait-frame']=v.wait_frames
parts['conf']=p.Blob(p.dump(conf))
if v.min_self_loop is not None:
 assert v.min_self_loop in (0,1,2)
 conf['online_conf']['--min-self-loop-frame']=v.min_self_loop
 parts['conf']=p.Blob(p.dump(conf))
if v.keyword_step_weight is not None:
 assert 1<=v.keyword_step_weight<=4 and v.fst_json
 f=json.loads(v.fst_json.read_text());g=f['graph'];assert p.fst_blob(g,f['start'])==parts['fst'],'FST reference does not match base'
 changed=0
 for state in g:
  for arc in state['arcs']:
   if arc['weight']>1.0 and arc['input']!=0 and arc['next']!=state['state']:
    assert arc['input']!=0 and arc['next']!=state['state']
    arc['weight']=v.keyword_step_weight;changed+=1
 assert changed==8,'unexpected keyword graph'
 parts['fst']=p.Blob(p.fst_blob(g,f['start']))
out=p.container(*[parts[k] for k in ['conf','model','fst','am','prior']]);v.output.parent.mkdir(parents=True,exist_ok=True);v.output.write_bytes(out)
print(json.dumps({'base_sha256':hashlib.sha256(b).hexdigest(),'sha256':hashlib.sha256(out).hexdigest(),'bytes':len(out),'am_sha256':hashlib.sha256(parts['am']).hexdigest(),'score_threshold':v.score_threshold,'wait_frames':v.wait_frames,'min_self_loop':v.min_self_loop,'keyword_step_weight':v.keyword_step_weight}))
