"""Write LX06 wake model containers (inverse of native-model-container.py).

Parts: conf (BINN object), model (Kaldi binary TransitionModel), fst (OpenFST
vector/standard), am (soil, see native-model-am.py), prior (BINN object).
Self-test (no args): parse every part of both factory models into plain values,
re-emit them and the top container, and require byte equality with the ELF symbol.
"""
from pathlib import Path
import math,struct

R=Path(__file__).resolve().parents[1];O=R/'temp/analysis/models'

# ---- BINN dialect: tags as observed, 4-byte BE blob length, smallest integer tag
class Blob(bytes):pass

def integer(n):return bytes([n]) if n<0x80 else struct.pack('>I',n|0x80000000)

def dump(v):
    if v is None:return b'\0'
    if v is True:return b'\1'
    if v is False:return b'\2'
    if isinstance(v,Blob):return b'\xc0'+struct.pack('>I',len(v))+v
    if isinstance(v,str):e=v.encode();return b'\xa0'+integer(len(e))+e+b'\0'
    if isinstance(v,float):return b'\x62'+struct.pack('>f',v)
    if isinstance(v,int):
        for tag,fmt,lo,hi in((0x20,'>B',0,255),(0x40,'>H',0,65535),(0x60,'>I',0,2**32-1),(0x80,'>Q',0,2**64-1)) if v>=0 else \
                             ((0x21,'>b',-128,127),(0x41,'>h',-32768,32767),(0x61,'>i',-2**31,2**31-1),(0x81,'>q',-2**63,2**63-1)):
            if lo<=v<=hi:return bytes([tag])+struct.pack(fmt,v)
    if isinstance(v,(dict,list)):
        body=b''.join((bytes([len(k.encode())])+k.encode()+dump(x)) for k,x in v.items()) if isinstance(v,dict) else b''.join(map(dump,v))
        tag=b'\xe2' if isinstance(v,dict) else b'\xe0';cnt=integer(len(v))
        size=1+1+len(cnt)+len(body)
        if size>=0x80:size+=3
        return tag+integer(size)+cnt+body
    raise TypeError(type(v))

def load(b,p=0):
    t=b[p];p+=1
    def num(p):return (int.from_bytes(b[p:p+4],'big')&0x7fffffff,p+4) if b[p]&0x80 else (b[p],p+1)
    if t in(0xe0,0xe2):
        start=p-1;size,p=num(p);count,p=num(p);r={} if t==0xe2 else []
        for _ in range(count):
            if t==0xe2:n=b[p];k=b[p+1:p+1+n].decode();p+=1+n
            v,p=load(b,p)
            if t==0xe2:r[k]=v
            else:r.append(v)
        if p!=start+size:raise ValueError('container size')
        return r,p
    if t in(0,1,2):return (None,True,False)[t],p
    f={0x20:'>B',0x21:'>b',0x40:'>H',0x41:'>h',0x60:'>I',0x61:'>i',0x62:'>f',0x80:'>Q',0x81:'>q',0x82:'>d'}.get(t)
    if f:n=struct.calcsize(f);return struct.unpack_from(f,b,p)[0],p+n
    if t==0xa0:n,p=num(p);return b[p:p+n].decode(),p+n+1
    if t==0xc0:n=int.from_bytes(b[p:p+4],'big');return Blob(b[p+4:p+4+n]),p+4+n
    raise ValueError(f'tag {t:#x}')

def container(conf,model,fst,am,prior):
    return dump({'list':[{'conf':Blob(conf)},{'model':Blob(model)},{'fst':Blob(fst)},{'am':Blob(am)},{'prior':Blob(prior)}]})

def prior_blob(log_priors,left,right):return dump({'log_priors':[float(x) for x in log_priors],'left_context':left,'right_context':right})

# ---- Kaldi binary TransitionModel (one Topology, per-phone 1-emitting-state HMMs as in factory)
def transition_model(t):
    """t: phones, phone2entry (index by phone, [0]=-1), entries [[(pdf_class,[(dst,prob)])...]], triples [(phone,hmm_state,pdf)], logprobs"""
    I=lambda v:b'\4'+struct.pack('<i',v);F=lambda v:b'\4'+struct.pack('<f',v)
    V=lambda v:b'\4'+struct.pack(f'<i{len(v)}i',len(v),*v)
    out=[b'\0B<TransitionModel> <Topology> ',V(t['phones']),V(t['phone2entry']),I(len(t['entries']))]
    for e in t['entries']:
        out.append(I(len(e)))
        for pdf,trans in e:out+=[I(pdf),I(len(trans))]+[I(d)+F(p) for d,p in trans]
    out+=[b'</Topology> <Triples> ',I(len(t['triples']))]+[I(a)+I(b)+I(c) for a,b,c in t['triples']]
    lp=t['logprobs'];out+=[b'</Triples> <LogProbs> FV ',b'\4'+struct.pack(f'<i{len(lp)}f',len(lp),*lp),b'</LogProbs> </TransitionModel> ']
    return b''.join(out)

