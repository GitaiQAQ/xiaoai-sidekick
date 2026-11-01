from pathlib import Path
import struct,json,math
R=Path(__file__).resolve().parents[1]/'temp/analysis/models'

def decode(blob):
    pos=0
    def take(fmt):
        nonlocal pos
        n=struct.calcsize(fmt)
        if pos+n>len(blob):raise ValueError('truncated fst')
        value=struct.unpack_from(fmt,blob,pos);pos+=n
        return value[0] if len(value)==1 else value
    def string():
        nonlocal pos
        n=take('<i')
        if not 0<=n<=128 or pos+n>len(blob):raise ValueError('fst string bounds')
        text=blob[pos:pos+n].decode('ascii');pos+=n;return text
    if take('<i')!=2125659606:raise ValueError('FST magic')
    fst,arc=string(),string()
    version,flags,properties,start,states,header_arcs=take('<iiQqqq')
    if fst!='vector' or arc!='standard' or version!=2 or flags or not 0<states<1000:raise ValueError('unsupported fst header')
    nodes=[]
    for i in range(states):
        final,n=take('<fq')
        if not 0<=n<1000:raise ValueError('arc count')
        edges=[]
        for _ in range(n):
            il,ol,w,dst=take('<iifi')
            if not 0<=dst<states:raise ValueError('destination state')
            edges.append(dict(input=il,output=ol,weight=w,next=dst))
        nodes.append(dict(state=i,final=None if math.isinf(final) else final,arcs=edges))
    if pos!=len(blob):raise ValueError('fst end mismatch')
    return dict(type=fst,arc_type=arc,version=version,properties=hex(properties),start=start,
                states=states,header_arcs=header_arcs,actual_arcs=sum(len(n['arcs']) for n in nodes),graph=nodes)

for folder in R.glob('mdata_*'):
    result=decode((folder/'root.list.2.fst.bin').read_bytes())
    (folder/'fst-decoded.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    lines=['digraph wake {','  rankdir=LR;']
    for n in result['graph']:
        lines.append(f'  s{n["state"]} [shape={"doublecircle" if n["final"] is not None else "circle"}];')
        for a in n['arcs']:
            lines.append(f'  s{n["state"]} -> s{a["next"]} [label="{a["input"]}:{a["output"]}/{a["weight"]:g}"];')
    lines.append('}')
    (folder/'wake-graph.dot').write_text('\n'.join(lines),encoding='utf-8')
    print(folder.name,'states',result['states'],'arcs',result['actual_arcs'])
    for n in result['graph']:
        for a in n['arcs']:
            if a['output']:print(' output',n['state'],'->',a['next'],'label',a['output'],'input',a['input'])
