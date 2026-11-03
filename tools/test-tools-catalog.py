"""Compile the real C catalog and validate its schema and transport capacities."""
from pathlib import Path
import json,re,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='lx06-tools-catalog-') as temp:
    c=Path(temp)/'catalog.c';exe=Path(temp)/'catalog'
    c.write_text('#include "native_tools.h"\nint main(void){char b[32768];if(!lx_tool_action_allowed("alarm_list",LX_TOOL_VOICE))return 1;int n=lx_tools_json(LX_TOOL_VOICE,b,sizeof(b));if(n<0)return 2;puts(b);return 0;}\n')
    subprocess.run(['clang','-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-I',str(root/'src'),str(c),'-o',str(exe)],check=True)
    raw=subprocess.check_output([str(exe)]).strip();tools=json.loads(raw)
assert len(tools)==35 and len({t['name'] for t in tools})==35
catalog={t['name']:t for t in tools}
for t in tools:
    assert t['type']=='function' and t['parameters']['type']=='object'
    schema=t['parameters'];assert schema.get('additionalProperties') is False
    assert all(k in schema['properties'] for k in schema.get('required',[]))
    for k,p in schema['properties'].items():
        assert p['type'] in ('string','integer','boolean')
        if 'minimum' in p and 'maximum' in p:assert p['minimum']<=p['maximum']
        if 'pattern' in p:re.compile(p['pattern'])
assert not {'shell','reboot','factory_reset','ota','miio_raw','mijia_raw','infrared_raw'} & catalog.keys()
cap_source=(root/'src/native_capabilities.h').read_text();validation=cap_source[cap_source.index('static int cap_tool_arguments_valid'):cap_source.index('static void *cap_execute')]
known=set(re.findall(r'const char \*keys\[\]=\{([^}]+)\}',validation)[0].replace('"','').split(','))
for t in tools:assert set(t['parameters']['properties'])<=known,t['name']
# Minimal validator exercises the published schemas, independently of native dispatch.
def valid(schema,value):
    kind=schema.get('type')
    if isinstance(value,dict) and any(k not in value for k in schema.get('required',[])):return False
    if kind=='object':
        if not isinstance(value,dict):return False
        if any(k not in value for k in schema.get('required',[])):return False
        if schema.get('additionalProperties') is False and set(value)-set(schema.get('properties',{})):return False
        if any(not valid(schema['properties'][k],v) for k,v in value.items() if k in schema.get('properties',{})):return False
    if kind=='integer' and (type(value) is not int or value<schema.get('minimum',value) or value>schema.get('maximum',value)):return False
    if kind=='boolean' and type(value) is not bool:return False
    if kind=='string':
        if not isinstance(value,str) or len(value)<schema.get('minLength',0) or len(value)>schema.get('maxLength',len(value)):return False
        if 'pattern' in schema and not re.search(schema['pattern'],value):return False
    if 'enum' in schema and value not in schema['enum']:return False
    if 'anyOf' in schema and not any(valid(s,value) for s in schema['anyOf']):return False
    if 'not' in schema and valid(schema['not'],value):return False
    if 'oneOf' in schema and sum(valid(s,value) for s in schema['oneOf'])!=1:return False
    return True
alarm=catalog['alarm_create']['parameters']
assert valid(alarm,{'after_minutes':10}) and valid(alarm,{'local_date':'2026-11-04','local_time':'07:00'})
assert not valid(alarm,{}) and not valid(alarm,{'after_minutes':10,'timestamp':1793746800}) and not valid(alarm,{'local_date':'2026-11-04'})
assert not valid(catalog['speaker_volume_set']['parameters'],{'value':True})
assert not valid(catalog['speaker_volume_set']['parameters'],{'value':101})
assert not valid(catalog['music_seek']['parameters'],{'position_ms':-1})
assert not valid(catalog['music_chart_tracks']['parameters'],{'id':'123','limit':21})
assert valid(catalog['music_search']['parameters'],{'query':'你好世界'*10})
session={'type':'session.create','session':{'tools':tools,'instructions':'你是豆包，用用户的语言简短回答。需要操作音箱时调用对应工具；工具未成功，不要声称成功。','audio':{'input':{'format':{'type':'pcm','rate':16000}},'output':{'format':{'type':'pcm_s16le','rate':24000},'voice':'zh_female_vv_uranus_bigtts'}}}}
wire=json.dumps(session,ensure_ascii=False,separators=(',',':')).encode()
assert len(raw)+128<32768 and len(wire)<65536
out=root/'temp/analysis/speaker-capabilities';out.mkdir(parents=True,exist_ok=True)
(out/'catalog.json').write_bytes(raw+b'\n')
print(f'tools_catalog=pass count={len(tools)} catalog_bytes={len(raw)} session_create_bytes={len(wire)} local_response_limit=32768 websocket_limit=65536 network_connections=0')
