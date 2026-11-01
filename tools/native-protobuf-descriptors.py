from pathlib import Path
from elftools.elf.elffile import ELFFile
import struct,json,re
R=Path(__file__).resolve().parents[1];B=R/'temp/rootfs-factory-system0';O=R/'temp/analysis'
f=(B/'usr/lib/libaivs-message-util.so').open('rb');e=ELFFile(f);dyn=e.get_section_by_name('.dynsym');syms=list(dyn.iter_symbols())
relocs={}
for sec in e.iter_sections():
 if sec['sh_type']=='SHT_REL':
  table=e.get_section(sec['sh_link'])
  for r in sec.iter_relocations():relocs[r['r_offset']]=(r['r_info_type'],table.get_symbol(r['r_info_sym']))
def data(addr,n):
 for sec in e.iter_sections():
  if sec['sh_type']!='SHT_NOBITS' and sec['sh_addr']<=addr<sec['sh_addr']+sec['sh_size']:return sec.data()[addr-sec['sh_addr']:addr-sec['sh_addr']+n]
 return b''
def u(addr):return struct.unpack('<I',data(addr,4))[0]
def ptr(addr):
 val=u(addr)
 if addr in relocs:
  typ,s=relocs[addr]
  if typ==2:val+=s['st_value']
  elif typ in (21,22):val=s['st_value']
 return val
def text(addr):
 if not addr:return ''
 return data(addr,512).split(b'\0')[0].decode('utf-8',errors='replace')
types=['int32','sint32','sfixed32','int64','sint64','sfixed64','uint32','fixed32','uint64','fixed64','float','double','bool','enum','string','bytes','message']
result=[]
for s in syms:
 if not s.name.startswith('speech_message') or not s.name.endswith('__descriptor') or s['st_shndx']=='SHN_UNDEF':continue
 addr=s['st_value']
 if u(addr)!=0x28aaeef9:continue
 n=u(addr+24);fieldbase=ptr(addr+28)
 if n>200:raise ValueError('Invalid field count')
 fields=[]
 for i in range(n):
  a=fieldbase+i*48;typeid=u(a+12)
  fields.append(dict(name=text(ptr(a)),id=u(a+4),label=u(a+8),type=types[typeid] if typeid<len(types) else typeid,offset=u(a+20),descriptor_address=hex(ptr(a+24)),flags=u(a+32)))
 result.append(dict(symbol=s.name,address=hex(addr),name=text(ptr(addr+4)),size=u(addr+20),fields=fields))
(O/'native-speech-protobuf-descriptors.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
for msg in result:
 if re.search('wakeup|upward_message__descriptor|downward_message__descriptor|stream_transmit|stream_prepare|event_notify',msg['symbol']):
  print(msg['symbol'],msg['address'],'size',msg['size'])
  for fld in msg['fields']:print(' ',fld['id'],fld['type'],fld['name'],'offset',fld['offset'])
print('Total message descriptors:',len(result))
