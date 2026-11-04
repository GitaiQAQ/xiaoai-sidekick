"""Sequence-level (differentiable Viterbi) fine-tune of the LX06 "豆包豆包" wake model.

Local Mac experiment only. No SSH, device access, recording, cloud upload, or deploy.

Stages
  unit-test  compare torch differentiable Viterbi with tools/native-model-forward.py
  stage0     L1 full-data sequence fine-tune from d96-v5, r34 controls; feasibility gate
  stage1     5-fold CV of L1 (time-ordered 35 s3 groups, 7 per fold)
  stage2     L2 full-data sequence fine-tune from l2-d160 + L1/L2 cascade
  cv         L1+L2 5-fold CV and cascade (stage1 and stage2 CV numbers)
  final      package full-data L1/L2 BINN (AM only replaced, r34 conf/model/fst/prior)
  all        unit-test -> stage0 (gate) -> stage1/stage2/cv/final if gate passes
  round2     online-augmentation L1 run with --neg-agg/--freeze/--long-neg; output in round2/
  round2-sweep  run the built-in <=8 variant sweep and write round2/REPORT.md
  round2-report regenerate round2/REPORT.md from round2/summary.json

Example:
  ~/Library/Caches/xiaoai-sidekick/train-venv/bin/python tools/wake-seq-train.py all
"""
from pathlib import Path
import argparse, copy, csv, hashlib, importlib.util, json, math, struct, time, zlib
import numpy as np
import torch
import torch.nn as nn

R = Path(__file__).resolve().parents[1]
C = R / 'private/inputs/recordings/wake-corpus'
TTS = R / 'temp/analysis/tts-corpus'
OUT = R / 'temp/analysis/wake-seq'
ROUND2 = OUT / 'round2'
MODELS = R / 'temp/analysis/models'
BASE = {1: R / 'temp/analysis/wake-train/d96-v5',
        2: R / 'temp/analysis/wake-train/l2-d160'}
R34 = R / 'temp/analysis/wake-group-labels/review-controls-r34'
S3_LEVELS = R / 'temp/analysis/mfcc-audit/s3-35-levels.json'
S3_MATRIX = R / 'temp/analysis/mfcc-audit/s3-35-matrix.json'

KW = [1, 2, 4, 5, 6, 7, 8, 9]
SIL, GARB = 0, 3
BIG = 1e6


def load_module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


M = load_module('wt', R / 'tools/wake-train.py')
F = M.F
P = M.P


def set_level(level):
    global M
    M.M = MODELS / ('mdata_wakeup_mobile' if level == 1 else 'mdata_wakeup_mobile_clone')
    M.KW[:] = KW


def mfcc16(pcm):
    """S16 input contract: clip -> rint -> int16 before the MFCC front end."""
    return F.mfcc(np.rint(np.clip(pcm, -32768, 32767)).astype(np.int16))


def forward64(feats, net, blob):
    """Double-precision copy of native-model-forward.forward for the export parity check."""
    fw = np.frombuffer(blob, '<f4').astype(np.float64)
    x = feats.astype(np.float64)
    for c in net['components'][1:]:
        t = c['type']
        if t == 7:
            x = np.concatenate([np.repeat(x[:1], c['left_context'], 0), x,
                                np.repeat(x[-1:], c['right_context'], 0)])
        elif t == 6:
            o = c['offsets']
            lo, hi = -min(o), max(o)
            T = len(x) - lo - hi
            x = np.concatenate([x[lo + k:lo + k + T] for k in o], 1)
        elif t == 8:
            W = fw[c['weight_word']:c['weight_word'] + c['in'] * c['out']].reshape(c['out'], c['in'])
            b = fw[c['bias_word']:c['bias_word'] + c['out']]
            x = x @ W.T + b
        elif t == 9:
            x = np.maximum(x, 0)
        elif t == 17:
            x = x / np.sqrt(np.maximum((x * x).sum(1, keepdims=True) * c['target_rms'] / c['size'], 1.3552527e-20))
        elif t == 19:
            x = x - x.max(1, keepdims=True)
            x = x - np.log(np.exp(x).sum(1, keepdims=True))
        else:
            raise ValueError(c['name'])
    return x


def export_error(net, parsed, am, probe):
    """Return (double-precision parity error, float32 roundoff) for the exported AM."""
    with torch.no_grad():
        net64 = copy.deepcopy(net).double()
        t64 = net64(torch.from_numpy(M.padded(probe).T[None].astype(np.float64)))[0].T.numpy()
    e64 = float(np.abs(t64 - forward64(probe, parsed, am)).max())
    e32 = float(np.abs(M.posteriors(net, probe) - F.forward(probe, parsed, am)).max())
    return e64, e32


def wav(path):
    return M.wav(path)


# ---------------- r34 controls (conf/model/fst/prior from the reviewed containers)
def decode_fst(blob):
    pos = 0

    def take(fmt):
        nonlocal pos
        v = struct.unpack_from(fmt, blob, pos)
        pos += struct.calcsize(fmt)
        return v[0] if len(v) == 1 else v

    def string():
        nonlocal pos
        n = take('<i')
        v = blob[pos:pos + n].decode()
        pos += n
        return v

    if take('<i') != 2125659606:
        raise ValueError('FST magic')
    fst, arc = string(), string()
    version, flags, props, start, states, harcs = take('<iiQqqq')
    graph = []
    for i in range(states):
        final, n = take('<fq')
        arcs = []
        for _ in range(n):
            il, ol, w, dst = take('<iifi')
            arcs.append(dict(input=il, output=ol, weight=w, next=dst))
        graph.append(dict(state=i, final=None if math.isinf(final) else final, arcs=arcs))
    if pos != len(blob):
        raise ValueError('FST trailing bytes')
    return dict(type=fst, arc_type=arc, version=version, properties=hex(props),
                start=start, states=states, header_arcs=harcs,
                actual_arcs=sum(len(n['arcs']) for n in graph), graph=graph)


def r34_controls(level):
    blob = (R34 / f'l{level}.binn').read_bytes()
    top, end = P.load(blob)
    assert end == len(blob)
    parts = {k: v for d in top['list'] for k, v in d.items()}
    fst = decode_fst(parts['fst'])
    prior = np.array(P.load(parts['prior'])[0]['log_priors'], dtype=np.float32)
    step = max(a['weight'] for n in fst['graph'] for a in n['arcs']
               if a['input'] != 0 and a['weight'] > 1.0)
    return parts, fst, prior, float(step)


# ---------------- differentiable Viterbi (same recurrence as F.viterbi)
def dviterbi(am, step):
    """am: B x T x 10 = scale*(loglik-prior). Returns B x T margins cost9-cost1."""
    B, T, K = am.shape
    cost = torch.full((B, 10), BIG, dtype=am.dtype, device=am.device)
    cost[:, 0] = 0.0
    out = []
    for t in range(T):
        a = am[:, t]
        c = cost
        n1 = torch.minimum(c[:, 0] + 1.0 - a[:, 0], c[:, 0] + 1.0 - a[:, 3])
        n0 = n1                                                       # epsilon state1 -> state0
        n2 = torch.minimum(c[:, 2] + 1.0 - a[:, KW[0]], c[:, 0] + step - a[:, KW[0]])
        n3 = torch.minimum(c[:, 3] + 1.0 - a[:, KW[1]], c[:, 2] + step - a[:, KW[1]])
        n4 = torch.minimum(c[:, 4] + 1.0 - a[:, KW[2]], c[:, 3] + step - a[:, KW[2]])
        n5 = torch.minimum(c[:, 5] + 1.0 - a[:, KW[3]], c[:, 4] + step - a[:, KW[3]])
        n6 = torch.minimum(c[:, 6] + 1.0 - a[:, KW[4]], c[:, 5] + step - a[:, KW[4]])
        n7 = torch.minimum(c[:, 7] + 1.0 - a[:, KW[5]], c[:, 6] + step - a[:, KW[5]])
        n8 = torch.minimum(c[:, 8] + 1.0 - a[:, KW[6]], c[:, 7] + step - a[:, KW[6]])
        n9 = torch.minimum(c[:, 9] + 1.0 - a[:, KW[7]], c[:, 8] + step - a[:, KW[7]])
        cost = torch.stack([n0, n1, n2, n3, n4, n5, n6, n7, n8, n9], 1)
        out.append(cost[:, 9] - cost[:, 1])
    return torch.stack(out, 1)


def dviterbi_k(am, step, k=1):
    """Torch min-duration-k decoder.  am: B x T x 10.  Returns B x T margins.

    k=1 is the r34 FST recurrence (identical to dviterbi); k>1 requires each
    keyword pdf to be visited for at least k frames, as in review/dur.py.  The
    r34 FST cannot express k>1, so k>1 models are evaluation-only.
    """
    B, T, _ = am.shape
    dtype, device = am.dtype, am.device
    c = torch.full((B, 8, k), BIG, dtype=dtype, device=device)
    F = torch.zeros(B, dtype=dtype, device=device)
    out = []
    for t in range(T):
        a = am[:, t]
        kw = a[:, KW]
        fill = F + 1.0 - torch.maximum(a[:, 0], a[:, 3])
        entry = torch.cat([F[:, None], c[:, :-1, -1]], 1)
        first = entry + step - kw
        if k == 1:
            nc = torch.minimum(c[:, :, 0] + 1.0 - kw, first)[:, :, None]
        else:
            parts = [first[:, :, None]]
            if k > 2:
                parts.append(c[:, :, :k - 2] + 1.0 - kw[:, :, None])
            last = torch.minimum(c[:, :, k - 2] + 1.0 - kw,
                                 c[:, :, k - 1] + 1.0 - kw)
            parts.append(last[:, :, None])
            nc = torch.cat(parts, 2)
        c = nc
        F = fill
        out.append(c[:, -1, -1] - F)
    return torch.stack(out, 1)


def viterbi_k_np(am, step, k=1):
    """Numpy min-duration-k decoder matching review/dur.py.  am: T x 10."""
    am = np.asarray(am, np.float64)
    T = len(am)
    c = np.full((8, k), BIG, np.float64)
    F = 0.0
    out = np.empty(T)
    for t in range(T):
        a = am[t]
        fill = F + 1.0 - max(a[0], a[3])
        kw = a[KW]
        entry = np.empty(8)
        entry[0] = F
        entry[1:] = c[:-1, -1]
        first = entry + step - kw
        if k == 1:
            nc = np.minimum(c[:, 0] + 1.0 - kw, first)[:, None]
        else:
            parts = [first[:, None]]
            if k > 2:
                parts.append(c[:, :k - 2] + 1.0 - kw[:, None])
            last = np.minimum(c[:, k - 2] + 1.0 - kw, c[:, k - 1] + 1.0 - kw)
            parts.append(last[:, None])
            nc = np.concatenate(parts, 1)
        c = nc
        F = fill
        out[t] = c[-1, -1] - F
    return out


def unit_test():
    set_level(1)
    base = BASE[1]
    net = M.Net(96)
    net.load_state_dict(torch.load(base / 'net.pt', weights_only=True, map_location='cpu'))
    net.eval()
    prior = np.array(json.loads((base / 'prior.json').read_text()), dtype=np.float32)
    fst = json.loads((base / 'fst-decoded.json').read_text())
    am_net = json.loads((base / 'am-decoded.json').read_text())
    am_blob = (base / 'root.list.3.am.bin').read_bytes()
    rng = np.random.default_rng(123)
    rows = []
    for name, sec in [('s3-music-pos', 20), ('s1-pos-quiet', 20), ('s2-music-silent', 20)]:
        pcm = wav(C / f'{name}.aec16k.wav')
        off = int(rng.integers(0, max(1, len(pcm) - sec * 16000)))
        pcm = pcm[off:off + sec * 16000]
        feat = mfcc16(pcm)
        np_ll = F.forward(feat, am_net, am_blob)
        np_m = F.viterbi(np_ll, fst, prior)
        with torch.no_grad():
            x = torch.from_numpy(M.padded(feat).T[None].astype(np.float32))
            torch_ll = net(x)[0].T.numpy()
            # Double precision here isolates the recurrence from float32 accumulation;
            # training/eval still use float32 because the exported network is float32.
            amd = 0.1 * (torch_ll.astype(np.float64) - prior.astype(np.float64))
            tm = dviterbi_k(torch.from_numpy(amd[None]), 2.25, 1)[0].numpy()
            tn = viterbi_k_np(amd, 2.25, 1)
            kdiff = {}
            for kk in (3, 6):
                tk = dviterbi_k(torch.from_numpy(amd[None]), 2.25, kk)[0].numpy()
                nk = viterbi_k_np(amd, 2.25, kk)
                kdiff[kk] = float(np.abs(tk - nk).max())
        finite = np.isfinite(np_m)
        fdiff = float(np.abs(torch_ll - np_ll).max())
        vdiff = float(np.abs(tm[finite] - np_m[finite]).max())
        ndiff = float(np.abs(tn[finite] - np_m[finite]).max())
        rows.append({'clip': name, 'offset_s': off / 16000, 'frames': len(feat),
                     'forward_maxerr': fdiff, 'viterbi_maxerr': vdiff,
                     'numpy_k1_maxerr': ndiff, 'torch_numpy_kdiff': kdiff,
                     'finite_frames': int(finite.sum())})
        assert fdiff < 1e-3 and vdiff < 1e-3 and ndiff < 1e-3, (name, fdiff, vdiff, ndiff)
        assert all(v < 1e-3 for v in kdiff.values()), (name, kdiff)
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / 'unit-test.json').write_text(json.dumps({'threshold': 1e-3, 'rows': rows}, indent=2))
    print(json.dumps(rows, indent=2))
    return rows


