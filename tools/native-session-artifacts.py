from pathlib import Path
import json
R=Path(__file__).resolve().parents[1];O=R/'temp/analysis'
messages=json.loads((O/'native-speech-protobuf-descriptors.json').read_text())
names={m['address']:m['name'].replace('.','_') for m in messages}
lines=['// Reconstructed from LX06 1.94.13 native protobuf-c descriptors.','// This is an analysis artifact, not Xiaomi source code.','syntax = "proto2";','package lx06_native_reconstructed;','']
for m in sorted(messages,key=lambda m:m['name']):
 lines.append('message '+m['name'].replace('.','_')+' {')
 for field in m['fields']:
  label={0:'required',1:'optional',2:'repeated'}[field['label']]
  typename=names.get(field['descriptor_address'],field['type']) if field['type']=='message' else field['type']
  lines.append(f"  {label} {typename} {field['name']} = {field['id']};")
 lines.append('}\n')
(O/'native-speech-reconstructed.proto').write_text('\n'.join(lines),encoding='utf-8')
up=[(0,'register_request',0x149c8),(1,'stream_prepare_request',0x14c58),(2,'stream_cancel_request',0x14ec4),(3,'stream_transmitting',0x1515c),(4,'wakeup_latency',0x1548c),(5,'voip',0x15714),(6,'tts_finish',0x1597c),(7,'multi_channel_upload_request',0x15bec)]
(O/'upward-types.json').write_text(json.dumps([{'type':i,'meaning':n,'pack_callsite':hex(a),'evidence':'static builder stores + protobuf descriptor offsets'} for i,n,a in up],indent=2),encoding='utf-8')
regions=[(0x13bc4,0x13c04),(0x18e7c,0x18f78),(0x19528,0x19704),(0x19740,0x19760),(0x19940,0x199a4),(0x288e4,0x28a68)]
source=(O/'usr_bin_mipns-xiaomi-annotated.txt').read_text().splitlines();out=[]
for lo,hi in regions:
 out.append(f'\nADDRESS RANGE {lo:08x}-{hi:08x}; bounds selected for evidence, not automatic function recovery')
 for line in source:
  try:a=int(line[:8],16)
  except ValueError:continue
  if lo<=a<hi:out.append(line)
(O/'callback-abi-excerpts.txt').write_text('\n'.join(out),encoding='utf-8')
print('Reconstructed proto, 8 upstream types, callback excerpts saved.')
