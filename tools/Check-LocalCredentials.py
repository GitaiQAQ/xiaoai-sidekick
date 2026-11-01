"""Print presence/shape flags only. Never print credentials or parsed contents."""
from pathlib import Path
import json,re,sys
r=Path(__file__).resolve().parents[1]
p=Path(sys.argv[1]) if len(sys.argv)>1 else r/'private/credentials.json'
if not p.is_file():
    print('credentials_file_present=0');sys.exit(2)
if p.stat().st_size>65536:
    print('credentials_file_invalid=size_limit');sys.exit(2)
try:
    d=json.loads(p.read_text(encoding='utf-8-sig'))
except (ValueError,OSError,UnicodeError):
    print('credentials_file_invalid=json_or_read_error');sys.exit(2)
if not isinstance(d,dict) or d.get('schema_version')!=1:
    print('credentials_file_invalid=schema');sys.exit(2)
def field(section,key):
    s=d.get(section)
    return s.get(key) if isinstance(s,dict) else None
def key_shape(v):
    return isinstance(v,str) and bool(re.fullmatch(r'[\x21-\x7e]{1,4096}',v))
flags={'ark_api_key_shape_ready':key_shape(field('ark','api_key')),
       'ark_model_shape_ready':bool(isinstance(field('ark','model'),str) and re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_.:-]{0,127}',field('ark','model'))),
       'realtime_api_key_shape_ready':key_shape(field('realtime','api_key'))}
print('credentials_file_present=1')
for k,v in flags.items():print(k+'='+str(int(v)))
print('authenticated_calls_performed=0')
sys.exit(0 if all(flags.values()) else 2)
