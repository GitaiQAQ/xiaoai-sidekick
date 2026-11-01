"""Decode observed LX06 model containers; never modifies firmware.

Container/scalar tags match BINN. The firmware uses fixed 4-byte big-endian
blob lengths, unlike the current upstream format. Bounds/count/end validation
is mandatory; the result is a reverse-engineered dialect, not a generic BINN parser.
"""
from pathlib import Path
from elftools.elf.elffile import ELFFile
import hashlib,json,re,struct

R=Path(__file__).resolve().parents[1];O=R/'temp/analysis/models'

class Model:
    def __init__(self,blob,out):self.blob=blob;self.out=out;self.nodes=0;self.blobs=[]
    def read(self,p,n):
        if p<0 or n<0 or p+n>len(self.blob):raise ValueError(f'out of bounds {p:#x}+{n}')
        return self.blob[p:p+n]
    def integer(self,p):
        first=self.read(p,1)[0]
        if first&0x80:return int.from_bytes(self.read(p,4),'big')&0x7fffffff,p+4
        return first,p+1
    def value(self,p,path='root',depth=0):
        self.nodes+=1
        if depth>30 or self.nodes>50000:raise ValueError('structural bound')
        start=p;t=self.read(p,1)[0];p+=1
        if t in (0xe0,0xe2):
            size,p=self.integer(p);count,p=self.integer(p)
            if size<3 or start+size>len(self.blob) or count>50000:raise ValueError('container bounds')
            result=[] if t==0xe0 else {}
            for i in range(count):
                key=str(i)
                if t==0xe2:
                    n=self.read(p,1)[0];p+=1;key=self.read(p,n).decode('utf-8');p+=n
                    if key in result:raise ValueError('duplicate object key')
                child,p=self.value(p,path+'.'+key,depth+1)
                if t==0xe0:result.append(child)
                else:result[key]=child
            if p!=start+size:raise ValueError(f'container end mismatch {path}: {p:#x} != {start+size:#x}')
            return result,p
        if t in (0,1,2):return {0:None,1:True,2:False}[t],p
        formats={0x20:'>B',0x21:'>b',0x40:'>H',0x41:'>h',0x60:'>I',0x61:'>i',0x62:'>f',0x80:'>Q',0x81:'>q',0x82:'>d'}
        if t in formats:
            n=struct.calcsize(formats[t]);return struct.unpack(formats[t],self.read(p,n))[0],p+n
        if t==0xa0:
            n,p=self.integer(p);value=self.read(p,n).decode('utf-8');p+=n
            if self.read(p,1)!=b'\0':raise ValueError('string terminator')
            return value,p+1
        if t==0xc0:
            n=int.from_bytes(self.read(p,4),'big');p+=4;b=self.read(p,n)
            filename=re.sub(r'[^A-Za-z0-9_.-]','_',path)+'.bin'
            self.out.mkdir(parents=True,exist_ok=True);(self.out/filename).write_bytes(b)
            value={'blob_size':n,'offset':hex(p),'sha256':hashlib.sha256(b).hexdigest(),'file':filename}
            self.blobs.append(value)
            if b[:1] in (b'\xe0',b'\xe2'):
                nested,end=self.value(p,path+'.contents',depth+1)
                if end!=p+n:raise ValueError('nested container blob end')
                value['decoded_container']=nested
            else:
                value['structured_tokens']=[x.decode('ascii') for x in re.findall(rb'<[A-Za-z][A-Za-z0-9_/-]{2,}>',b)][:80]
                value['header_hex']=b[:64].hex()
            return value,p+n
        raise ValueError(f'unknown type {t:#x} at {start:#x} path={path}')

summary=[]
with (R/'temp/rootfs-factory-system0/usr/lib/libxaudio_engine.so').open('rb') as f:
    e=ELFFile(f)
    for s in e.get_section_by_name('.dynsym').iter_symbols():
        if s.name not in ('mdata_wakeup_mobile','mdata_wakeup_mobile_clone'):continue
        sec=e.get_section(s['st_shndx']);a=s['st_value']-sec['sh_addr'];blob=sec.data()[a:a+s['st_size']]
        m=Model(blob,O/s.name);value,end=m.value(0)
        if end!=len(blob):raise ValueError('top-level trailing data')
        report={'symbol':s.name,'elf_va':hex(s['st_value']),'total_bytes':len(blob),'nodes':m.nodes,'model':value}
        (O/s.name/'decoded.json').write_text(json.dumps(report,indent=2,ensure_ascii=False),encoding='utf-8')
        summary.append({'symbol':s.name,'bytes':len(blob),'validated_container_nodes':m.nodes,'blobs':m.blobs})
        print(s.name,'validated full container',len(blob),'nodes',m.nodes)
        for b in m.blobs:print(' ',b['file'],b['blob_size'])
(O/'manifest.json').write_text(json.dumps(summary,indent=2),encoding='utf-8')