def parse_transition_model(b):
    p=0
    def lit(s):
        nonlocal p
        if b[p:p+len(s)]!=s:raise ValueError(f'expected {s!r} at {p}')
        p+=len(s)
    def one(f):
        nonlocal p
        if b[p]!=4:raise ValueError(f'size byte at {p}')
        v=struct.unpack_from(f,b,p+1)[0];p+=5;return v
    def vec():
        nonlocal p
        n=one('<i');v=list(struct.unpack_from(f'<{n}i',b,p));p+=4*n;return v
    lit(b'\0B<TransitionModel> <Topology> ');t={'phones':vec(),'phone2entry':vec(),'entries':[]}
    for _ in range(one('<i')):
        t['entries'].append([(one('<i'),[(one('<i'),one('<f')) for _ in range(one('<i'))]) for _ in range(one('<i'))])
    lit(b'</Topology> <Triples> ');t['triples']=[(one('<i'),one('<i'),one('<i')) for _ in range(one('<i'))]
    lit(b'</Triples> <LogProbs> FV ');n=one('<i');t['logprobs']=list(struct.unpack_from(f'<{n}f',b,p));p+=4*n
    lit(b'</LogProbs> </TransitionModel> ')
    if p!=len(b):raise ValueError('transition model trailing bytes')
    return t

# ---- OpenFST vector/standard
def fst_properties(graph):
    """Static property bits the factory writer set: expanded|mutable, label/weight/epsilon facts, not-top-sorted."""
    arcs=[a for n in graph for a in n['arcs']]
    p=0x3|0x8000000000
    p|=0x10000 if all(a['input']==a['output'] for a in arcs) else 0x20000
    if any(a['input']==0 and a['output']==0 for a in arcs):p|=0x400000
    if any(a['input']==0 for a in arcs):p|=0x1000000
    if any(a['output']==0 for a in arcs):p|=0x4000000
    srt=lambda k:all(all(x[k]<=y[k] for x,y in zip(n['arcs'],n['arcs'][1:])) for n in graph)
    p|=0x10000000 if srt('input') else 0x20000000
    p|=0x40000000 if srt('output') else 0x80000000
    if any(a['weight'] not in(0.0,) for a in arcs) or any(n['final'] not in(None,0.0) for n in graph):p|=0x100000000
    return p

def fst_blob(graph,start=0,properties=None):
    s=lambda x:struct.pack('<i',len(x))+x.encode()
    out=[struct.pack('<i',2125659606),s('vector'),s('standard'),
         struct.pack('<iiQqqq',2,0,fst_properties(graph) if properties is None else properties,start,len(graph),0)]
    for n in graph:
        out.append(struct.pack('<fq',math.inf if n['final'] is None else n['final'],len(n['arcs'])))
        out+=[struct.pack('<iifi',a['input'],a['output'],a['weight'],a['next']) for a in n['arcs']]
    return b''.join(out)

def selftest():
    import json
    from elftools.elf.elffile import ELFFile
    with (R/'temp/rootfs-factory-system0/usr/lib/libxaudio_engine.so').open('rb') as f:
        e=ELFFile(f);syms={}
        for s in e.get_section_by_name('.dynsym').iter_symbols():
            if s.name in('mdata_wakeup_mobile','mdata_wakeup_mobile_clone'):
                sec=e.get_section(s['st_shndx']);a=s['st_value']-sec['sh_addr'];syms[s.name]=sec.data()[a:a+s['st_size']]
    for name,blob in sorted(syms.items()):
        top,end=load(blob)
        if end!=len(blob) or dump(top)!=blob:raise SystemExit(f'{name}: top container does not round-trip')
        parts={k:v for d in top['list'] for k,v in d.items()}
        conf,_=load(parts['conf']);prior,_=load(parts['prior']);tm=parse_transition_model(parts['model'])
        fst=json.loads((O/name/'fst-decoded.json').read_text())
        g=[dict(final=n['final'],arcs=n['arcs']) for n in fst['graph']]
        checks={'conf':dump(conf)==parts['conf'],'prior':prior_blob(prior['log_priors'],prior['left_context'],prior['right_context'])==parts['prior'],
                'model':transition_model(tm)==parts['model'],'fst':fst_blob(g,fst['start'])==parts['fst'],
                'am':(O/name/'root.list.3.am.bin').read_bytes()==parts['am'],
                'container':container(dump(conf),transition_model(tm),fst_blob(g),parts['am'],prior_blob(prior['log_priors'],40,9))==blob}
        print(name,len(blob),checks)
        if not all(checks.values()):raise SystemExit('round trip failed')
    print('all parts re-emitted byte-identical')

if __name__=='__main__':selftest()
