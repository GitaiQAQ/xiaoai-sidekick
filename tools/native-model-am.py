"""Parse the soil binary network inside the extracted wake models.

Reads temp/analysis/models/<symbol>/root.list.3.am.bin written by
native-model-container.py. Layout follows libxaudio_engine.so (factory 1.94.13):
SoilLoadModel 0x6da40 reads a component count, then 0x6f028 reads per component
  type(int32), type-specific fields, n_inputs(int32), input ids(int32 * n)
Type codes come from the name table at 0x6e3c4. Field order per type comes from
the binary branches of the loaders listed below. Every word must be consumed.
"""
from pathlib import Path
import json,struct

R=Path(__file__).resolve().parents[1];O=R/'temp/analysis/models'
TYPES={1:'input',2:'gru',3:'lstm',4:'convolution',5:'max_pooling',6:'splice',7:'padding',8:'affine',9:'relu',10:'tanh',11:'add',
 12:'reducesum',13:'unfixedscale',14:'sigmoid',15:'pnorm',16:'fixedscale',17:'normalize',18:'stats_pooling',19:'softmax',
 20:'embedding',21:'reshape',22:'alphaconvolution',23:'disabled'}

def parse(b):
    if len(b)%4:raise ValueError('not word aligned')
    W=len(b)//4;iw=struct.unpack(f'<{W}i',b);fw=struct.unpack(f'<{W}f',b);p=0
    def ints(n):
        nonlocal p
        if n<0 or p+n>W:raise ValueError(f'out of bounds at word {p}+{n}')
        v=list(iw[p:p+n]);p+=n;return v
    def floats(n):
        nonlocal p
        if n<0 or p+n>W:raise ValueError(f'out of bounds at word {p}+{n}')
        s=p;p+=n;return s
    count=ints(1)[0];comps=[]
    for idx in range(count):
        kind=ints(1)[0];c={'index':idx,'type':kind,'name':TYPES.get(kind,'unknown')}
        if kind==1:pass                                                   # no fields, no inputs
        elif kind==7:c['left_context'],c['right_context'],c['padding_type'],c['zero']=ints(4)          # 0x85dac
        elif kind==6:                                                      # 0x832f0
            c['in'],c['out'],c['f58'],c['f4c'],c['f48'],c['f54'],c['f64'],has=ints(8)
            if has:
                n,c['f44']=ints(2);c['offsets']=ints(n)
                if c['out']!=c['in']*n:raise ValueError(f'splice {idx} dims')
        elif kind==8:                                                      # 0x8c494, W is out x in row-major
            c['in'],c['out']=ints(2);c['weight_word']=floats(c['in']*c['out']);c['bias_word']=floats(c['out'])
        elif kind==9:pass                                                  # relu, no fields
        elif kind==17:                                                     # 0x86030
            c['size'],c['need_divide'],c['need_minus'],c['need_reverse']=ints(4);c['target_rms']=fw[floats(1)];c['vector_source']=ints(1)[0]
            if c['vector_source']==1:raise ValueError(f'normalize {idx}: vector_source=1 branch not decoded')
        elif kind==19:c['dolog'],c['logsoftmax']=ints(2)                   # 0x83d38
        else:raise ValueError(f'type {kind} ({TYPES.get(kind)}) at component {idx} not decoded')
        if kind!=1:
            n=ints(1)[0];c['inputs']=ints(n)
            if any(i>=idx for i in c['inputs']):raise ValueError(f'component {idx} forward reference')
        comps.append(c)
    if p!=W:raise ValueError(f'{W-p} trailing words')
    return {'components':comps,'words':W}

def emit(net,weights):
    """Inverse of parse. weights maps component index -> (W out x in, bias) float32 arrays-like."""
    out=[struct.pack('<i',len(net['components']))]
    I=lambda *v:out.append(struct.pack(f'<{len(v)}i',*v));F=lambda v:out.append(struct.pack(f'<{len(v)}f',*v))
    for c in net['components']:
        t=c['type'];I(t)
        if t==7:I(c['left_context'],c['right_context'],c['padding_type'],c['zero'])
        elif t==6:
            I(c['in'],c['out'],c['f58'],c['f4c'],c['f48'],c['f54'],c['f64'],1 if 'offsets' in c else 0)
            if 'offsets' in c:I(len(c['offsets']),c['f44'],*c['offsets'])
        elif t==8:
            W,b=weights[c['index']];I(c['in'],c['out']);F([float(x) for row in W for x in row]);F([float(x) for x in b])
        elif t==17:I(c['size'],c['need_divide'],c['need_minus'],c['need_reverse']);F([c['target_rms']]);I(c['vector_source'])
        elif t==19:I(c['dolog'],c['logsoftmax'])
        if t!=1:I(len(c['inputs']),*c['inputs'])
    return b''.join(out)

if __name__=='__main__':
    for model in ('mdata_wakeup_mobile','mdata_wakeup_mobile_clone'):
        blob=(O/model/'root.list.3.am.bin').read_bytes();net=parse(blob);fw=struct.unpack(f'<{len(blob)//4}f',blob)
        ws={c['index']:([fw[c['weight_word']+r*c['in']:c['weight_word']+(r+1)*c['in']] for r in range(c['out'])],fw[c['bias_word']:c['bias_word']+c['out']])
            for c in net['components'] if c['type']==8}
        if emit(net,ws)!=blob:raise ValueError(f'{model}: emit does not reproduce original bytes')
        cs=net['components'];splices=[c['offsets'] for c in cs if c['type']==6]
        pad=[c for c in cs if c['type']==7]
        net['splice_left'],net['splice_right']=-sum(min(o) for o in splices),sum(max(o) for o in splices)
        (O/model/'am-decoded.json').write_text(json.dumps(net,indent=1),encoding='utf-8')
        chain=' > '.join(c['name']+(f"[{c['in']}>{c['out']}]" if c['type'] in(6,8) else '') for c in cs)
        params=sum(c['in']*c['out']+c['out'] for c in cs if c['type']==8)
        print(model,'components',len(cs),'words',net['words'],'affine params',params)
        print(' padding',[(c['left_context'],c['right_context'],c['padding_type'],c['zero']) for c in pad],'splice context',net['splice_left'],net['splice_right'])
        print(' normalize',sorted({(c['need_divide'],c['need_minus'],c['need_reverse'],c['target_rms'],c['vector_source']) for c in cs if c['type']==17}))
        print(' softmax',[(c['dolog'],c['logsoftmax']) for c in cs if c['type']==19],'non-chain inputs',[(c['index'],c['inputs']) for c in cs if c.get('inputs') not in(None,[c['index']-1])])
        print(' ',chain)