# ---------------- grouping / sources
def group_rows(rows, session, full_pcm):
    groups = []
    for row in sorted(rows, key=lambda r: float(r['start_s'])):
        a, b = float(row['start_s']), float(row['end_s'])
        if groups and a - groups[-1]['end_s'] < .5:
            groups[-1]['end_s'] = b
            groups[-1]['members'].append(row)
        else:
            groups.append({'session': session, 'start_s': a, 'end_s': b, 'members': [row]})
    for g in groups:
        start, end = int(g['start_s'] * 16000), int(g['end_s'] * 16000)
        pcm = np.zeros(end - start, np.float32)
        holes, last = [], start
        for row in g['members']:
            a, b = int(float(row['start_s']) * 16000), int(float(row['end_s']) * 16000)
            if a > last:
                holes.append([last / 16000, a / 16000])
            pcm[a - start:b - start] = full_pcm[a:b]
            last = b
        clip_rms = 10 * np.log10(max(np.mean((pcm.astype(np.float64) / 32768) ** 2), 1e-12))
        g.update(id=f"{session}-group@{g['start_s']:.2f}", pcm=pcm, kind='pos', sub='real',
                 real=True, split='train', text='', zero_holes=holes,
                 zero_seconds=sum(b - a for a, b in holes),
                 member_count=len(g['members']),
                 member_intervals=[[float(r['start_s']), float(r['end_s'])] for r in g['members']],
                 clip_rms_dbfs=float(clip_rms),
                 peak_s16=float(np.abs(pcm).max()))
    return groups


def build_groups():
    hl = M.human()
    out = {}
    for session in ('s1-pos-quiet', 's3-music-pos'):
        rows = [r for r in csv.DictReader(open(C / f'{session}.segments.csv', encoding='utf-8'))
                if hl.get((session, r['start_s'])) == 'pos']
        out[session] = group_rows(rows, session, wav(C / f'{session}.aec16k.wav'))
    assert len(out['s3-music-pos']) == 35, len(out['s3-music-pos'])
    assert len(out['s1-pos-quiet']) == 60, len(out['s1-pos-quiet'])
    return out['s1-pos-quiet'], out['s3-music-pos']


def base_sources():
    src, bg = M.sources()
    tts = [s for s in src if not s['real']]
    s1neg = [s for s in src if s['real'] and s['kind'] == 'neg']
    dev = [s for s in src if s['split'] == 'dev']
    return tts, s1neg, dev, bg


def active_region(pcm):
    if len(pcm) < 400:
        return None
    e = M.frame_db(pcm)
    act = np.flatnonzero(e > e.max() - 30)
    if len(act) < 4:
        return None
    return int(act[0]), int(act[-1] + 1), e


def split_point(pcm, a, b, e):
    if b - a < 30:
        return None
    lo = a + int(.30 * (b - a))
    hi = a + int(.70 * (b - a))
    if hi <= lo + 2:
        return None
    sm = np.convolve(e, np.ones(5) / 5, 'same')
    k = lo + int(np.argmin(sm[lo:hi]))
    depth = float(np.max(sm[lo:hi]) - sm[k])
    return k, depth


