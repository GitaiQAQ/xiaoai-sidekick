"""Audit existing ASR support for HUMAN pos fragments, without relabelling them."""
from pathlib import Path
import csv,json,collections,re,argparse
R=Path(__file__).resolve().parents[1];C=R/'private/inputs/recordings/wake-corpus'
def chars(t):return ''.join(re.findall('[\u4e00-\u9fff]',t or ''))
def evidence(row):
 counts=[chars(row.get(k,'')).count('豆包') for k in ('text','text2')]
 if min(counts)>=2:kind='both_complete_support'
 elif max(counts)>=2:kind='one_complete_support'
 elif counts==[1,1]:kind='both_single_support'
 elif max(counts)==1:kind='one_single_ambiguous'
 else:kind='no_exact_keyword_ambiguous'
 return dict(asr_keyword_counts=counts,evidence=kind,text=row.get('text',''),text2=row.get('text2',''))
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--out',default='temp/analysis/wake-group-labels');a=ap.parse_args();out=R/a.out;out.mkdir(parents=True,exist_ok=True);labels={(r['session'],r['start_s']):r['label'] for r in csv.DictReader(open(C/'review/labels.csv'))};sessions={}
 for session in ('s1-pos-quiet','s3-music-pos'):
  rows=[]
  for row in csv.DictReader(open(C/f'{session}.segments.csv')):
   if labels.get((session,row['start_s']))!='pos':continue
   rows.append(dict(session=session,start_s=float(row['start_s']),end_s=float(row['end_s']),start_key=row['start_s'],**evidence(row)))
  sessions[session]=sorted(rows,key=lambda x:x['start_s'])
 groups=[]
 for row in sessions['s3-music-pos']:
  if groups and row['start_s']-groups[-1]['end_s']<.5:groups[-1]['end_s']=row['end_s'];groups[-1]['members'].append(row)
  else:groups.append({'id':len(groups)+1,'start_s':row['start_s'],'end_s':row['end_s'],'members':[row]})
 assert len(sessions['s3-music-pos'])==58 and len(groups)==35
 for g in groups:
  g['multi_fragment']=len(g['members'])>1;g['asr_supports_multiple_full']=any(min(x['asr_keyword_counts'])>=4 for x in g['members']);g['repeat_gap_zero_eligible']=g['multi_fragment'] or g['asr_supports_multiple_full'];g['zero_fill_seconds']=sum(max(0,b['start_s']-a['end_s']) for a,b in zip(g['members'],g['members'][1:]));g['asr_joined_counts']=[chars(''.join(x[k] for x in g['members'])).count('豆包') for k in ('text','text2')]
 old=json.load(open(R/'temp/analysis/mfcc-audit/alignment-tail-audit.json'));short={x['id'].split('@')[1] for x in old if x['real'] and x['kw_frames']<=16};shortrows=[x for x in sessions['s1-pos-quiet'] if x['start_key'] in short];assert len(shortrows)==20
 result={'sessions':sessions,'s3_groups':groups,'s1_short_20':shortrows,'summary':{'s3':dict(collections.Counter(x['evidence'] for x in sessions['s3-music-pos'])),'s1_short_20':dict(collections.Counter(x['evidence'] for x in shortrows)),'s3_multi_fragment_groups':sum(g['multi_fragment'] for g in groups),'zero_fill_seconds':sum(g['zero_fill_seconds'] for g in groups),'note':'ASR evidence only; no human label changes'}}
 (out/'semantics.json').write_text(json.dumps(result,ensure_ascii=False,indent=2));print(json.dumps(result['summary'],ensure_ascii=False))
if __name__=='__main__':main()