def syllable_bounds(pcm, a, b, e):
    """Return sample indices [s0,s1,s2,s3,s4] for the four syllables of 豆包豆包.

    `a`, `b` are frame indices from active_region; the returned cuts are samples.
    Two energy valleys are used: one between the two 豆包, then one inside each
    豆包.  When a valley is not found the corresponding interval is split evenly.
    """
    a_s, b_s = int(a * 160), int(b * 160)
    if b_s - a_s < 1600:
        return [a_s + (b_s - a_s) * q // 4 for q in range(5)]
    sp2 = split_point(pcm, a, b, e)
    if sp2 is None:
        return [a_s + (b_s - a_s) * q // 4 for q in range(5)]
    m2 = int(sp2[0] * 160)
    sp1 = split_point(pcm, a, sp2[0], e)
    sp3 = split_point(pcm, sp2[0], b, e)
    m1 = int(sp1[0] * 160) if sp1 is not None else (a_s + m2) // 2
    m3 = int(sp3[0] * 160) if sp3 is not None else (m2 + b_s) // 2
    return [a_s, max(a_s + 1, m1), max(a_s + 2, m2), max(a_s + 3, m3), b_s]


def group_syllables(g):
    ar = active_region(g['pcm'])
    if ar is None:
        return None
    cuts = syllable_bounds(g['pcm'], ar[0], ar[1], ar[2])
    segs = [g['pcm'][cuts[i]:cuts[i + 1]].astype(np.float64) for i in range(4)]
    if any(len(s) < 16 for s in segs):
        return None
    return segs


def fade(x, n=80):
    x = x.copy()
    n = min(n, len(x) // 2)
    if n > 0:
        r = np.linspace(0, 1, n)
        x[:n] *= r
        x[-n:] *= r[::-1]
    return x


def scale_rms(x, target_db):
    if len(x) < 400:
        return x
    e = M.frame_db(x)
    act = e > e.max() - 30
    if not act.any():
        return x
    lvl = 10 * np.log10(np.mean(10 ** (e[act] / 10)))
    return x * 10 ** ((target_db - lvl) / 20)


def add_music(x, rng, bg, target_db, rel_db):
    m = bg['music']
    if len(m) <= len(x):
        return x
    s = int(rng.integers(0, len(m) - len(x)))
    g = 10 ** ((target_db + rel_db) / 20)
    return x + m[s:s + len(x)] * g


def place(x, rng, pre=(.2, .8), post=(.2, .6)):
    pre_n = int(rng.uniform(*pre) * 16000)
    post_n = int(rng.uniform(*post) * 16000)
    y = np.concatenate([np.zeros(pre_n, np.float32), x, np.zeros(post_n, np.float32)])
    return y, pre_n


def pos_from_group(g, rng, bg, levels, do_speed=True, do_music=True):
    ar = active_region(g['pcm'])
    if ar is None:
        return None
    a, b, e = ar
    # active_region / split_point work in 100 fps frames; every waveform slice below
    # must be in samples.  (Earlier versions mixed the two units, which truncated
    # constructed positives to ~0.5 s and placed the positive loss window on ~1 frame.)
    a_s, b_s = int(a * 160), int(b * 160)
    lo = max(0, a_s - 4800)                                           # 0.3 s left context
    hi = min(len(g['pcm']), b_s + 8000)                               # 0.5 s right context
    x = g['pcm'][lo:hi].astype(np.float64)
    wa, wb = a_s - lo, b_s - lo
    r = 1.0
    if do_speed and rng.random() < .7:
        r = float(rng.uniform(.85, 1.25))
        x = M.speed(x, r)
        wa, wb = int(wa / r), int(wb / r)
    target = None
    if do_music and rng.random() < .8:
        target = float(rng.choice(levels))
        x = scale_rms(x, target)
        x = add_music(x, rng, bg, target, float(rng.uniform(-10, 4)))
    y, pre = place(np.clip(x, -32768, 32767).astype(np.float32), rng)
    return dict(pcm=y, win=(pre + wa, pre + wb), speed=r, target_rms_dbfs=target)


def splice_pos(g1, g2, rng, bg, levels):
    ar1, ar2 = active_region(g1['pcm']), active_region(g2['pcm'])
    if ar1 is None or ar2 is None:
        return None
    a1, b1, e1 = ar1
    a2, b2, e2 = ar2
    sp1 = split_point(g1['pcm'], a1, b1, e1)
    sp2 = split_point(g2['pcm'], a2, b2, e2)
    if sp1 is None or sp2 is None:
        return None
    s1, d1 = sp1
    s2, d2 = sp2
    a1_s, b2_s = int(a1 * 160), int(b2 * 160)
    s1_s, s2_s = int(s1 * 160), int(s2 * 160)
    lo1 = max(0, a1_s - 4800)
    hi2 = min(len(g2['pcm']), b2_s + 8000)
    first = fade(g1['pcm'][lo1:s1_s].astype(np.float64))
    second = fade(g2['pcm'][s2_s:hi2].astype(np.float64))
    gap = int(rng.uniform(0, .4) * 16000)
    x = np.concatenate([first, np.zeros(gap), second])
    wa, wb = a1_s - lo1, len(first) + gap + (b2_s - s2_s)
    r = 1.0
    if rng.random() < .7:
        r = float(rng.uniform(.85, 1.25))
        x = M.speed(x, r)
        wa, wb = int(wa / r), int(wb / r)
    target = None
    if rng.random() < .85:
        target = float(rng.choice(levels))
        x = scale_rms(x, target)
        x = add_music(x, rng, bg, target, float(rng.uniform(-10, 4)))
    y, pre = place(np.clip(x, -32768, 32767).astype(np.float32), rng)
    return dict(pcm=y, win=(pre + wa, pre + wb), speed=r, target_rms_dbfs=target,
                split_depths=[d1, d2], gap_ms=gap / 16)


def single_neg(g, rng, bg, levels):
    ar = active_region(g['pcm'])
    if ar is None:
        return None
    a, b, e = ar
    sp = split_point(g['pcm'], a, b, e)
    if sp is None:
        return None
    s, d = sp
    a_s, s_s = int(a * 160), int(s * 160)
    lo = max(0, a_s - 4800)
    x = g['pcm'][lo:s_s].astype(np.float64)
    target = None
    if rng.random() < .85:
        target = float(rng.uniform(-70, -40))
        x = scale_rms(x, target)
        x = add_music(x, rng, bg, target, float(rng.uniform(-10, 4)))
    y, pre = place(np.clip(x, -32768, 32767).astype(np.float32), rng)
    return dict(pcm=y, win=None, target_rms_dbfs=target, split_depth=d)


def plus_neg(g, other, rng, bg, levels):
    ar = active_region(g['pcm'])
    if ar is None:
        return None
    a, b, e = ar
    sp = split_point(g['pcm'], a, b, e)
    if sp is None:
        return None
    s, d = sp
    a_s, s_s = int(a * 160), int(s * 160)
    lo = max(0, a_s - 4800)
    first = fade(g['pcm'][lo:s_s].astype(np.float64))
    oa = active_region(other['pcm'])
    if oa is None:
        return None
    olo, ohi = max(0, int(oa[0] * 160) - 1600), min(len(other['pcm']), int(oa[1] * 160) + 1600)
    second = fade(other['pcm'][olo:ohi].astype(np.float64))
    gap = int(rng.uniform(0, .3) * 16000)
    x = np.concatenate([first, np.zeros(gap), second])
    target = None
    if rng.random() < .85:
        target = float(rng.uniform(-70, -40))
        x = scale_rms(x, target)
        x = add_music(x, rng, bg, target, float(rng.uniform(-10, 4)))
    y, pre = place(np.clip(x, -32768, 32767).astype(np.float32), rng)
    return dict(pcm=y, win=None, target_rms_dbfs=target, split_depth=d, gap_ms=gap / 16)


def gap_neg(g1, g2, rng, bg, levels):
    ar1, ar2 = active_region(g1['pcm']), active_region(g2['pcm'])
    if ar1 is None or ar2 is None:
        return None
    a1, b1, e1 = ar1
    a2, b2, e2 = ar2
    sp1 = split_point(g1['pcm'], a1, b1, e1)
    sp2 = split_point(g2['pcm'], a2, b2, e2)
    if sp1 is None or sp2 is None:
        return None
    s1, d1 = sp1
    s2, d2 = sp2
    a1_s, b2_s = int(a1 * 160), int(b2 * 160)
    s1_s, s2_s = int(s1 * 160), int(s2 * 160)
    lo1 = max(0, a1_s - 4800)
    hi2 = min(len(g2['pcm']), b2_s + 8000)
    first = fade(g1['pcm'][lo1:s1_s].astype(np.float64))
    second = fade(g2['pcm'][s2_s:hi2].astype(np.float64))
    gap = int(rng.uniform(1.0, 1.5) * 16000)
    x = np.concatenate([first, np.zeros(gap), second])
    target = None
    if rng.random() < .85:
        target = float(rng.uniform(-70, -40))
        x = scale_rms(x, target)
        x = add_music(x, rng, bg, target, float(rng.uniform(-10, 4)))
    y, pre = place(np.clip(x, -32768, 32767).astype(np.float32), rng)
    return dict(pcm=y, win=None, target_rms_dbfs=target, split_depths=[d1, d2], gap_ms=gap / 16)


def tts_pos(s, rng, bg, levels):
    ar = active_region(s['pcm'])
    if ar is None:
        return None
    a, b, e = ar
    a_s, b_s = int(a * 160), int(b * 160)
    lo = max(0, a_s - 4800)
    hi = min(len(s['pcm']), b_s + 8000)
    x = s['pcm'][lo:hi].astype(np.float64)
    wa, wb = a_s - lo, b_s - lo
    r = 1.0
    if rng.random() < .7:
        r = float(rng.uniform(.85, 1.25))
        x = M.speed(x, r)
        wa, wb = int(wa / r), int(wb / r)
    target = None
    if rng.random() < .7:
        target = float(rng.choice(levels))
        x = scale_rms(x, target)
        x = add_music(x, rng, bg, target, float(rng.uniform(-10, 4)))
    y, pre = place(np.clip(x, -32768, 32767).astype(np.float32), rng)
    return dict(pcm=y, win=(pre + wa, pre + wb), speed=r, target_rms_dbfs=target)


def tts_neg(s, rng, bg):
    ar = active_region(s['pcm'])
    if ar is None:
        return None
    lo, hi = max(0, int(ar[0] * 160) - 1600), min(len(s['pcm']), int(ar[1] * 160) + 1600)
    x = s['pcm'][lo:hi].astype(np.float64)
    target = None
    if rng.random() < .6:
        target = float(rng.uniform(-70, -40))
        x = scale_rms(x, target)
        x = add_music(x, rng, bg, target, float(rng.uniform(-10, 4)))
    y, pre = place(np.clip(x, -32768, 32767).astype(np.float32), rng)
    return dict(pcm=y, win=None, target_rms_dbfs=target)


def long_neg(s2train, pool, rng, levels):
    """6-10 s music crop with 1-3 non-keyword utterances inserted at s3 speech levels."""
    n = int(rng.uniform(6, 10) * 16000)
    if len(s2train) <= n or not pool:
        return None
    s = int(rng.integers(0, len(s2train) - n))
    crop = s2train[s:s + n].astype(np.float64)
    target = float(rng.choice(levels)) if rng.random() < .85 else float(rng.uniform(-70, -44))
    crop = scale_rms(crop, target)
    parents, used = [], []
    for _ in range(int(rng.integers(1, 4))):
        o = pool[int(rng.integers(len(pool)))]
        ar = active_region(o['pcm'])
        if ar is None:
            continue
        lo, hi = max(0, int(ar[0] * 160) - 1600), min(len(o['pcm']), int(ar[1] * 160) + 1600)
        seg = o['pcm'][lo:hi].astype(np.float64)
        if len(seg) > n:
            seg = seg[:n]
        seg = fade(scale_rms(seg, float(rng.choice(levels))))
        pos = int(rng.integers(0, n - len(seg) + 1))
        for _try in range(5):
            if all(not (pos < b and pos + len(seg) > a) for a, b in used):
                break
            pos = int(rng.integers(0, n - len(seg) + 1))
        crop[pos:pos + len(seg)] += seg
        used.append((pos, pos + len(seg)))
        parents.append(o['id'])
    return dict(pcm=np.clip(crop, -32768, 32767).astype(np.float32), win=None,
                target_rms_dbfs=target, n_insert=len(parents), insert_parents=parents)


ORDER_PERMS = [(1, 0, 2, 3), (0, 2, 1, 3), (0, 3, 1, 2), (1, 2, 0, 3)]
ORDER_MODES = ['reverse'] * 2 + ['perm'] * 4 + ['truncate'] * 4 + ['single'] * 2 + ['sylrev'] * 2


def order_neg(g, rng, bg, levels, mode):
    """Real-channel negative built from one 豆包豆包 group with the syllable order broken.

    modes: reverse (3-2-1-0), perm (one of four non-identity permutations),
    truncate (drop one syllable), single (one 豆包 only), sylrev (reverse each
    syllable in place).  The clip is then speed-perturbed, level-matched to the
    s3 RMS distribution and mixed with s2 music, exactly like the positives.
    """
    segs = group_syllables(g)
    if segs is None:
        return None
    if mode == 'reverse':
        order = [3, 2, 1, 0]
        use = segs
    elif mode == 'perm':
        order = list(ORDER_PERMS[int(rng.integers(len(ORDER_PERMS)))])
        use = segs
    elif mode == 'truncate':
        drop = int(rng.integers(4))
        order = [i for i in range(4) if i != drop]
        use = segs
    elif mode == 'single':
        order = [0, 1] if rng.random() < .5 else [2, 3]
        use = segs
    elif mode == 'sylrev':
        order = [0, 1, 2, 3]
        use = [s[::-1].copy() for s in segs]
    else:
        raise ValueError(mode)
    parts = []
    for j, i in enumerate(order):
        parts.append(fade(use[i].copy()))
        if j < len(order) - 1:
            parts.append(np.zeros(int(rng.uniform(0, .12) * 16000), np.float64))
    x = np.concatenate(parts)
    r = 1.0
    if rng.random() < .6:
        r = float(rng.uniform(.9, 1.15))
        x = M.speed(x, r)
    target = None
    if rng.random() < .85:
        target = float(rng.choice(levels))
        x = scale_rms(x, target)
        x = add_music(x, rng, bg, target, float(rng.uniform(-10, 4)))
    y, pre = place(np.clip(x, -32768, 32767).astype(np.float32), rng)
    return dict(pcm=y, win=None, target_rms_dbfs=target, speed=r, order=mode,
                order_indices=order)


def build_dataset(allowed_groups, tts, s1neg, bg, levels, seed, cfg):
    rng = np.random.default_rng(seed)
    exs, manifest = [], []
    counter = [0]

    def add(e, op, parents, meta=None):
        if e is None:
            return
        counter[0] += 1
        e['id'] = f'{cfg["tag"]}:{counter[0]:04d}:{op}'
        e['op'] = op
        e['parents'] = list(parents)
        e['meta'] = dict(meta or {})
        exs.append(e)
        manifest.append({'id': e['id'], 'kind': e['kind'], 'op': op, 'parents': list(parents),
                         'speed': e.get('speed'), 'target_rms_dbfs': e.get('target_rms_dbfs'),
                         'gap_ms': e.get('gap_ms'), 'split_depths': e.get('split_depths'),
                         'split_depth': e.get('split_depth'), 'win_samples': e.get('win'),
                         'meta': e.get('meta', {})})

    # Real group positives: two independent augmentations each.
    for g in allowed_groups:
        for k in range(2):
            e = pos_from_group(g, rng, bg, levels)
            if e is not None:
                e['kind'] = 'pos'
                add(e, 'augment', [g['id']], {'copy': k})

    # Cross-spliced positives: first half of one, second half of another.
    n_splice = cfg['splice']
    for _ in range(n_splice):
        g1 = allowed_groups[int(rng.integers(len(allowed_groups)))]
        g2 = allowed_groups[int(rng.integers(len(allowed_groups)))]
        if g1['id'] == g2['id']:
            continue
        e = splice_pos(g1, g2, rng, bg, levels)
        if e is not None:
            e['kind'] = 'pos'
            add(e, 'splice', [g1['id'], g2['id']])

    # Hard negatives built from positives.
    for _ in range(cfg['single']):
        g = allowed_groups[int(rng.integers(len(allowed_groups)))]
        e = single_neg(g, rng, bg, levels)
        if e is not None:
            e['kind'] = 'neg'
            add(e, 'single-kw', [g['id']])

    pool_other = [s for s in tts if s['split'] == 'train' and s['kind'] in ('neg', 'hardneg')] + s1neg
    for _ in range(cfg['plus']):
        g = allowed_groups[int(rng.integers(len(allowed_groups)))]
        o = pool_other[int(rng.integers(len(pool_other)))]
        e = plus_neg(g, o, rng, bg, levels)
        if e is not None:
            e['kind'] = 'neg'
            add(e, 'kw-plus-speech', [g['id'], o['id']])

    for _ in range(cfg['gap']):
        g1 = allowed_groups[int(rng.integers(len(allowed_groups)))]
        g2 = allowed_groups[int(rng.integers(len(allowed_groups)))]
        if g1['id'] == g2['id']:
            continue
        e = gap_neg(g1, g2, rng, bg, levels)
        if e is not None:
            e['kind'] = 'neg'
            add(e, 'kw-gap-1s', [g1['id'], g2['id']])

    # TTS positives (pure keyword only) and negatives/hard negatives.
    tts_train = [s for s in tts if s['split'] == 'train' and s['sub'] != 'homophone']
    pure = sorted([s for s in tts_train if s['kind'] == 'pos' and s['text'] in M.PURE], key=lambda s: s['id'])
    negs = sorted([s for s in tts_train if s['kind'] == 'neg'], key=lambda s: s['id'])
    hard = sorted([s for s in tts_train if s['sub'] == 'hardneg'], key=lambda s: s['id'])
    for s in [pure[int(i)] for i in rng.choice(len(pure), min(cfg['tts_pos'], len(pure)), False)]:
        e = tts_pos(s, rng, bg, levels)
        if e is not None:
            e['kind'] = 'pos'
            add(e, 'tts-pos', [s['id']], {'text': s['text']})
    for s in [negs[int(i)] for i in rng.choice(len(negs), min(cfg['tts_neg'], len(negs)), False)]:
        e = tts_neg(s, rng, bg)
        if e is not None:
            e['kind'] = 'neg'
            add(e, 'tts-neg', [s['id']], {'text': s['text']})
    for s in [hard[int(i)] for i in rng.choice(len(hard), min(cfg['tts_hard'], len(hard)), False)]:
        e = tts_neg(s, rng, bg)
        if e is not None:
            e['kind'] = 'neg'
            add(e, 'tts-hardneg', [s['id']], {'text': s['text']})

    # Real-channel order negatives: same channel/speaker/level as the positives,
    # but the four syllables are reversed, permuted, truncated, or one 豆包 only.
    # At least as many as the positives, so they dominate the real-channel negatives.
    if cfg.get('order_neg', 0) > 0:
        n_pos = sum(e['kind'] == 'pos' for e in exs)
        n_order = max(int(cfg['order_neg']), n_pos)
        plan = [ORDER_MODES[i % len(ORDER_MODES)] for i in range(n_order)]
        rng.shuffle(plan)
        for mode in plan:
            g = allowed_groups[int(rng.integers(len(allowed_groups)))]
            e = order_neg(g, rng, bg, levels, mode)
            if e is not None:
                e['kind'] = 'neg'
                add(e, f'order-{mode}', [g['id']],
                    {'order': e.get('order'), 'order_indices': e.get('order_indices')})

    for s in s1neg:
        for k in range(2):
            e = tts_neg(s, rng, bg)
            if e is not None:
                e['kind'] = 'neg'
                add(e, 's1-neg', [s['id']], {'copy': k})

    # Music-only negatives from s2 (never s2b).
    m = bg['music']
    for k in range(cfg['music']):
        n = int(rng.uniform(1.5, 3.0) * 16000)
        if len(m) <= n:
            continue
        s = int(rng.integers(0, len(m) - n))
        add({'pcm': m[s:s + n].astype(np.float32), 'win': None, 'kind': 'neg'},
            'music', ['s2-music-silent'], {'copy': k})

    # Long music negatives with inserted non-keyword speech (round 2).
    n_long = cfg.get('long_neg', 0)
    added = tries = 0
    while added < n_long and tries < n_long * 4:
        tries += 1
        e = long_neg(m, pool_other, rng, levels)
        if e is None:
            continue
        e['kind'] = 'neg'
        add(e, 'long-music-speech', e.pop('insert_parents'), {'insert': e.pop('n_insert')})
        added += 1

    # S16 features and loss windows.
    kept = []
    for e in exs:
        f = mfcc16(e['pcm'])
        if len(f) < 10:
            continue
        if e['kind'] == 'pos':
            s, t = e['win']
            f0 = max(7, int(s / 160) - 30)                            # [start-0.3, end+0.5]
            f1 = min(len(f) - 1, int(t / 160) + 50)
            if f1 < f0:
                continue
            e['wf'] = (f0, f1)
        e['feat'] = f
        kept.append(e)
    print(f'  {cfg["tag"]}: examples {len(kept)} '
          f'(pos {sum(e["kind"]=="pos" for e in kept)}, neg {sum(e["kind"]=="neg" for e in kept)}) '
          f'frames {sum(len(e["feat"]) for e in kept)}', flush=True)
    return kept, manifest


# ---------------- training
def batch_tensor(batch):
    T = max(len(e['feat']) for e in batch)
    X = np.zeros((len(batch), 40, T + 49), np.float32)
    for j, e in enumerate(batch):
        X[j, :, :len(e['feat']) + 49] = M.padded(e['feat']).T
    return torch.from_numpy(X), T


def set_trainable(net, mode):
    for p in net.parameters():
        p.requires_grad = True
    if mode == 'last-affine':
        for p in net.l.parameters():
            p.requires_grad = False
    elif mode == 'last-two':
        for p in list(net.l)[:-1]:
            for q in p.parameters():
                q.requires_grad = False
    elif mode != 'none':
        raise ValueError(f'unknown freeze mode {mode}')
    return [p for p in net.parameters() if p.requires_grad]


def _run_epoch(net, net0, exs, opt, trainable, p0, prior, step, cfg, rng, ep, epochs, log):
    order = rng.permutation(len(exs))
    tot = n = 0
    pl_sum = nl_sum = an_sum = 0.0
    pt = torch.tensor(prior, dtype=torch.float32)[None, :, None]
    t0 = time.time()
    for off in range(0, len(order), cfg['batch']):
        batch = [exs[int(i)] for i in order[off:off + cfg['batch']]]
        X, T = batch_tensor(batch)
        ll = net(X)                                                    # B x 10 x T
        am = 0.1 * (ll - pt)
        marg = dviterbi_k(am.transpose(1, 2), step, int(cfg.get('dur_k', 1)))  # B x T
        mp_mask = torch.zeros_like(marg, dtype=torch.bool)
        mn_mask = torch.zeros_like(marg, dtype=torch.bool)
        is_pos = torch.zeros(len(batch), dtype=torch.bool)
        is_neg = torch.zeros(len(batch), dtype=torch.bool)
        for j, e in enumerate(batch):
            if e['kind'] == 'pos':
                is_pos[j] = True
                mp_mask[j, e['wf'][0]:e['wf'][1] + 1] = True
            else:
                is_neg[j] = True
                mn_mask[j, 7:len(e['feat'])] = True
        mp = marg.masked_fill(~mp_mask, BIG).min(1).values
        mn = marg.masked_fill(~mn_mask, BIG).min(1).values
        pos_loss = torch.relu(mp[is_pos] + cfg['delta']).mean() if is_pos.any() else torch.tensor(0.)
        neg_min = (torch.relu(cfg['delta'] - mn[is_neg]).mean() if is_neg.any()
                   else torch.tensor(0.))
        if cfg.get('neg_agg', 'min') in ('frames', 'both'):
            mask = mn_mask & is_neg[:, None]
            frame_loss = torch.relu(cfg['delta'] - marg)
            neg_frames = (frame_loss * mask).sum() / mask.sum().clamp(min=1)
        else:
            neg_frames = torch.tensor(0.)
        if cfg.get('neg_agg', 'min') == 'frames':
            neg_loss = neg_frames
        elif cfg.get('neg_agg', 'min') == 'both':
            neg_loss = neg_min + neg_frames
        else:
            neg_loss = neg_min
        anc = sum(((p - q) ** 2).mean() for p, q in zip(trainable, p0)) / len(p0)
        if cfg.get('kl_weight', 0) > 0:
            with torch.no_grad():
                base_ll = net0(X)
            kl = torch.nn.functional.kl_div(ll.reshape(-1, 10), base_ll.exp().reshape(-1, 10),
                                            reduction='batchmean')
        else:
            kl = torch.tensor(0.)
        loss = (cfg['pos_weight'] * pos_loss + cfg['neg_weight'] * neg_loss
                + cfg['anchor'] * anc + cfg.get('kl_weight', 0) * kl)
        opt.zero_grad()
        loss.backward()
        torch.nn.utils.clip_grad_norm_(trainable, 1.0)
        opt.step()
        tot += float(loss.item())
        pl_sum += float(pos_loss.item())
        nl_sum += float(neg_loss.item())
        an_sum += float(anc.item())
        n += 1
    log(f'  epoch {ep+1}/{epochs} loss {tot/n:.4f} pos {pl_sum/n:.4f} '
        f'neg {nl_sum/n:.4f} anchor {an_sum/n:.6f} {time.time()-t0:.1f}s')


def train_seq(net0, exs, prior, step, epochs, cfg, seed, log):
    net = copy.deepcopy(net0)
    net.train()
    trainable = set_trainable(net, cfg.get('freeze', 'none'))
    p0 = [p.detach().clone() for p in trainable]
    opt = torch.optim.Adam(trainable, lr=cfg['lr'], weight_decay=1e-5)
    rng = np.random.default_rng(seed)
    for ep in range(epochs):
        _run_epoch(net, net0, exs, opt, trainable, p0, prior, step, cfg, rng, ep, epochs, log)
    net.eval()
    return net


def select_key(m):
    return (0 if m['s2val_events'] == 0 else 1, -m['s3_groups'], m['dev']['neg']['accepted'])


def train_seq_online(net0, build_fn, prior, step, epochs, cfg, seed, log, eval_fn, manifest_fn):
    """Rebuild the constructed set every epoch; evaluate and keep the best epoch."""
    net = copy.deepcopy(net0)
    net.train()
    trainable = set_trainable(net, cfg.get('freeze', 'none'))
    p0 = [p.detach().clone() for p in trainable]
    opt = torch.optim.Adam(trainable, lr=cfg['lr'], weight_decay=1e-5)
    rng = np.random.default_rng(seed)
    history, best = [], None
    for ep in range(epochs):
        exs, manifest = build_fn(ep)
        manifest_fn(ep, manifest)
        _run_epoch(net, net0, exs, opt, trainable, p0, prior, step, cfg, rng, ep, epochs, log)
        m = eval_fn(net, ep)
        history.append(m)
        key = select_key(m)
        if best is None or key < best[0]:
            best = (key, ep, copy.deepcopy(net.state_dict()), m)
        log(f'  epoch {ep+1} eval: groups {m["s3_groups"]}/35 (inwin {m["s3_inwindow"]}, '
            f'34 {m["s3_groups34"]}/{m["s3_inwindow34"]}), s2b {m["s2b_events"]}, '
            f's2 {m["s2_events"]}, s2val {m["s2val_events"]}, '
            f'dev pos {m["dev"]["pos"]["accepted"]}/{m["dev"]["pos"]["n"]}, '
            f'hardneg {m["dev"]["hardneg"]["accepted"]}/{m["dev"]["hardneg"]["n"]}, '
            f'neg {m["dev"]["neg"]["accepted"]}/{m["dev"]["neg"]["n"]}, '
            f'homophone {m["dev"]["homophone"]["accepted"]}/{m["dev"]["homophone"]["n"]}')
    net.load_state_dict(best[2])
    net.eval()
    return net, history, best[1], best[3]


# ---------------- export / evaluation
def save_model(net, level, outdir, probe_feat, write_container=True):
    set_level(level)
    parsed, am = M.export(net)
    parts, fst, prior, step = r34_controls(level)
    blob = P.container(parts['conf'], parts['model'], parts['fst'], am, parts['prior'])
    top, end = P.load(blob)
    newparts = {k: v for d in top['list'] for k, v in d.items()}
    assert end == len(blob) and P.dump(top) == blob
    assert all(newparts[k] == parts[k] for k in ('conf', 'model', 'fst', 'prior'))
    err64, err32 = export_error(net, parsed, am, probe_feat)
    outdir.mkdir(parents=True, exist_ok=True)
    if write_container:
        (outdir / 'model.binn').write_bytes(blob)
    (outdir / 'root.list.3.am.bin').write_bytes(am)
    (outdir / 'am-decoded.json').write_text(json.dumps(parsed))
    (outdir / 'fst-decoded.json').write_text(json.dumps(fst, indent=2))
    (outdir / 'prior.json').write_text(json.dumps([float(x) for x in prior]))
    torch.save(net.state_dict(), outdir / 'net.pt')
    info = {'level': level, 'step': step, 'sha256': hashlib.sha256(blob).hexdigest(),
            'bytes': len(blob), 'export_maxerr': err64,
            'export_maxerr_float32': err32,
            'export_check': 'double-precision torch vs numpy forward',
            'non_am_equal_r34': True,
            'container_written': bool(write_container),
            'container_note': ('r34 FST/conf/prior container; AM replaced' if write_container
                               else 'reference only: r34 FST expresses k=1, this model was trained/evaluated with k>1')}
    (outdir / 'export.json').write_text(json.dumps(info, indent=2))
    assert err64 < 1e-5, err64
    return info, parsed, am, fst, prior


def load_net(level, path):
    net = M.Net(96 if level == 1 else 160)
    net.load_state_dict(torch.load(path, weights_only=True, map_location='cpu'))
    net.eval()
    return net


def events(m, thr=0, gap=100):
    hit = np.flatnonzero(np.array(m) < thr)
    ev = []
    for t in hit:
        if not ev or t - ev[-1][1] > gap:
            ev.append([int(t), int(t)])
        else:
            ev[-1][1] = int(t)
    return ev


def confirmed(l1, l2):
    out = []
    for a, b in events(l1, 0):
        if np.array(l2)[max(0, a - 100):b + 100].min() < 0:
            out.append([a, b])
    return out


def group_matrix(groups, margins):
    mids = [(a + b) / 200 for a, b in events(margins, 0)]
    rows = []
    for g in groups:
        f0 = max(0, int((g['start_s'] - .3) * 100))
        f1 = min(len(margins) - 1, int((g['end_s'] + .5) * 100))
        rows.append({'id': g['id'], 'start_s': g['start_s'], 'end_s': g['end_s'],
                     'mac_hit': any(g['start_s'] - .3 <= t <= g['end_s'] + .5 for t in mids),
                     'min_margin': float(np.array(margins)[f0:f1 + 1].min())})
    return rows


def gain_bias(feat, gain_db):
    """Digital gain folded into MFCC c0 (same convention as review/gain.py)."""
    if gain_db == 0:
        return feat
    f = feat.copy()
    f[:, 0] += gain_db * math.sqrt(40) * math.log(10) / 10
    return f


def eval_margins(parsed, am, fst, prior, pcm, k=1, step=None, gain_db=0.0):
    feat = gain_bias(mfcc16(pcm), gain_db)
    ll = F.forward(feat, parsed, am)
    if k == 1:
        return F.viterbi(ll, fst, prior)
    if step is None:
        step = max(a['weight'] for n in fst['graph'] for a in n['arcs']
                   if a['input'] != 0 and a['weight'] > 1.0)
    return viterbi_k_np(0.1 * (ll.astype(np.float64) - prior.astype(np.float64)), step, k)


def eval_dev(parsed, am, fst, prior, dev, bg_override=None, k=1, step=None, gain_db=0.0):
    rng = np.random.default_rng(7)
    if bg_override is None:
        m = wav(C / 's2b-music-silent.aec16k.wav')
        bg = {'quiet': m, 'music': m}
    else:
        bg = bg_override
    out = []
    for s in dev:
        y = M.augment(s['pcm'], rng, bg, False)[0]
        v = eval_margins(parsed, am, fst, prior, y, k=k, step=step, gain_db=gain_db)
        out.append([s['sub'], float(v.min()), s['id']])
    return out


def _isolated_acc(parsed, am, fst, prior, pcm, k, step, pad=4000):
    y = np.concatenate([np.zeros(pad, np.float32), np.asarray(pcm, np.float32),
                        np.zeros(pad, np.float32)])
    return float(eval_margins(parsed, am, fst, prior, y, k=k, step=step).min()) < 0


def robust_clip_metrics(parsed, am, fst, prior, groups, k, step):
    """Acceptance on isolated real clips: forward, time-reversed, syllable-permuted, first half.

    This is the direct test of the "real channel energy => keyword" shortcut.  The
    supervisor probe used 4000 samples of zero padding; we keep that convention so
    the numbers are comparable to temp/analysis/wake-seq/review/reverse.json.
    """
    out = {'n': len(groups), 'forward': 0, 'reversed': 0, 'permuted': 0, 'first_half': 0,
           'n_permuted': 0, 'n_first_half': 0}
    for g in groups:
        pcm = g['pcm'].astype(np.float32)
        out['forward'] += _isolated_acc(parsed, am, fst, prior, pcm, k, step)
        out['reversed'] += _isolated_acc(parsed, am, fst, prior, pcm[::-1].copy(), k, step)
        syl = group_syllables(g)
        if syl is not None:
            gap = np.zeros(800, np.float64)
            perm = np.concatenate([fade(syl[2]), gap, syl[3], gap, fade(syl[0]), gap, syl[1]])
            out['permuted'] += _isolated_acc(parsed, am, fst, prior, perm, k, step)
            out['n_permuted'] += 1
        ar = active_region(g['pcm'])
        sp = split_point(g['pcm'], ar[0], ar[1], ar[2]) if ar is not None else None
        if sp is not None:
            out['first_half'] += _isolated_acc(parsed, am, fst, prior,
                                               pcm[:int(sp[0] * 160)], k, step)
            out['n_first_half'] += 1
    return out


def gain_metrics(parsed, am, fst, prior, groups, dev, dev_bg, s3_pcm, s2b_pcm, s2_pcm,
                 k, step, gains=(-10, 0, 12)):
    out = {}
    for gdb in gains:
        s3 = eval_margins(parsed, am, fst, prior, s3_pcm, k=k, step=step, gain_db=gdb)
        s2b = eval_margins(parsed, am, fst, prior, s2b_pcm, k=k, step=step, gain_db=gdb)
        s2 = eval_margins(parsed, am, fst, prior, s2_pcm, k=k, step=step, gain_db=gdb)
        gm = group_matrix(groups, s3)
        devm = eval_dev(parsed, am, fst, prior, dev, dev_bg, k=k, step=step, gain_db=gdb)
        out[f'{gdb:+d}'] = {
            's3_groups': int(sum(r['mac_hit'] for r in gm)),
            's3_inwindow': int(sum(r['min_margin'] < 0 for r in gm)),
            's2b_events': len(events(s2b, 0)),
            's2_events': len(events(s2, 0)),
            'dev': {kk: {'accepted': int(sum(d[1] < 0 for d in devm if d[0] == kk)),
                         'n': int(sum(d[0] == kk for d in devm))}
                    for kk in ('pos', 'hardneg', 'neg', 'homophone')},
        }
    return out


def summarize(groups, s3, s2b, s2, dev):
    gm = group_matrix(groups, s3)
    gm34 = [r for r in gm if not r['id'].endswith('@98.67')]
    out = {'s3_groups': int(sum(r['mac_hit'] for r in gm)),
           's3_inwindow': int(sum(r['min_margin'] < 0 for r in gm)),
           's3_groups34': int(sum(r['mac_hit'] for r in gm34)),
           's3_inwindow34': int(sum(r['min_margin'] < 0 for r in gm34)),
           's2b_events': len(events(s2b, 0)),
           's2_events': len(events(s2, 0)),
           's3_events': len(events(s3, 0)),
           'dev': {}}
    for k in ('pos', 'hardneg', 'neg', 'homophone'):
        vals = [d[1] for d in dev if d[0] == k]
        out['dev'][k] = {'accepted': int(sum(v < 0 for v in vals)), 'n': len(vals)}
    return out, gm


def eval_net(net, level, outdir, groups, dev, tag, log, dev_bg=None, s2val_pcm=None,
             dur_k=1, robust=False, gain_sweep=False, s1groups=None):
    probe = mfcc16(wav(C / 's3-music-pos.aec16k.wav'))[:150]            # fixed ~1.5 s probe
    info, parsed, am, fst, prior = save_model(net, level, outdir, probe,
                                              write_container=(dur_k == 1))
    s3_pcm = wav(C / 's3-music-pos.aec16k.wav')
    s2b_pcm = wav(C / 's2b-music-silent.aec16k.wav')
    s2_pcm = wav(C / 's2-music-silent.aec16k.wav')
    s3 = eval_margins(parsed, am, fst, prior, s3_pcm, k=dur_k, step=info['step'])
    s2b = eval_margins(parsed, am, fst, prior, s2b_pcm, k=dur_k, step=info['step'])
    s2 = eval_margins(parsed, am, fst, prior, s2_pcm, k=dur_k, step=info['step'])
    s2v = (eval_margins(parsed, am, fst, prior, s2val_pcm, k=dur_k, step=info['step'])
           if s2val_pcm is not None else None)
    devm = eval_dev(parsed, am, fst, prior, dev, dev_bg, k=dur_k, step=info['step'])
    summary, gm = summarize(groups, s3, s2b, s2, devm)
    if s2v is not None:
        summary['s2val_events'] = len(events(s2v, 0))
    summary['dur_k'] = dur_k
    summary['r34_fst_expressible'] = (dur_k == 1)
    if robust and s1groups is not None:
        summary['robust'] = {
            's1': robust_clip_metrics(parsed, am, fst, prior, s1groups, dur_k, info['step']),
            's3': robust_clip_metrics(parsed, am, fst, prior, groups, dur_k, info['step']),
        }
    if gain_sweep:
        summary['gain'] = gain_metrics(parsed, am, fst, prior, groups, dev, dev_bg,
                                       s3_pcm, s2b_pcm, s2_pcm, dur_k, info['step'])
    res = dict(info)
    res.update(summary)
    res['tag'] = tag
    outdir.mkdir(parents=True, exist_ok=True)
    payload = {'s3-music-pos': s3.tolist(), 's2b-music-silent': s2b.tolist(),
               's2-music-silent': s2.tolist(), 'dev': devm}
    if s2v is not None:
        payload['s2val'] = s2v.tolist()
    (outdir / 'eval.json').write_text(json.dumps(payload, indent=2))
    (outdir / 'matrix.json').write_text(json.dumps(gm, indent=2))
    extra = f', s2val {summary["s2val_events"]}' if s2v is not None else ''
    log(f'  {tag}: L{level} groups {summary["s3_groups"]}/35 '
        f'(inwin {summary["s3_inwindow"]}, 34 {summary["s3_groups34"]}/{summary["s3_inwindow34"]}), '
        f's2b {summary["s2b_events"]}, s2 {summary["s2_events"]}{extra}, '
        f'dev pos {summary["dev"]["pos"]["accepted"]}/{summary["dev"]["pos"]["n"]}, '
        f'hardneg {summary["dev"]["hardneg"]["accepted"]}/{summary["dev"]["hardneg"]["n"]}, '
        f'neg {summary["dev"]["neg"]["accepted"]}/{summary["dev"]["neg"]["n"]}, '
        f'export {info["export_maxerr"]:.2e}')
    return res, gm, {'s3': s3, 's2b': s2b, 's2': s2, 's2val': s2v, 'dev': devm}


def torch_margins(net, feat, prior, step, k=1):
    with torch.no_grad():
        x = torch.from_numpy(M.padded(feat).T[None].astype(np.float32))
        ll = net(x)[0].T.numpy()
    am = (0.1 * (ll.astype(np.float32) - prior)).astype(np.float32)[None]
    return dviterbi_k(torch.from_numpy(am), step, k)[0].numpy()


def make_eval_cache(level, groups, dev, bg_eval, s2val_pcm):
    """Cache MFCC features once; per-epoch selection then only runs the network."""
    set_level(level)
    cache = {'groups': groups,
             's3': mfcc16(wav(C / 's3-music-pos.aec16k.wav')),
             's2b': mfcc16(wav(C / 's2b-music-silent.aec16k.wav')),
             's2': mfcc16(wav(C / 's2-music-silent.aec16k.wav')),
             's2val': mfcc16(s2val_pcm)}
    rng = np.random.default_rng(7)
    cache['dev'] = [[s['sub'], mfcc16(M.augment(s['pcm'], rng, bg_eval, False)[0]), s['id']]
                    for s in dev]
    return cache


def eval_cache_metrics(net, cache, prior, step, k=1):
    s3 = torch_margins(net, cache['s3'], prior, step, k)
    s2b = torch_margins(net, cache['s2b'], prior, step, k)
    s2 = torch_margins(net, cache['s2'], prior, step, k)
    s2v = torch_margins(net, cache['s2val'], prior, step, k)
    devm = [[sub, float(torch_margins(net, feat, prior, step, k).min()), sid]
            for sub, feat, sid in cache['dev']]
    summary, _ = summarize(cache['groups'], s3, s2b, s2, devm)
    summary['s2val_events'] = len(events(s2v, 0))
    summary['dur_k'] = k
    return summary


def eval_cascade(l1dir, l2dir, groups, dev, tag, log):
    l1 = json.loads((l1dir / 'eval.json').read_text())
    l2 = json.loads((l2dir / 'eval.json').read_text())
    l1m, l2m = np.array(l1['s3-music-pos']), np.array(l2['s3-music-pos'])
    ev = confirmed(l1m, l2m)
    mids = [(a + b) / 200 for a, b in ev]
    gm = []
    for g in groups:
        f0 = max(0, int((g['start_s'] - .3) * 100))
        f1 = min(len(l1m) - 1, int((g['end_s'] + .5) * 100))
        gm.append({'id': g['id'], 'start_s': g['start_s'], 'end_s': g['end_s'],
                   'mac_hit': any(g['start_s'] - .3 <= t <= g['end_s'] + .5 for t in mids),
                   'l1_min_margin': float(l1m[f0:f1 + 1].min()),
                   'l2_min_margin': float(l2m[f0:f1 + 1].min())})
    s2b = confirmed(l1['s2b-music-silent'], l2['s2b-music-silent'])
    s2 = confirmed(l1['s2-music-silent'], l2['s2-music-silent'])
    d1 = {d[2]: d for d in l1['dev']}
    d2 = {d[2]: d for d in l2['dev']}
    devc = []
    for k in d1:
        if k in d2:
            devc.append([d1[k][0], d1[k][1] if d2[k][1] < 0 else 1e9, k])
    out = {'s3_groups': int(sum(r['mac_hit'] for r in gm)),
           's2b_events': len(s2b), 's2_events': len(s2), 'dev': {}}
    for k in ('pos', 'hardneg', 'neg', 'homophone'):
        vals = [d[1] for d in devc if d[0] == k]
        out['dev'][k] = {'accepted': int(sum(v < 0 for v in vals)), 'n': len(vals)}
    out['tag'] = tag
    d = l2dir.parent / f'{l2dir.name}-cascade'
    d.mkdir(parents=True, exist_ok=True)
    (d / 'eval.json').write_text(json.dumps({'s3-music-pos': l1m.tolist(), 's2b-music-silent': l1['s2b-music-silent'],
                                             's2-music-silent': l1['s2-music-silent'], 'dev': devc,
                                             'confirmed_events': ev}, indent=2))
    (d / 'matrix.json').write_text(json.dumps(gm, indent=2))
    log(f'  {tag}: cascade groups {out["s3_groups"]}/35, s2b {out["s2b_events"]}, s2 {out["s2_events"]}, '
        f'dev pos {out["dev"]["pos"]["accepted"]}/{out["dev"]["pos"]["n"]}, '
        f'hardneg {out["dev"]["hardneg"]["accepted"]}/{out["dev"]["hardneg"]["n"]}')
    return out, gm


# ---------------- missed-group analysis
def miss_analysis(groups, margins):
    rows = []
    for g in groups:
        f0 = max(0, int((g['start_s'] - .3) * 100))
        f1 = min(len(margins) - 1, int((g['end_s'] + .5) * 100))
        w = np.array(margins)[f0:f1 + 1]
        ar = active_region(g['pcm'])
        sp = split_point(g['pcm'], ar[0], ar[1], ar[2]) if ar is not None else None
        e = M.frame_db(g['pcm'])
        act = e > e.max() - 30
        rows.append({'id': g['id'], 'start_s': g['start_s'], 'end_s': g['end_s'],
                     'min_margin': float(w.min()), 'min_margin_s': (f0 + int(w.argmin())) / 100,
                     'clip_rms_dbfs': g['clip_rms_dbfs'], 'peak_s16': g['peak_s16'],
                     'active_seconds': float(act.sum() / 100) if len(act) else 0,
                     'zero_seconds': g['zero_seconds'], 'member_count': g['member_count'],
                     'split_depth_db': float(sp[1]) if sp else None,
                     'active_rms_dbfs': float(10 * np.log10(np.mean(10 ** (e[act] / 10)))) if act.any() else None,
                     'margin_curve': [float(x) for x in w]})
    return rows


# ---------------- report
def write_report(result, path):
    s0 = result.get('stage0', {})
    lines = ['# Report: sequence-level (differentiable Viterbi) wake training', '',
             '2026-11-03. Mac-only experiment. No SSH, device access, recording, cloud upload or deployment. '
             'All numbers below are Mac reference scores under the r34 control fields '
             '(L1 FST forward weight 2.25, L2 forward weight 3.0, min-self-loop 0); they are not ARM or device results.', '',
             '## Method', '',
             '- Fine-tunes the AM only; conf/model/fst/prior bytes stay equal to the r34 review containers.',
             '- Torch differentiable Viterbi reproduces `tools/native-model-forward.py` frame margins '
             '(unit test max error < 1e-3); hard-min path subgradient.',
             '- Positive loss `relu(min_t margin + delta)` on frames [start-0.3, end+0.5]; negative loss '
             '`relu(delta - min_t margin)` on all valid frames; small L2 anchor to the starting parameters.',
             '- Constructed samples: two-豆包 split at the energy valley, cross-splicing different sentences, '
             '0-400 ms gaps; s2 music mixing at the s3 RMS distribution; single-豆包, 豆包+speech and '
             'two-豆包 >=1 s hard negatives. s2b is never used in training.', '',
             '## Stage 0: L1 full-data fit (known-set, not generalization)', '']
    if s0:
        b = s0.get('baseline', {})
        c = s0.get('candidate', {})
        lines += ['| model | L1 35 groups | s2b events | s2 events | dev pos | dev hardneg | dev neg | dev homophone |',
                  '|---|---|---|---|---|---|---|---|']
        for name, v in [('v5 baseline', b), ('sequence candidate', c)]:
            if not v:
                continue
            d = v['dev']
            lines.append(f'| {name} | {v["s3_groups"]}/35 | {v["s2b_events"]} | {v["s2_events"]} | '
                         f'{d["pos"]["accepted"]}/{d["pos"]["n"]} | {d["hardneg"]["accepted"]}/{d["hardneg"]["n"]} | '
                         f'{d["neg"]["accepted"]}/{d["neg"]["n"]} | {d["homophone"]["accepted"]}/{d["homophone"]["n"]} |')
        lines += ['', f'Gate: {"PASS" if s0.get("gate") else "FAIL"}; '
                  f'training time {s0.get("seconds", 0):.1f}s; examples {s0.get("examples", "?")}.', '']
        if not s0.get('gate'):
            lines += ['The feasibility gate requires 35/35 L1 groups with 0 events on both music clips. '
                      'Missed groups and their minimum-margin windows are in '
                      '`stage0/missed-groups.json`; the per-group feature summary follows.', '',
                      '| group | start-end s | min margin | at s | clip RMS dBFS | active s | split depth dB |',
                      '|---|---|---|---|---|---|---|']
            for r in s0.get('missed', []):
                lines.append(f'| {r["id"]} | {r["start_s"]:.2f}-{r["end_s"]:.2f} | {r["min_margin"]:.2f} | '
                             f'{r["min_margin_s"]:.2f} | {r["clip_rms_dbfs"]:.1f} | {r["active_seconds"]:.2f} | '
                             f'{(r["split_depth_db"] or 0):.1f} |')
            lines += ['', 'No later stage was run because the known-set fit did not reach the gate. '
                      'This is a negative result: it does not establish CV generalization or device behavior.', '']
    if result.get('stage1'):
        lines += ['## Stage 1: 5-fold CV (time-ordered 35 s3 groups, 7 per fold)', '',
                  'Test-fold original clips and every derived/spliced sample with a test-fold parent are excluded. '
                  's3 unlabelled gap audio is never used.', '',
                  '| fold | test groups | L1 hits | s2b events | s2 events |', '|---|---|---|---|---|']
        for f in result['stage1']['folds']:
            lines.append(f'| {f["fold"]} | {f["test_ids"][0]}..{f["test_ids"][-1]} | '
                         f'{f["l1"]["s3_groups"]}/7 | {f["l1"]["s2b_events"]} | {f["l1"]["s2_events"]} |')
        lines += ['', f'CV total L1: {result["stage1"]["total_hits"]}/35 groups, '
                  f's2b events {result["stage1"]["s2b_events"]}, s2 events {result["stage1"]["s2_events"]}. '
                  'This is the only generalization number in this report.', '']
    if result.get('stage2'):
        lines += ['## Stage 2: L2 and cascade (full-data fit)', '',
                  '| model | cascade 35 groups | s2b events | s2 events | dev pos | dev hardneg | dev neg |',
                  '|---|---|---|---|---|---|---|']
        v = result['stage2']['cascade']
        d = v['dev']
        lines.append(f'| L1+L2 | {v["s3_groups"]}/35 | {v["s2b_events"]} | {v["s2_events"]} | '
                     f'{d["pos"]["accepted"]}/{d["pos"]["n"]} | {d["hardneg"]["accepted"]}/{d["hardneg"]["n"]} | '
                     f'{d["neg"]["accepted"]}/{d["neg"]["n"]} |')
        l2 = result['stage2']['l2']
        d2 = l2['dev']
        lines.append(f'| L2 alone | {l2["s3_groups"]}/35 | {l2["s2b_events"]} | {l2["s2_events"]} | '
                     f'{d2["pos"]["accepted"]}/{d2["pos"]["n"]} | {d2["hardneg"]["accepted"]}/{d2["hardneg"]["n"]} | '
                     f'{d2["neg"]["accepted"]}/{d2["neg"]["n"]} |')
        lines.append('')
    if result.get('cv'):
        lines += ['## Cascade 5-fold CV', '',
                  '| fold | cascade hits | s2b events |', '|---|---|---|']
        for f in result['cv']['folds']:
            lines.append(f'| {f["fold"]} | {f["cascade"]["s3_groups"]}/7 | {f["cascade"]["s2b_events"]} |')
        lines += ['', f'Cascade CV total: {result["cv"]["total_hits"]}/35 groups, '
                  f's2b events {result["cv"]["s2b_events"]}.', '']
    if result.get('final'):
        lines += ['## Final candidate', '',
                  '| level | SHA256 | bytes | export max error | non-AM bytes equal r34 |',
                  '|---|---|---|---|---|']
        for k, v in result['final'].items():
            lines.append(f'| {k} | {v["sha256"]} | {v["bytes"]} | {v["export_maxerr"]:.2e} | {v["non_am_equal_r34"]} |')
        lines.append('')
    lines += ['## Limits', '',
              '- All scores are Mac reference outputs from the numpy forward pass. They are not ARM two-stage '
              'confirmation, not device acceptance, and not evidence of live recall.',
              '- Stage 0 is a known-set fit (all 35 groups in training). Only the 5-fold CV rows estimate '
              'unseen-group behavior.',
              '- s2b is the independent music veto and never enters gradients; s2 is a training background.',
              '- No model was promoted or deployed.']
    Path(path).write_text('\n'.join(lines) + '\n')


# ---------------- stage runners
def cfg_defaults(args):
    return {'tag': 'full', 'lr': args.lr, 'batch': args.batch, 'delta': args.delta,
            'anchor': args.anchor, 'pos_weight': args.pos_weight, 'neg_weight': args.neg_weight,
            'kl_weight': args.kl_weight, 'neg_agg': args.neg_agg, 'freeze': args.freeze,
            'long_neg': args.long_neg, 'order_neg': args.order_neg, 'dur_k': args.dur_k,
            'splice': args.splice, 'single': args.single,
            'plus': args.plus, 'gap': args.gap, 'tts_pos': args.tts_pos,
            'tts_neg': args.tts_neg, 'tts_hard': args.tts_hard, 'music': args.music}


def run_stage0(args, s1groups, s3groups, tts, s1neg, dev, bg, levels, log):
    cfg = cfg_defaults(args)
    cfg['tag'] = 'stage0'
    exs, manifest = build_dataset(s1groups + s3groups, tts, s1neg, bg, levels, args.seed, cfg)
    (OUT / 'stage0').mkdir(parents=True, exist_ok=True)
    (OUT / 'stage0' / 'manifest.json').write_text(json.dumps(manifest, indent=2))
    parts, fst, prior, step = r34_controls(1)
    base = load_net(1, BASE[1] / 'net.pt')
    t0 = time.time()
    net = train_seq(base, exs, prior, step, args.epochs, cfg, args.seed + 1, log)
    seconds = time.time() - t0
    log(f'  stage0 training {seconds:.1f}s')
    base_res, _, _ = eval_net(base, 1, OUT / 'stage0' / 'baseline', s3groups, dev, 'v5-baseline', log,
                              dur_k=cfg['dur_k'])
    cand, gm, mm = eval_net(net, 1, OUT / 'stage0' / 'l1', s3groups, dev, 'seq-l1', log,
                            dur_k=cfg['dur_k'])
    missed = [r for r in gm if not r['mac_hit']]
    miss = miss_analysis([g for g in s3groups if not any(r['id'] == g['id'] and r['mac_hit'] for r in gm)],
                         mm['s3'])
    (OUT / 'stage0' / 'missed-groups.json').write_text(json.dumps(miss, indent=2))
    gate = cand['s3_groups'] == 35 and cand['s2b_events'] == 0 and cand['s2_events'] == 0
    res = {'seconds': seconds, 'examples': len(exs), 'epochs': args.epochs,
           'baseline': base_res, 'candidate': cand, 'gate': gate,
           'gate_rule': 'L1 35/35 and s2b 0 and s2 0', 'missed': miss}
    (OUT / 'stage0' / 'result.json').write_text(json.dumps(res, indent=2))
    return res, net, exs


# ---------------- round 2: online augmentation, long negatives, per-epoch selection
ROUND2_VARIANTS = [
    {'name': 'order-w5-k1', 'neg_agg': 'frames', 'neg_weight': 5.0, 'freeze': 'none', 'dur_k': 1},
    {'name': 'order-w5-k3', 'neg_agg': 'frames', 'neg_weight': 5.0, 'freeze': 'none', 'dur_k': 3},
    {'name': 'order-w5-k6', 'neg_agg': 'frames', 'neg_weight': 5.0, 'freeze': 'none', 'dur_k': 6},
    {'name': 'order-w5-lastaffine', 'neg_agg': 'frames', 'neg_weight': 5.0,
     'freeze': 'last-affine', 'dur_k': 1},
]


def round2_parts():
    s2 = wav(C / 's2-music-silent.aec16k.wav')
    cut = int(round(.75 * len(s2)))
    return s2[:cut], s2[cut:]


def round2_cfg(args, variant):
    cfg = cfg_defaults(args)
    cfg.update({k: v for k, v in variant.items() if k != 'name'})
    cfg['tag'] = variant['name']
    cfg['epochs'] = args.epochs
    cfg['seed'] = args.seed
    return cfg


def round2_command(cfg):
    keys = ['epochs', 'lr', 'batch', 'delta', 'anchor', 'neg_weight', 'neg_agg', 'freeze',
            'long_neg', 'order_neg', 'dur_k',
            'splice', 'single', 'plus', 'gap', 'tts_pos', 'tts_neg', 'tts_hard', 'music']
    return ('round2 --variant ' + cfg['tag'] + ' ' +
            ' '.join(f'--{k.replace("_", "-")} {cfg[k]}' for k in keys if k in cfg))


def round2_baseline(args, s1groups, s3groups, dev, bg_eval, s2val, log):
    d = ROUND2 / 'baseline'
    f = d / 'result.json'
    if f.exists():
        res = json.loads(f.read_text())
        if 'robust' in res and 'gain' in res:
            return res
    base = load_net(1, BASE[1] / 'net.pt')
    res, gm, mm = eval_net(base, 1, d, s3groups, dev, 'v5-baseline-round2', log,
                           dev_bg=bg_eval, s2val_pcm=s2val, dur_k=1,
                           robust=True, gain_sweep=True, s1groups=s1groups)
    f.write_text(json.dumps(res, indent=2))
    (d / 'missed-groups.json').write_text(json.dumps(
        miss_analysis([g for g in s3groups if not any(r['id'] == g['id'] and r['mac_hit'] for r in gm)],
                      mm['s3']), indent=2))
    return res


def run_round2(args, s1groups, s3groups, tts, s1neg, dev, bg, levels, log, variant):
    cfg = round2_cfg(args, variant)
    k = int(cfg.get('dur_k', 1))
    out = ROUND2 / variant['name']
    out.mkdir(parents=True, exist_ok=True)
    (out / 'epochs').mkdir(parents=True, exist_ok=True)
    s2train, s2val = round2_parts()
    bg_train = {'quiet': bg['quiet'], 'music': s2train}
    bg_eval = bg_train
    _, _, prior, step = r34_controls(1)
    base = load_net(1, BASE[1] / 'net.pt')
    base_res = round2_baseline(args, s1groups, s3groups, dev, bg_eval, s2val, log)
    cache = make_eval_cache(1, s3groups, dev, bg_eval, s2val)

    def build_fn(ep):
        return build_dataset(s1groups + s3groups, tts, s1neg, bg_train, levels,
                             args.seed + ep, cfg)

    def manifest_fn(ep, manifest):
        counts = {}
        for e in manifest:
            counts[e['op']] = counts.get(e['op'], 0) + 1
        payload = {'epoch': ep + 1, 'seed': args.seed + ep, 'counts': counts,
                   'examples': [{'id': e['id'], 'kind': e['kind'], 'op': e['op'],
                                 'parents': e['parents'], 'meta': e.get('meta', {})}
                                for e in manifest]}
        (out / 'epochs' / f'epoch{ep+1:02d}-manifest.json').write_text(json.dumps(payload, indent=2))

    t0 = time.time()
    net, history, best_ep, best_m = train_seq_online(
        base, build_fn, prior, step, args.epochs, cfg, args.seed + 1, log,
        lambda n, ep: eval_cache_metrics(n, cache, prior, step, k), manifest_fn)
    seconds = time.time() - t0
    log(f'  round2 {variant["name"]}: training+selection {seconds:.1f}s, best epoch {best_ep+1}')
    # keep the selected epoch's constructed-sample provenance next to the model
    src = out / 'epochs' / f'epoch{best_ep+1:02d}-manifest.json'
    if src.exists():
        (out / 'manifest-best.json').write_text(src.read_text())
    cand, gm, mm = eval_net(net, 1, out / 'best', s3groups, dev,
                            f'{variant["name"]}-best', log, dev_bg=bg_eval, s2val_pcm=s2val,
                            dur_k=k, robust=True, gain_sweep=True, s1groups=s1groups)
    miss = miss_analysis([g for g in s3groups
                          if not any(r['id'] == g['id'] and r['mac_hit'] for r in gm)], mm['s3'])
    (out / 'missed-groups.json').write_text(json.dumps(miss, indent=2))
    gate = cand['s3_groups'] == 35 and cand['s2b_events'] == 0 and cand['s2_events'] == 0
    expressible = (k == 1)
    res = {'variant': variant, 'cfg': cfg, 'command': round2_command(cfg),
           'seconds': seconds, 'epochs': args.epochs, 'best_epoch': best_ep + 1,
           'history': history, 'baseline': base_res, 'candidate': cand, 'gate': gate,
           'dur_k': k, 'r34_fst_expressible': expressible,
           'packable': bool(gate and expressible),
           'gate_rule': 'L1 35/35 midpoint AND s2b 0 AND s2 0; packable only when dur_k=1',
           'missed': miss}
    (out / 'result.json').write_text(json.dumps(res, indent=2))
    return res


def _fmt_rob(r):
    return (f'{r["forward"]}/{r["n"]}', f'{r["reversed"]}/{r["n"]}',
            f'{r["permuted"]}/{r["n_permuted"]}', f'{r["first_half"]}/{r["n_first_half"]}')


def write_round2_report(results):
    lines = ['# Round 2: real-channel order negatives, duration-k decoding, robustness probes', '',
             '2026-11-03. Mac-only. No SSH, device access, recording, cloud upload or deployment. '
             'All scores are Mac reference margins under r34 L1 controls (FST forward weight 2.25, '
             'min-self-loop 0); they are not ARM or device results.', '',
             '## Method', '',
             '- **Unit fix (important).** `active_region` and `split_point` return 100 fps frame indices, but '
             'the earlier constructed-sample code sliced waveforms with those values directly. That truncated '
             'positives to about 0.5 s and made the positive loss window about one frame wide, so the model '
             'could fit by firing on real-channel energy instead of learning 豆包豆包 order. All waveform '
             'slices now convert frames to samples (`*160`). The pre-fix `frames-w2` run is kept only as a '
             'historical artifact and is not comparable to the tables below.',
             '- Differentiable Viterbi is now min-duration-k (`dviterbi_k`, `viterbi_k_np`); k=1 is the r34 '
             'FST recurrence, k>1 matches `review/dur.py`. The r34 FST cannot express k>1, so k>1 models are '
             'evaluation-only and are not packaged.',
             '- Constructed set rebuilt every epoch with seed+epoch; per-epoch manifests record counts, parents, '
             'and order metadata. `manifest-best.json` is the selected epoch.',
             '- Real-channel order negatives: same s1/s3 speakers and channel as positives, with the four '
             'syllables reversed, permuted (4 fixed permutations), truncated (drop one syllable), one 豆包 '
             'only, or each syllable time-reversed in place. At least as many as the positives each epoch, '
             'then speed-perturbed, level-matched to the s3 RMS distribution, and mixed with s2 music.',
             '- Long negatives: 6-10 s crops from the first 75% of s2 with 1-3 non-keyword utterances inserted. '
             'Negative aggregation `frames` = mean per-frame relu.',
             '- s2 last 25% is validation only: never a training background or crop source. Selection: '
             's2val 0 events first, then most midpoint groups, then fewest dev neg accepts.',
             '- Per-epoch selection uses cached torch forward; the selected epoch is re-exported and re-scored '
             'with the numpy reference forward.', '',
             '## Variant results (known-set fit; all 35 s3 groups participate in training)', '',
             '| variant | k | agg | neg w | freeze | best ep | midpoint | in-window | 34 midpoint | 34 in-window | '
             's2b | s2 | s2val | dev pos | dev hardneg | dev neg | dev homo | s3 events | gate | r34 FST |',
             '|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|']
    for r in results:
        if 'error' in r:
            lines.append(f'| {r["variant"]["name"]} | - | - | - | - | - | - | - | - | - | - | - | - | - | - | - | - | - | ERROR | - |')
            continue
        v = r['variant']
        c = r['candidate']
        d = c['dev']
        k = r.get('dur_k', c.get('dur_k', 1))
        lines.append(f'| {v["name"]} | {k} | {v["neg_agg"]} | {v["neg_weight"]} | {v["freeze"]} | '
                     f'{r["best_epoch"]} | {c["s3_groups"]}/35 | {c["s3_inwindow"]}/35 | '
                     f'{c["s3_groups34"]}/34 | {c["s3_inwindow34"]}/34 | {c["s2b_events"]} | '
                     f'{c["s2_events"]} | {c.get("s2val_events", "-")} | '
                     f'{d["pos"]["accepted"]}/{d["pos"]["n"]} | {d["hardneg"]["accepted"]}/{d["hardneg"]["n"]} | '
                     f'{d["neg"]["accepted"]}/{d["neg"]["n"]} | {d["homophone"]["accepted"]}/{d["homophone"]["n"]} | '
                     f'{c["s3_events"]} | {"PASS" if r["gate"] else "FAIL"} | '
                     f'{"yes" if r.get("r34_fst_expressible") else "NO (k>1)"} |')
    if results and 'baseline' in results[0]:
        b = results[0]['baseline']
        d = b['dev']
        lines += ['', f'v5 baseline under the same round2 eval: midpoint {b["s3_groups"]}/35, '
                  f'in-window {b["s3_inwindow"]}/35, 34 midpoint {b["s3_groups34"]}/34, '
                  f'34 in-window {b["s3_inwindow34"]}/34, s2b {b["s2b_events"]}, '
                  f's2 {b["s2_events"]}, s2val {b.get("s2val_events", "-")}, '
                  f'dev pos {d["pos"]["accepted"]}/{d["pos"]["n"]}, '
                  f'hardneg {d["hardneg"]["accepted"]}/{d["hardneg"]["n"]}, '
                  f'neg {d["neg"]["accepted"]}/{d["neg"]["n"]}, '
                  f'homophone {d["homophone"]["accepted"]}/{d["homophone"]["n"]}, '
                  f's3 events {b["s3_events"]}.', '']
        if 'robust' in b:
            rb = b['robust']
            lines += ['v5 isolated-clip robustness (4000-sample zero padding, same convention as '
                      '`review/reverse.json`):', '',
                      '| set | forward | reversed | permuted | first half |', '|---|---|---|---|---|']
            for name in ('s1', 's3'):
                rr = rb[name]
                lines.append(f'| {name} | ' + ' | '.join(_fmt_rob(rr)) + ' |')
            lines.append('')
        if 'gain' in b:
            lines += ['v5 gain sweep (MFCC c0 offset):', '',
                      '| gain dB | s3 midpoint | s3 in-window | s2b | s2 | dev pos | dev hardneg | dev neg | dev homo |',
                      '|---|---|---|---|---|---|---|---|---|']
            for g in sorted(b['gain'], key=lambda x: int(x)):
                q = b['gain'][g]
                dd = q['dev']
                lines.append(f'| {g} | {q["s3_groups"]}/35 | {q["s3_inwindow"]}/35 | {q["s2b_events"]} | '
                             f'{q["s2_events"]} | {dd["pos"]["accepted"]}/{dd["pos"]["n"]} | '
                             f'{dd["hardneg"]["accepted"]}/{dd["hardneg"]["n"]} | '
                             f'{dd["neg"]["accepted"]}/{dd["neg"]["n"]} | '
                             f'{dd["homophone"]["accepted"]}/{dd["homophone"]["n"]} |')
            lines.append('')
    lines += ['## Order-shortcut probes (reversed / permuted / first half)', '',
              'Isolated real clips with 4000 samples of zero padding; the first-half cut is the first 豆包 '
              'up to the energy valley, and permuted is syllable order [2,3,0,1].', '',
              '| variant | s1 forward | s1 reversed | s1 permuted | s1 first half | '
              's3 forward | s3 reversed | s3 permuted | s3 first half |',
              '|---|---|---|---|---|---|---|---|---|']
    for r in results:
        if 'error' in r or 'robust' not in r.get('candidate', {}):
            continue
        rb = r['candidate']['robust']
        lines.append(f'| {r["variant"]["name"]} | ' + ' | '.join(_fmt_rob(rb['s1'])) + ' | ' +
                     ' | '.join(_fmt_rob(rb['s3'])) + ' |')
    lines.append('')
    lines += ['## Gain sweep (MFCC c0 offset; -10 / 0 / +12 dB)', '',
              '| variant | gain dB | s3 midpoint | s3 in-window | s2b | s2 | dev pos | dev hardneg | dev neg | dev homo |',
              '|---|---|---|---|---|---|---|---|---|---|']
    for r in results:
        if 'error' in r or 'gain' not in r.get('candidate', {}):
            continue
        for g in sorted(r['candidate']['gain'], key=lambda x: int(x)):
            q = r['candidate']['gain'][g]
            dd = q['dev']
            lines.append(f'| {r["variant"]["name"]} | {g} | {q["s3_groups"]}/35 | {q["s3_inwindow"]}/35 | '
                         f'{q["s2b_events"]} | {q["s2_events"]} | {dd["pos"]["accepted"]}/{dd["pos"]["n"]} | '
                         f'{dd["hardneg"]["accepted"]}/{dd["hardneg"]["n"]} | '
                         f'{dd["neg"]["accepted"]}/{dd["neg"]["n"]} | '
                         f'{dd["homophone"]["accepted"]}/{dd["homophone"]["n"]} |')
    lines.append('')
    lines += ['## Per-epoch selection curves', '',
              'Each row is the cached-torch evaluation after that epoch; selection uses these values.', '']
    for r in results:
        if 'error' in r or not r.get('history'):
            continue
        lines += [f'### {r["variant"]["name"]} (k={r.get("dur_k", 1)})', '',
                  '| epoch | midpoint | in-window | 34 midpoint | s2b | s2 | s2val | dev pos | dev hardneg | dev neg | dev homo |',
                  '|---|---|---|---|---|---|---|---|---|---|---|']
        for i, m in enumerate(r['history']):
            d = m['dev']
            lines.append(f'| {i+1} | {m["s3_groups"]}/35 | {m["s3_inwindow"]}/35 | {m["s3_groups34"]}/34 | '
                         f'{m["s2b_events"]} | {m["s2_events"]} | {m["s2val_events"]} | '
                         f'{d["pos"]["accepted"]}/{d["pos"]["n"]} | {d["hardneg"]["accepted"]}/{d["hardneg"]["n"]} | '
                         f'{d["neg"]["accepted"]}/{d["neg"]["n"]} | '
                         f'{d["homophone"]["accepted"]}/{d["homophone"]["n"]} |')
        lines.append('')
    # Pareto front: maximize midpoint groups, minimize s2b, s2, dev neg.
    ok = [r for r in results if 'error' not in r]
    def vals(r):
        c = r['candidate']
        return c['s3_groups'], c['s2b_events'], c['s2_events'], c['dev']['neg']['accepted']
    pareto = []
    for r in ok:
        a = vals(r)
        if not any(all(b[i] >= a[i] for i in (0,)) and all(b[i] <= a[i] for i in (1, 2, 3))
                   and (b[0] > a[0] or b[1] < a[1] or b[2] < a[2] or b[3] < a[3])
                   for b in (vals(q) for q in ok if q is not r)):
            pareto.append(r)
    lines += ['## Pareto front (groups up; s2b, s2, dev neg down)', '',
              '| variant | k | midpoint | s2b | s2 | dev neg | gate | r34 FST |',
              '|---|---|---|---|---|---|---|---|']
    for r in sorted(pareto, key=lambda x: (-vals(x)[0], vals(x)[1], vals(x)[2], vals(x)[3])):
        a = vals(r)
        lines.append(f'| {r["variant"]["name"]} | {r.get("dur_k", 1)} | {a[0]}/35 | {a[1]} | {a[2]} | {a[3]} | '
                     f'{"PASS" if r["gate"] else "FAIL"} | '
                     f'{"yes" if r.get("r34_fst_expressible") else "NO (k>1)"} |')
    lines.append('')
    if ok:
        packable = [r for r in ok if r.get('packable')]
        if packable:
            best = sorted(packable, key=lambda r: (-vals(r)[0], vals(r)[1], vals(r)[2], vals(r)[3]))[0]
        else:
            best = sorted(ok, key=lambda r: (0 if r['gate'] else 1, -vals(r)[0], vals(r)[1],
                                             vals(r)[2], vals(r)[3]))[0]
        c = best['candidate']
        lines += ['## Best candidate', '',
                  f'- variant: `{best["variant"]["name"]}`; k={best.get("dur_k", 1)}; '
                  f'command: `{best["command"]}`',
                  f'- gate: {"PASS" if best["gate"] else "FAIL"}; r34 FST expressible: '
                  f'{best.get("r34_fst_expressible")}; packable: {best.get("packable")}',
                  f'- container reference SHA256: `{c["sha256"]}`; bytes: {c["bytes"]}; '
                  f'export double-precision max error: {c["export_maxerr"]:.2e}; '
                  f'non-AM bytes equal r34: {c["non_am_equal_r34"]}; container written: '
                  f'{c.get("container_written")}',
                  f'- path: `{ROUND2 / best["variant"]["name"] / "best"}`', '']
        if best.get('missed'):
            lines += ['Missed groups (midpoint metric):', '',
                      '| group | window s | min margin | clip RMS dBFS |', '|---|---|---|---|']
            for m in best['missed']:
                lines.append(f'| {m["id"]} | {m["start_s"]:.2f}-{m["end_s"]:.2f} | '
                             f'{m["min_margin"]:.2f} | {m["clip_rms_dbfs"]:.1f} |')
            lines.append('')
        mf = ROUND2 / best['variant']['name'] / 'best' / 'matrix.json'
        if mf.exists():
            mat = json.loads(mf.read_text())
            lines += ['## 35-group matrix for the best variant', '',
                      '| group | window s | Mac hit | min margin |', '|---|---|---|---|']
            for m in mat:
                lines.append(f'| {m["id"]} | {m["start_s"]:.2f}-{m["end_s"]:.2f} | '
                             f'{"yes" if m["mac_hit"] else "no"} | {m["min_margin"]:.2f} |')
            lines.append('')
        if not any(r['gate'] for r in ok):
            lines += ['No variant passed the unchanged gate (35/35 midpoint AND s2b 0 AND s2 0). '
                      'Stage 1 CV, stage 2 and final packaging were not run.', '']
        elif not packable:
            lines += ['The gate was reached only with k>1, which the r34 FST cannot express. '
                      'No BINN was packed and no device result is claimed. Stage 1 CV and stage 2 '
                      'were not run.', '']
        else:
            lines += ['At least one k=1 variant passed the gate; see `cv/` and `stage2/` outputs if present.', '']
    lines += ['## Limits', '',
              '- Known-set fitting only unless a CV section exists; no CV number is implied by this table.',
              '- s2b is used only as the independent music veto metric, never in gradients or selection.',
              '- s2 first 75% is the only training background/crop source; the last 25% is validation only.',
              '- k>1 uses a decoder the r34 FST cannot express; those rows are diagnostics, not candidates.',
              '- No model was promoted or deployed.']
    (ROUND2 / 'REPORT.md').write_text('\n'.join(lines) + '\n')


def run_round2_sweep(args, s1groups, s3groups, tts, s1neg, dev, bg, levels, log):
    wanted = set(args.variants.split(',')) if args.variants else None
    existing = {}
    sf = ROUND2 / 'summary.json'
    if sf.exists():
        for r in json.loads(sf.read_text()):
            existing[r['variant']['name']] = r
    for v in ROUND2_VARIANTS:
        if wanted and v['name'] not in wanted:
            continue
        log(f'round2 variant {v["name"]}: agg={v["neg_agg"]} neg_w={v["neg_weight"]} freeze={v["freeze"]}')
        try:
            r = run_round2(args, s1groups, s3groups, tts, s1neg, dev, bg, levels, log, v)
        except Exception as exc:
            import traceback
            traceback.print_exc()
            r = {'variant': v, 'error': repr(exc)}
            log(f'variant {v["name"]} FAILED: {exc!r}')
        existing[v['name']] = r
        results = [existing[q['name']] for q in ROUND2_VARIANTS if q['name'] in existing]
        sf.write_text(json.dumps(results, indent=2))
        write_round2_report(results)
    return results


def fold_ids(s3groups):
    return [set(g['id'] for g in s3groups[k * 7:(k + 1) * 7]) for k in range(5)]


def run_cv(args, s1groups, s3groups, tts, s1neg, dev, bg, levels, log):
    cfg = cfg_defaults(args)
    parts1, fst1, prior1, step1 = r34_controls(1)
    parts2, fst2, prior2, step2 = r34_controls(2)
    folds = []
    total_l1 = total_cas = total_s2b = total_s2 = 0
    for k, test_ids in enumerate(fold_ids(s3groups)):
        cfg['tag'] = f'fold{k}'
        allowed = s1groups + [g for g in s3groups if g['id'] not in test_ids]
        exs, manifest = build_dataset(allowed, tts, s1neg, bg, levels, args.seed + 100 + k, cfg)
        d = OUT / 'cv' / f'fold{k}'
        d.mkdir(parents=True, exist_ok=True)
        (d / 'manifest.json').write_text(json.dumps(manifest, indent=2))
        base1 = load_net(1, BASE[1] / 'net.pt')
        net1 = train_seq(base1, exs, prior1, step1, args.epochs, cfg, args.seed + 200 + k, log)
        r1, gm1, mm1 = eval_net(net1, 1, d / 'l1', s3groups, dev, f'fold{k}-l1', log,
                                dur_k=cfg['dur_k'])
        base2 = load_net(2, BASE[2] / 'net.pt')
        net2 = train_seq(base2, exs, prior2, step2, args.epochs, cfg, args.seed + 300 + k, log)
        r2, gm2, mm2 = eval_net(net2, 2, d / 'l2', s3groups, dev, f'fold{k}-l2', log,
                                dur_k=cfg['dur_k'])
        # count only the held-out seven groups
        test_groups = [g for g in s3groups if g['id'] in test_ids]
        l1_hits = sum(r['mac_hit'] for r in gm1 if r['id'] in test_ids)
        cas, cgm = eval_cascade(d / 'l1', d / 'l2', s3groups, dev, f'fold{k}-cascade', log)
        cas_hits = sum(r['mac_hit'] for r in cgm if r['id'] in test_ids)
        # restrict s2b/s2 to this fold's model (already in r1/r2/cas)
        folds.append({'fold': k, 'test_ids': [g['id'] for g in test_groups],
                      'l1': {'s3_groups': l1_hits, 's2b_events': r1['s2b_events'], 's2_events': r1['s2_events']},
                      'l2': {'s3_groups': sum(r['mac_hit'] for r in gm2 if r['id'] in test_ids),
                             's2b_events': r2['s2b_events'], 's2_events': r2['s2_events']},
                      'cascade': {'s3_groups': cas_hits, 's2b_events': cas['s2b_events'],
                                  's2_events': cas['s2_events']}})
        total_l1 += l1_hits
        total_cas += cas_hits
        total_s2b += cas['s2b_events']
        total_s2 += cas['s2_events']
        log(f'  fold{k} test {sorted(test_ids)} L1 {l1_hits}/7 cascade {cas_hits}/7')
    res = {'folds': folds, 'total_hits': total_l1, 's2b_events': sum(f['l1']['s2b_events'] for f in folds),
           's2_events': sum(f['l1']['s2_events'] for f in folds),
           'cascade_total_hits': total_cas, 'cascade_s2b_events': total_s2b, 'cascade_s2_events': total_s2}
    (OUT / 'cv' / 'result.json').write_text(json.dumps(res, indent=2))
    return res


def run_stage2(args, s1groups, s3groups, tts, s1neg, dev, bg, levels, log):
    cfg = cfg_defaults(args)
    cfg['tag'] = 'stage2'
    exs, manifest = build_dataset(s1groups + s3groups, tts, s1neg, bg, levels, args.seed, cfg)
    (OUT / 'stage2').mkdir(parents=True, exist_ok=True)
    (OUT / 'stage2' / 'manifest.json').write_text(json.dumps(manifest, indent=2))
    parts, fst, prior, step = r34_controls(2)
    base = load_net(2, BASE[2] / 'net.pt')
    t0 = time.time()
    net = train_seq(base, exs, prior, step, args.epochs, cfg, args.seed + 1, log)
    seconds = time.time() - t0
    log(f'  stage2 training {seconds:.1f}s')
    base_res, _, _ = eval_net(base, 2, OUT / 'stage2' / 'baseline-l2', s3groups, dev, 'l2-baseline', log,
                              dur_k=cfg['dur_k'])
    l2, gm2, mm2 = eval_net(net, 2, OUT / 'stage2' / 'l2', s3groups, dev, 'seq-l2', log,
                            dur_k=cfg['dur_k'])
    cas, cgm = eval_cascade(OUT / 'stage0' / 'l1', OUT / 'stage2' / 'l2', s3groups, dev, 'seq-cascade', log)
    res = {'seconds': seconds, 'examples': len(exs), 'epochs': args.epochs,
           'baseline': base_res, 'l2': l2, 'cascade': cas}
    (OUT / 'stage2' / 'result.json').write_text(json.dumps(res, indent=2))
    return res, net


def run_final(args, log):
    res = {}
    for level, src in ((1, OUT / 'stage0' / 'l1'), (2, OUT / 'stage2' / 'l2')):
        net = load_net(level, src / 'net.pt')
        probe = mfcc16(wav(C / 's3-music-pos.aec16k.wav'))[:150]
        info, parsed, am, fst, prior = save_model(net, level, OUT / 'final' / f'l{level}', probe)
        # byte-level checks against the reviewed r34 container
        parts, _, _, _ = r34_controls(level)
        blob = (OUT / 'final' / f'l{level}' / 'model.binn').read_bytes()
        top, end = P.load(blob)
        np2 = {k: v for d in top['list'] for k, v in d.items()}
        assert end == len(blob) and P.dump(top) == blob
        assert all(np2[k] == parts[k] for k in ('conf', 'model', 'fst', 'prior'))
        # torch vs numpy forward parity is already checked in save_model (double precision)
        err = info['export_maxerr']
        (OUT / 'final' / f'l{level}' / 'export.json').write_text(json.dumps(info, indent=2))
        res[f'L{level}'] = info
        log(f'  final L{level}: {info["sha256"]} {info["bytes"]} bytes export {err:.2e}')
    (OUT / 'final' / 'result.json').write_text(json.dumps(res, indent=2))
    return res


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('cmd', choices=['unit-test', 'stage0', 'stage1', 'stage2', 'cv', 'final', 'all',
                                    'round2', 'round2-sweep', 'round2-report'])
    ap.add_argument('--epochs', type=int, default=3)
    ap.add_argument('--lr', type=float, default=1e-4)
    ap.add_argument('--batch', type=int, default=8)
    ap.add_argument('--delta', type=float, default=2.0)
    ap.add_argument('--anchor', type=float, default=1e-3)
    ap.add_argument('--pos-weight', type=float, default=1.0)
    ap.add_argument('--neg-weight', type=float, default=1.0)
    ap.add_argument('--neg-agg', choices=['min', 'frames', 'both'], default='min',
                    help='negative aggregation: worst frame (min, old), mean frame (frames), or both')
    ap.add_argument('--freeze', choices=['none', 'last-affine', 'last-two'], default='none',
                    help='none (old), last-affine (only output layer), last-two (last conv + output)')
    ap.add_argument('--long-neg', type=int, default=0,
                    help='round2 long music+speech negatives per epoch (0 keeps old behavior)')
    ap.add_argument('--order-neg', type=int, default=0,
                    help='real-channel order negatives per epoch; 0 disables, otherwise '
                         'max(value, positive count) are generated')
    ap.add_argument('--dur-k', type=int, default=1, choices=[1, 2, 3, 4, 6],
                    help='minimum frames per keyword pdf in the differentiable decoder '
                         '(1 = r34 FST; k>1 is evaluation-only because the r34 FST cannot express it)')
    ap.add_argument('--kl-weight', type=float, default=0.0,
                    help='KL(base posteriors || candidate posteriors) on training frames')
    ap.add_argument('--seed', type=int, default=4242)
    ap.add_argument('--splice', type=int, default=80)
    ap.add_argument('--single', type=int, default=80)
    ap.add_argument('--plus', type=int, default=50)
    ap.add_argument('--gap', type=int, default=50)
    ap.add_argument('--tts-pos', type=int, default=100)
    ap.add_argument('--tts-neg', type=int, default=100)
    ap.add_argument('--tts-hard', type=int, default=150)
    ap.add_argument('--music', type=int, default=80)
    ap.add_argument('--variant', default=None, help='round2 single-run variant name')
    ap.add_argument('--variants', default=None, help='round2-sweep comma-separated variant names')
    a = ap.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    logf = open(OUT / f'{a.cmd}.log', 'a')
    log = lambda s: (print(s, flush=True), logf.write(s + '\n'), logf.flush())
    log(f'config {vars(a)}')
    if a.cmd == 'unit-test':
        unit_test()
        return
    if a.cmd == 'round2-report':
        results = json.loads((ROUND2 / 'summary.json').read_text())
        write_round2_report(results)
        log('round2 report written')
        return
    s1groups, s3groups = build_groups()
    tts, s1neg, dev, bg = base_sources()
    levels = [float(x['clip_rms_dbfs']) for x in json.loads(S3_LEVELS.read_text())]
    if a.cmd == 'round2':
        name = a.variant or f'{a.neg_agg}-w{a.neg_weight}-{a.freeze}'
        v = {'name': name, 'neg_agg': a.neg_agg, 'neg_weight': a.neg_weight, 'freeze': a.freeze}
        run_round2(a, s1groups, s3groups, tts, s1neg, dev, bg, levels, log, v)
        return
    if a.cmd == 'round2-sweep':
        run_round2_sweep(a, s1groups, s3groups, tts, s1neg, dev, bg, levels, log)
        return
    result = {}
    if a.cmd in ('stage0', 'all'):
        log('stage0: L1 full-data sequence fine-tune')
        s0, l1net, exs = run_stage0(a, s1groups, s3groups, tts, s1neg, dev, bg, levels, log)
        result['stage0'] = s0
        (OUT / 'result.json').write_text(json.dumps(result, indent=2))
        if not s0['gate']:
            log('stage0 gate FAILED: stop before CV/final')
            write_report(result, OUT / 'REPORT.md')
            return
        if a.cmd == 'all':
            log('stage0 gate PASSED')
    if a.cmd in ('stage1', 'cv', 'all'):
        log('CV: L1 and L2 5-fold')
        cv = run_cv(a, s1groups, s3groups, tts, s1neg, dev, bg, levels, log)
        result['stage1'] = {'folds': [{'fold': f['fold'], 'test_ids': f['test_ids'], 'l1': f['l1']}
                                      for f in cv['folds']],
                            'total_hits': cv['total_hits'], 's2b_events': cv['s2b_events'],
                            's2_events': cv['s2_events']}
        result['cv'] = {'folds': [{'fold': f['fold'], 'cascade': f['cascade']} for f in cv['folds']],
                        'total_hits': cv['cascade_total_hits'], 's2b_events': cv['cascade_s2b_events'],
                        's2_events': cv['cascade_s2_events']}
        (OUT / 'result.json').write_text(json.dumps(result, indent=2))
    if a.cmd in ('stage2', 'all'):
        log('stage2: L2 full-data sequence fine-tune + cascade')
        s2, l2net = run_stage2(a, s1groups, s3groups, tts, s1neg, dev, bg, levels, log)
        result['stage2'] = s2
        (OUT / 'result.json').write_text(json.dumps(result, indent=2))
    if a.cmd in ('final', 'all'):
        log('final: package full-data L1/L2')
        result['final'] = run_final(a, log)
        (OUT / 'result.json').write_text(json.dumps(result, indent=2))
    write_report(result, OUT / 'REPORT.md')
    log('report written')


if __name__ == '__main__':
    main()
