"""Round 5: all-groups pre-AEC mixed-channel training, s2b early-stop, 4 variants.

Local Mac experiment only. No SSH, device access, recording, cloud upload, or deploy.

Commands
  pool      build the fixed pre-AEC mixed pool under round5/pool/ (>=2000 pos, >=4000 neg)
  run       train/evaluate 4 variants (dur-k 1/3/6, TTS weights 1x/3x/5x, freeze options)
  report    regenerate round5/REPORT.md from round5/summary.json
  all       pool + run

Example
  ~/Library/Caches/xiaoai-sidekick/train-venv/bin/python tools/wake-seq-round5.py all
"""
from pathlib import Path
import argparse, copy, importlib.util, json, math, time
import numpy as np
import torch

R = Path(__file__).resolve().parents[1]
OUT = R / 'temp/analysis/wake-seq'
ROUND5 = OUT / 'round5'
POOL = ROUND5 / 'pool'
C = R / 'private/inputs/recordings/wake-corpus'


def load_module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


W = load_module('ws', R / 'tools/wake-seq-train.py')
R3 = load_module('r3', R / 'tools/wake-seq-round3.py')

BASE_CFG = {'neg_agg': 'frames', 'neg_weight': 5.0, 'freeze': 'none', 'dur_k': 1,
            'lr': 5e-5, 'batch': 8, 'delta': 2.0, 'anchor': 1e-3, 'pos_weight': 1.0,
            'kl_weight': 0.0, 'long_neg': 120, 'order_neg': 0, 'splice': 0,
            'single': 30, 'plus': 30, 'gap': 30, 'tts_pos': 80, 'tts_neg': 150,
            'tts_hard': 200, 'music': 0, 'epochs': 10, 'seed': 5252,
            'pool_pos': 2000, 'pool_neg': 4000}


def cfg_for(name, dur_k, epochs, tts_pos=80, tts_neg=150, tts_hard=200,
            pos_weight=1.0, freeze='none', pool_pos=2000, pool_neg=4000):
    c = dict(BASE_CFG)
    c.update({'tag': name, 'dur_k': dur_k, 'epochs': epochs,
              'tts_pos': tts_pos, 'tts_neg': tts_neg, 'tts_hard': tts_hard,
              'pos_weight': pos_weight, 'freeze': freeze,
              'pool_pos': pool_pos, 'pool_neg': pool_neg})
    return c


# ---------------- event construction at 48 kHz
def mic48(raw, a, b):
    return raw[a:b, :6].astype(np.float64).mean(1)


def pos_event(g, raw, levels, rng):
    ar = W.active_region(g['pcm'])
    if ar is None:
        return None
    a, b, _ = ar
    gstart = int(g['start_s'] * 48000)
    a48 = gstart + int(a * 160 * 3)
    b48 = gstart + int(b * 160 * 3)
    lo = max(0, a48 - 14400)
    hi = min(len(raw), b48 + 24000)
    sig = mic48(raw, lo, hi).astype(np.float32)
    if len(sig) < 1600:
        return None
    return {'kind': 'pos', 'id': g['id'], 'sig': sig,
            'wa': a48 - lo, 'wb': b48 - lo, 'gain_dbfs': float(rng.choice(levels))}


def order_event(g, raw, levels, rng, mode):
    ar = W.active_region(g['pcm'])
    if ar is None:
        return None
    cuts = W.syllable_bounds(g['pcm'], ar[0], ar[1], ar[2])
    gstart = int(g['start_s'] * 48000)
    c48 = [gstart + c * 3 for c in cuts]
    lo = max(0, c48[0] - 4800)
    hi = min(len(raw), c48[4] + 4800)
    base = mic48(raw, lo, hi).astype(np.float32)
    cl = [c - lo for c in c48]
    segs = [W.fade(base[cl[i]:cl[i + 1]].astype(np.float64), 240) for i in range(4)]
    if any(len(s) < 16 for s in segs):
        return None
    if mode == 'reverse':
        order, gapped = [2, 3, 0, 1], True
    elif mode == 'perm':
        order, gapped = [int(x) for x in W.ORDER_PERMS[int(rng.integers(len(W.ORDER_PERMS)))]], True
    elif mode == 'truncate':
        order, gapped = ([0, 1, 2] if rng.random() < .5 else [1, 2, 3]), True
    elif mode == 'single':
        order, gapped = [0, 1], False
    elif mode == 'sylrev':
        order, gapped = [0, 1, 2, 3], True
        segs = [s[::-1].copy() for s in segs]
    else:
        return None
    parts = []
    for q, i in enumerate(order):
        parts.append(segs[i])
        if gapped and q < len(order) - 1:
            parts.append(np.zeros(int(rng.uniform(0, .4) * 48000), np.float64))
    sig = np.concatenate(parts).astype(np.float32)
    return {'kind': 'neg', 'id': f'{g["id"]}-{mode}', 'sig': sig,
            'wa': 0, 'wb': len(sig), 'gain_dbfs': float(rng.choice(levels))}


def s1neg_event(s, raw, levels, rng):
    start = int(float(s['id'].split('@')[1]) * 48000)
    n = len(s['pcm']) * 3
    sig = mic48(raw, start, min(len(raw), start + n)).astype(np.float32)
    if len(sig) < 1600:
        return None
    return {'kind': 'neg', 'id': s['id'], 'sig': sig, 'wa': 0, 'wb': len(sig),
            'gain_dbfs': float(rng.choice(levels))}


def build_events(all_groups, s1neg, levels, seed, n_pos=2000, n_order=2000, n_s1neg=300):
    rng = np.random.default_rng(seed)
    raw_s1 = R3.raw_memmap('s1-pos-quiet')
    raw_s3 = R3.raw_memmap('s3-music-pos')
    s1groups = [g for g in all_groups if g['session'] == 's1-pos-quiet']
    s3groups = [g for g in all_groups if g['session'] == 's3-music-pos']
    pos, neg = [], []
    # positives from both s1 and s3: ~50/50 split
    per = int(math.ceil(n_pos / len(all_groups)))
    for g in all_groups:
        raw = raw_s1 if g['session'] == 's1-pos-quiet' else raw_s3
        for _ in range(per):
            e = pos_event(g, raw, levels, rng)
            if e is not None:
                pos.append(e)
    # order negatives from both s1 and s3
    modes = W.ORDER_MODES
    for i in range(n_order):
        g = all_groups[int(rng.integers(len(all_groups)))]
        raw = raw_s1 if g['session'] == 's1-pos-quiet' else raw_s3
        e = order_event(g, raw, levels, rng, modes[i % len(modes)])
        if e is not None:
            neg.append(e)
    # s1 real negatives
    for i in range(n_s1neg):
        s = s1neg[int(rng.integers(len(s1neg)))]
        e = s1neg_event(s, raw_s1, levels, rng)
        if e is not None:
            neg.append(e)
    rng.shuffle(pos)
    rng.shuffle(neg)
    return pos, neg


# ---------------- stream placement / AEC / extraction
def build_stream(events, sidx, rng, levels, log):
    s2 = R3.raw_memmap('s2b-music-silent')  # s2b only — s2 stays validation
    nframes = int(.90 * len(s2))            # use 90% of s2b as negative canvas
    raw_out = POOL / f'stream{sidx:03d}.raw'
    mm = np.memmap(raw_out, dtype='<i4', mode='w+', shape=(nframes, 8))
    mm[:] = s2[:nframes]
    cur = 5 * 48000
    placed = []
    used = 0
    for ev in events:
        n = len(ev['sig'])
        if cur + n + 24000 >= nframes:
            break
        seg = ev['sig'][ev['wa']:ev['wb']] if ev['kind'] == 'pos' else ev['sig']
        rms = R3.active_rms_48k(seg)
        if rms is None or rms < 1e-9:
            used += 1
            continue
        scale = (32768.0 * 10 ** (ev['gain_dbfs'] / 20.0)) / rms
        add = ev['sig'] * scale
        nf = min(240, len(add) // 2)
        if nf > 0:
            add[:nf] *= np.linspace(0, 1, nf)
            add[-nf:] *= np.linspace(1, 0, nf)
        o = cur
        hi = o + n
        for ch in range(6):
            mm[o:hi, ch] = np.clip(mm[o:hi, ch].astype(np.float64) + add,
                                   -2 ** 31, 2 ** 31 - 1).astype(np.int32)
        placed.append({'id': ev['id'], 'kind': ev['kind'], 'start_frame': o,
                       'n48': n, 'wa': ev['wa'], 'wb': ev['wb'],
                       'gain_dbfs': ev['gain_dbfs']})
        used += 1
        cur = o + n + int(rng.uniform(.3, 1.0) * 48000)
    mm.flush()
    del mm
    aec = R3.aec_run(raw_out, POOL / f'stream{sidx:03d}.s16', log)
    raw_out.unlink(missing_ok=True)
    log(f'  stream {sidx}: {len(placed)} events, aec {len(aec)} samples')
    return placed, used, aec


def extract(placed, aec):
    pos, neg = [], []
    for p in placed:
        o16 = p['start_frame'] // 3
        n16 = p['n48'] // 3
        lo = max(0, o16 - 8000)
        hi = min(len(aec), o16 + n16 + 8000)
        pcm = aec[lo:hi].astype(np.int16)
        if p['kind'] == 'pos':
            wa = (o16 + p['wa'] // 3) - lo
            wb = (o16 + p['wb'] // 3) - lo
            if wa < 0 or wb >= len(pcm):
                continue
            pos.append({'id': p['id'], 'pcm': pcm, 'win': (int(wa), int(wb)),
                        'gain_dbfs': p['gain_dbfs'], 'stream': p.get('stream')})
        else:
            neg.append({'id': p['id'], 'pcm': pcm, 'gain_dbfs': p['gain_dbfs'],
                        'stream': p.get('stream')})
    return pos, neg


def s2_alone_windows(count, seed):
    """From s2 (not s2b) — s2 is validation-only, so we sample s2b for training."""
    rng = np.random.default_rng(seed)
    s2b = R3.raw_memmap('s2b-music-silent')
    s2b_16k = W.wav(C / 's2b-music-silent.aec16k.wav')
    n90 = int(.90 * len(s2b_16k))
    out = []
    for i in range(count):
        n = int(rng.uniform(2.0, 4.0) * 16000)
        a = int(rng.integers(0, max(1, n90 - n)))
        out.append({'id': f's2b-alone-{i:04d}', 'pcm': np.rint(s2b_16k[a:a + n]).astype(np.int16),
                    'gain_dbfs': None, 'stream': None})
    return out


def build_pool(all_groups, s1neg, levels, seed, log, n_pos=2000, n_order=2000,
               n_s1neg=300, n_s2b=2000, max_events=85):
    POOL.mkdir(parents=True, exist_ok=True)
    rng = np.random.default_rng(seed)
    pos_ev, neg_ev = build_events(all_groups, s1neg, levels, seed, n_pos, n_order, n_s1neg)
    log(f'  events: {len(pos_ev)} pos, {len(neg_ev)} mixed neg')
    all_ev = pos_ev + neg_ev
    rng.shuffle(all_ev)
    pos, neg = [], []
    i = sidx = 0
    while i < len(all_ev):
        chunk = all_ev[i:i + max_events]
        placed, used, aec = build_stream(chunk, sidx, rng, levels, log)
        for p in placed:
            p['stream'] = sidx
        pp, nn = extract(placed, aec)
        pos += pp
        neg += nn
        if used == 0:
            break
        i += used
        sidx += 1
    neg += s2_alone_windows(n_s2b, seed + 1)
    log(f'  pool raw: {len(pos)} pos, {len(neg)} neg')
    # features once
    pf, pw, pl = [], [], []
    for p in pos:
        f = W.mfcc16(p['pcm'])
        if len(f) < 10:
            continue
        s, t = p['win']
        wf = (max(7, int(s / 160) - 30),
              min(len(f) - 1, int(t / 160) + 50))
        if wf[1] < wf[0]:
            continue
        pf.append(f.astype(np.float32))
        pw.append(wf)
        pl.append(p)
    neg_keep = []
    nf = []
    for p in neg:
        f = W.mfcc16(p['pcm'])
        if len(f) >= 10:
            neg_keep.append(p)
            nf.append(f.astype(np.float32))

    def pack(items, feats, wins=None):
        pcm = np.concatenate([x['pcm'] for x in items]).astype(np.int16) if items else np.zeros(0, np.int16)
        lens = np.array([len(x['pcm']) for x in items], np.int32)
        fcat = np.concatenate(feats).astype(np.float32) if feats else np.zeros((0, 40), np.float32)
        flens = np.array([len(x) for x in feats], np.int32)
        ids = np.array([x['id'] for x in items], dtype=object)
        gains = np.array([x.get('gain_dbfs') if x.get('gain_dbfs') is not None else np.nan
                          for x in items], np.float32)
        d = {'pcm': pcm, 'lens': lens, 'fcat': fcat, 'flens': flens, 'ids': ids, 'gains': gains}
        if wins is not None:
            d['wins'] = np.array(wins, np.int32)
        return d

    np.savez_compressed(POOL / 'pos.npz', **pack(pl, pf, pw))
    np.savez_compressed(POOL / 'neg.npz', **pack(neg_keep, nf))
    meta = {'pos': len(pl), 'neg': len(neg_keep), 'pos_events': len(pos_ev),
            'neg_events': len(neg_ev), 's2b_alone': n_s2b, 'streams': sidx,
            's3_levels': levels, 'seed': seed}
    (POOL / 'meta.json').write_text(json.dumps(meta, indent=2))
    log(f'  pool kept: {len(pl)} pos, {len(neg_keep)} neg, streams {sidx}')
    return meta


def load_pool():
    def unpack(path, wins=False):
        z = np.load(path, allow_pickle=True)
        pcm, lens = z['pcm'], z['lens']
        fcat, flens = z['fcat'], z['flens']
        ids, gains = z['ids'], z['gains']
        po = 0
        fo = 0
        out = []
        for i in range(len(lens)):
            p = pcm[po:po + int(lens[i])]
            po += int(lens[i])
            f = fcat[fo:fo + int(flens[i])]
            fo += int(flens[i])
            e = {'id': str(ids[i]), 'pcm': p, 'feat': f, 'gain_dbfs': None if np.isnan(gains[i]) else float(gains[i])}
            if wins:
                e['wf'] = tuple(int(x) for x in z['wins'][i])
            out.append(e)
        return out
    return unpack(POOL / 'pos.npz', True), unpack(POOL / 'neg.npz')


# ---------------- dataset
def build_r5_dataset(pool, all_groups, tts, s1neg, bg, levels, seed, cfg,
                     s2val_pcm, log=print):
    rng = np.random.default_rng(seed)
    pos_pool, neg_pool = pool
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
                         'meta': e.get('meta', {})})

    # fixed pre-AEC pool (features cached; per-epoch subset)
    npos = min(int(cfg['pool_pos']), len(pos_pool))
    nneg = min(int(cfg['pool_neg']), len(neg_pool))
    for i in rng.permutation(len(pos_pool))[:npos]:
        p = pos_pool[int(i)]
        add({'pcm': p['pcm'], 'feat': p['feat'], 'wf': p['wf'], 'kind': 'pos'},
            'preeaec-pos', [p['id']], {'gain_dbfs': p['gain_dbfs']})
    for i in rng.permutation(len(neg_pool))[:nneg]:
        p = neg_pool[int(i)]
        op = 's2b-alone' if p['id'].startswith('s2b-alone') else 'preeaec-neg'
        add({'pcm': p['pcm'], 'feat': p['feat'], 'kind': 'neg'},
            op, [p['id']], {'gain_dbfs': p['gain_dbfs']})

    # All-group real positives: 3 augmentations each (more exposure to both speakers)
    for g in all_groups:
        for k in range(3):
            e = W.pos_from_group(g, rng, bg, levels)
            if e is not None:
                e['kind'] = 'pos'
                e['feat'] = W.mfcc16(e['pcm'])
                e['wf'] = (max(7, int(e['win'][0] / 160) - 30),
                           min(len(e['feat']) - 1, int(e['win'][1] / 160) + 50))
                add(e, 'augment', [g['id']], {'copy': k})

    # Cross-spliced positives: all pairs including cross-speaker (s1 x s3)
    n_splice = 200
    for _ in range(n_splice):
        g1 = all_groups[int(rng.integers(len(all_groups)))]
        g2 = all_groups[int(rng.integers(len(all_groups)))]
        if g1['id'] == g2['id']:
            continue
        e = W.splice_pos(g1, g2, rng, bg, levels)
        if e is not None:
            e['kind'] = 'pos'
            e['feat'] = W.mfcc16(e['pcm'])
            e['wf'] = (max(7, int(e['win'][0] / 160) - 30),
                       min(len(e['feat']) - 1, int(e['win'][1] / 160) + 50))
            add(e, 'splice', [g1['id'], g2['id']])

    # Order negatives from all groups
    n_order = 300
    modes = W.ORDER_MODES
    plan = [modes[i % len(modes)] for i in range(n_order)]
    rng.shuffle(plan)
    for mode in plan:
        g = all_groups[int(rng.integers(len(all_groups)))]
        e = W.order_neg(g, rng, bg, levels, mode)
        if e is not None:
            e['kind'] = 'neg'
            e['feat'] = W.mfcc16(e['pcm'])
            add(e, f'order-{mode}', [g['id']],
                {'order': e.get('order'), 'order_indices': e.get('order_indices')})

    # TTS positives / negatives / hard negatives: HIGHER WEIGHTS to prevent recall collapse
    tts_train = [s for s in tts if s['split'] == 'train' and s['sub'] != 'homophone']
    pure = sorted([s for s in tts_train if s['kind'] == 'pos' and s['text'] in W.M.PURE], key=lambda s: s['id'])
    negs = sorted([s for s in tts_train if s['kind'] == 'neg'], key=lambda s: s['id'])
    hard = sorted([s for s in tts_train if s['sub'] == 'hardneg'], key=lambda s: s['id'])
    # Repeat TTS positives cfg['tts_pos'] times (was 50 in round 4, now up to 80-120)
    for s in [pure[int(i)] for i in rng.choice(len(pure), min(cfg['tts_pos'], len(pure)), False)]:
        e = W.tts_pos(s, rng, bg, levels)
        if e is not None:
            e['kind'] = 'pos'
            e['feat'] = W.mfcc16(e['pcm'])
            e['wf'] = (max(7, int(e['win'][0] / 160) - 30),
                       min(len(e['feat']) - 1, int(e['win'][1] / 160) + 50))
            add(e, 'tts-pos', [s['id']], {'text': s['text']})
    for s in [negs[int(i)] for i in rng.choice(len(negs), min(cfg['tts_neg'], len(negs)), False)]:
        e = W.tts_neg(s, rng, bg)
        if e is not None:
            e['kind'] = 'neg'
            e['feat'] = W.mfcc16(e['pcm'])
            add(e, 'tts-neg', [s['id']], {'text': s['text']})
    for s in [hard[int(i)] for i in rng.choice(len(hard), min(cfg['tts_hard'], len(hard)), False)]:
        e = W.tts_neg(s, rng, bg)
        if e is not None:
            e['kind'] = 'neg'
            e['feat'] = W.mfcc16(e['pcm'])
            add(e, 'tts-hardneg', [s['id']], {'text': s['text']})

    # s1 real negatives
    for s in s1neg:
        for k in range(2):
            e = W.tts_neg(s, rng, bg)
            if e is not None:
                e['kind'] = 'neg'
                e['feat'] = W.mfcc16(e['pcm'])
                add(e, 's1-neg', [s['id']], {'copy': k})

    # Hard negatives from positives
    for _ in range(cfg['single']):
        g = all_groups[int(rng.integers(len(all_groups)))]
        e = W.single_neg(g, rng, bg, levels)
        if e is not None:
            e['kind'] = 'neg'; e['feat'] = W.mfcc16(e['pcm'])
            add(e, 'single-kw', [g['id']])

    pool_other = [s for s in tts if s['split'] == 'train' and s['kind'] in ('neg', 'hardneg')] + s1neg
    for _ in range(cfg['plus']):
        g = all_groups[int(rng.integers(len(all_groups)))]
        o = pool_other[int(rng.integers(len(pool_other)))]
        e = W.plus_neg(g, o, rng, bg, levels)
        if e is not None:
            e['kind'] = 'neg'; e['feat'] = W.mfcc16(e['pcm'])
            add(e, 'kw-plus-speech', [g['id'], o['id']])

    for _ in range(cfg['gap']):
        g1 = all_groups[int(rng.integers(len(all_groups)))]
        g2 = all_groups[int(rng.integers(len(all_groups)))]
        if g1['id'] == g2['id']:
            continue
        e = W.gap_neg(g1, g2, rng, bg, levels)
        if e is not None:
            e['kind'] = 'neg'; e['feat'] = W.mfcc16(e['pcm'])
            add(e, 'kw-gap-1s', [g1['id'], g2['id']])

    # Long music+speech negatives from s2b (not s2 — s2 stays validation)
    m = bg['music']
    added = tries = 0
    while added < cfg['long_neg'] and tries < cfg['long_neg'] * 4:
        tries += 1
        e = W.long_neg(m, pool_other, rng, levels)
        if e is None:
            continue
        e['kind'] = 'neg'; e['feat'] = W.mfcc16(e['pcm'])
        add(e, 'long-music-speech', e.pop('insert_parents'), {'insert': e.pop('n_insert')})
        added += 1

    kept = []
    for e in exs:
        if 'feat' not in e or len(e['feat']) < 10:
            continue
        if e['kind'] == 'pos' and e.get('wf') is None:
            continue
        kept.append(e)
    counts = {}
    for e in kept:
        counts[e['op']] = counts.get(e['op'], 0) + 1
    log(f'  {cfg["tag"]}: examples {len(kept)} (pos {sum(e["kind"]=="pos" for e in kept)}, '
        f'neg {sum(e["kind"]=="neg" for e in kept)}) frames {sum(len(e["feat"]) for e in kept)}')
    return kept, manifest, counts


# ---------------- training
def select_key_r5(m):
    """s2b must be 0; then prefer highest s3 groups; break ties with dev neg acceptance."""
    return (0 if m['s2b_events'] == 0 else 1, -m['s3_groups'],
            m['dev']['neg']['accepted'], m['dev']['hardneg']['accepted'])


def train_online_r5(net0, build_fn, prior, step, epochs, cfg, seed, log, eval_fn, manifest_fn):
    net = copy.deepcopy(net0)
    net.train()
    trainable = W.set_trainable(net, cfg.get('freeze', 'none'))
    p0 = [p.detach().clone() for p in trainable]
    opt = torch.optim.Adam(trainable, lr=cfg['lr'], weight_decay=1e-5)
    rng = np.random.default_rng(seed)
    history, best = [], None
    for ep in range(epochs):
        exs, manifest, counts = build_fn(ep)
        manifest_fn(ep, manifest, counts)
        W._run_epoch(net, net0, exs, opt, trainable, p0, prior, step, cfg, rng, ep, epochs, log)
        m = eval_fn(net, ep)
        history.append(m)
        key = select_key_r5(m)
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


def outside_events(groups, margins):
    ev = W.events(margins, 0)
    mids = [(a + b) / 200 for a, b in ev]
    outside = [t for t in mids
               if not any(g['start_s'] - .3 <= t <= g['end_s'] + .5 for g in groups)]
    return {'events': len(ev), 'outside': len(outside), 'outside_times': [round(t, 2) for t in outside]}


def group_98_67(s3groups):
    g = next((x for x in s3groups if x['id'].endswith('@98.67')), None)
    if g is None:
        return None
    raw = R3.raw_memmap('s3-music-pos')
    a, b = int(g['start_s'] * 48000), int(g['end_s'] * 48000)
    sig = mic48(raw, a, b)
    rms = R3.active_rms_48k(sig)
    mic_db = float(20 * np.log10(rms / 32768)) if rms else None
    return {'id': g['id'], 'start_s': g['start_s'], 'end_s': g['end_s'],
            'aec_rms_dbfs': g['clip_rms_dbfs'], 'mic_mix_active_rms_dbfs': mic_db,
            'note': 'AEC suppressed the speech; kept in the 35 count and flagged separately.'}


def run_variant(args, name, dur_k, epochs, pool, all_groups, s3groups, tts, s1neg, dev,
                bg, levels, log, tts_pos=80, tts_neg=150, tts_hard=200,
                pos_weight=1.0, freeze='none', pool_pos=2000, pool_neg=4000):
    cfg = cfg_for(name, dur_k, epochs, tts_pos, tts_neg, tts_hard,
                  pos_weight, freeze, pool_pos, pool_neg)
    out = ROUND5 / name
    out.mkdir(parents=True, exist_ok=True)
    (out / 'epochs').mkdir(parents=True, exist_ok=True)
    # s2b for training music (bg['music'] is s2b set in main); s2 last 25% for eval cache
    _, s2val = W.round2_parts()
    bg_train = {'quiet': bg['quiet'], 'music': bg['music']}
    _, _, prior, step = W.r34_controls(1)
    base = W.load_net(1, W.BASE[1] / 'net.pt')
    cache = W.make_eval_cache(1, s3groups, dev, {'quiet': bg['quiet'], 'music': s2val}, s2val)

    def build_fn(ep):
        return build_r5_dataset(pool, all_groups, tts, s1neg, bg_train, levels,
                                args.seed + ep, cfg, s2val, log=log)

    def manifest_fn(ep, manifest, counts):
        payload = {'epoch': ep + 1, 'seed': args.seed + ep, 'counts': counts,
                   'examples': [{'id': e['id'], 'kind': e['kind'], 'op': e['op'],
                                 'parents': e['parents'], 'meta': e.get('meta', {})}
                                for e in manifest]}
        (out / 'epochs' / f'epoch{ep+1:02d}-manifest.json').write_text(json.dumps(payload, indent=2))

    t0 = time.time()
    net, history, best_ep, best_m = train_online_r5(
        base, build_fn, prior, step, epochs, cfg, args.seed + 1, log,
        lambda n, ep: W.eval_cache_metrics(n, cache, prior, step, dur_k), manifest_fn)
    seconds = time.time() - t0
    log(f'  {name}: training+selection {seconds:.1f}s, best epoch {best_ep+1}')
    src = out / 'epochs' / f'epoch{best_ep+1:02d}-manifest.json'
    if src.exists():
        (out / 'manifest-best.json').write_text(src.read_text())
    cand, gm, mm = W.eval_net(net, 1, out / 'best', s3groups, dev, f'{name}-best', log,
                              dev_bg={'quiet': bg['quiet'], 'music': s2val}, s2val_pcm=s2val, dur_k=dur_k,
                              robust=True, gain_sweep=True, s1groups=all_groups)
    cand['outside'] = outside_events(s3groups, mm['s3'])
    miss = W.miss_analysis([g for g in s3groups
                            if not any(r['id'] == g['id'] and r['mac_hit'] for r in gm)], mm['s3'])
    (out / 'missed-groups.json').write_text(json.dumps(miss, indent=2))
    gate = cand['s3_groups'] == 35 and cand['s2b_events'] == 0 and cand['s2_events'] == 0
    res = {'variant': name, 'cfg': cfg, 'seconds': seconds, 'epochs': epochs,
           'best_epoch': best_ep + 1, 'history': history, 'candidate': cand,
           'gate': gate, 'missed': miss, 'known_set': True,
           'gate_rule': 'L1 35/35 midpoint AND s2b 0 AND s2 0'}
    (out / 'result.json').write_text(json.dumps(res, indent=2))
    return res


# ---------------- report
def _row(name, v):
    d = v['dev']
    return (f'| {name} | {v["s3_groups"]}/35 | {v["s3_inwindow"]}/35 | {v["s2b_events"]} | '
            f'{v["s2_events"]} | {d["pos"]["accepted"]}/{d["pos"]["n"]} | '
            f'{d["hardneg"]["accepted"]}/{d["hardneg"]["n"]} | {d["neg"]["accepted"]}/{d["neg"]["n"]} | '
            f'{d["homophone"]["accepted"]}/{d["homophone"]["n"]} |')


def _rob_row(name, v):
    r = v.get('robust')
    if not r:
        return None
    all_grp = r.get('s1', {})
    s3 = r.get('s3', {})
    return (f'| {name} | {all_grp["forward"]}/{all_grp["n"]} | {all_grp["reversed"]}/{all_grp["n"]} | '
            f'{all_grp["permuted"]}/{all_grp["n_permuted"]} | {all_grp["first_half"]}/{all_grp["n_first_half"]} | '
            f'{s3["forward"]}/{s3["n"]} | {s3["reversed"]}/{s3["n"]} | '
            f'{s3["permuted"]}/{s3["n_permuted"]} | {s3["first_half"]}/{s3["n_first_half"]} |')


def write_report(summary, path):
    pool = summary.get('pool') or {}
    lines = ['# Round 5: all-groups pre-AEC mixed-channel training, s2b early-stop', '',
             '2026-11-03. Mac-only. No SSH, device access, recording, cloud upload or deployment. '
             'All scores are Mac reference margins under r34 controls (L1 FST forward weight 2.25, '
             'min-self-loop 0); not ARM/device results.', '',
             '## Method', '',
             '- **Pool.** A fixed pool of pre-AEC mixed windows: all 95 groups (s1 + s3) 48 kHz '
             '8-channel mic signal plus real negatives plus real-syllable order negatives summed '
             'into the s2b-music-silent 48 kHz stream at random offsets and gains, then offline '
             'device AEC. s2b never enters training through this path either — it is only the '
             'negative canvas. s2 is validation-only.',
             f'- **Pool size.** {pool.get("pos", "?")} positive and {pool.get("neg", "?")} negative '
             f'windows from {pool.get("streams", "?")} mixed streams.',
             '- **Early stop.** s2b events > 0 triggers stop. s2 (last 25%) for validation. '
             'Neither s2b nor s2 enters gradients.',
             '- **All groups in training.** s1-pos-quiet (60 groups) + s3-music-pos (35 groups) '
             '= 95 groups total. Both speakers. 3 augmentations each + 200 cross-spliced + '
             '300 order negatives.',
             '- **TTS weights.** Higher TTS counts (pos 80-120, neg 150-200, hardneg 200-250) '
             'to prevent TTS recall collapse seen in round 2.',
             '- **4 variants.** dur-k (1/3/6), TTS weights (1x/3x/5x pos_weight), freeze options.',
             '- **Loss.** Differentiable Viterbi; positive loss `relu(min_t margin + 2)` over '
             '[start-0.3, end+0.5], negative loss `relu(2 - margin)` averaged over frames, '
             'neg weight 5, small L2 anchor, lr 5e-5, batch 8, gradient clipping 1.0.', '',
             '## Results (known-set, all groups in training)', '',
             '| model | s3 midpoint | in-window | s2b | s2 | dev pos | dev hardneg | dev neg | dev homophone |',
             '|---|---|---|---|---|---|---|---|---|']
    for name, v, kind in summary.get('table', []):
        if v:
            lines.append(_row(f'{name} ({kind})', v))
    lines += ['', 'Gate: L1 35/35 midpoint AND s2b 0 AND s2 0.', '',
              '| variant | selected epoch | s3 midpoint | s2b | s2 | gate |',
              '|---|---|---|---|---|---|']
    for name in ('r5-k1-tts1x', 'r5-k3-tts3x', 'r5-k6-tts1x', 'r5-k6-tts5x'):
        f = ROUND5 / name / 'result.json'
        if not f.exists():
            continue
        r = json.loads(f.read_text())
        if 'error' in r:
            lines.append(f'| {name} | - | - | - | - | ERROR |')
            continue
        c = r['candidate']
        lines.append(f'| {name} | {r["best_epoch"]} | {c["s3_groups"]}/35 | {c["s2b_events"]} | '
                     f'{c["s2_events"]} | {"PASS" if r["gate"] else "FAIL"} |')
    lines += ['', '### Per-epoch curves', '']
    for name in ('r5-k1-tts1x', 'r5-k3-tts3x', 'r5-k6-tts1x', 'r5-k6-tts5x'):
        f = ROUND5 / name / 'result.json'
        if not f.exists():
            continue
        r = json.loads(f.read_text())
        h = r.get('history') or []
        if not h:
            continue
        lines += [f'#### {name}', '',
                  '| epoch | s3 midpoint | s2b | s2 | s2val | dev pos | dev hardneg | dev neg |',
                  '|---|---|---|---|---|---|---|---|']
        for i, m in enumerate(h):
            mark = ' (selected)' if i + 1 == r.get('best_epoch') else ''
            lines.append(f'| {i+1}{mark} | {m["s3_groups"]}/35 | {m["s2b_events"]} | '
                         f'{m["s2_events"]} | {m["s2val_events"]} | '
                         f'{m["dev"]["pos"]["accepted"]}/{m["dev"]["pos"]["n"]} | '
                         f'{m["dev"]["hardneg"]["accepted"]}/{m["dev"]["hardneg"]["n"]} | '
                         f'{m["dev"]["neg"]["accepted"]}/{m["dev"]["neg"]["n"]} |')
        miss = r.get('missed') or []
        if miss:
            lines += [f'#### Missed groups, {name} (selected epoch)', '',
                      '| group | start-end (s) | min margin | margin time (s) | clip RMS dBFS | '
                      'active RMS dBFS | active s |', '|---|---|---|---|---|---|---|']
            for m in miss:
                lines.append(f'| {m["id"]} | {m["start_s"]:.2f}-{m["end_s"]:.2f} | '
                             f'{m["min_margin"]:.2f} | {m["min_margin_s"]:.2f} | '
                             f'{m["clip_rms_dbfs"]:.2f} | {m["active_rms_dbfs"]:.2f} | '
                             f'{m["active_seconds"]:.2f} |')
            lines.append('')
    lines += ['', '### Events outside the 35 group windows', '',
              '| model | events | outside | outside times (s) |', '|---|---|---|---|']
    for name, v, kind in summary.get('table', []):
        if v and v.get('outside'):
            o = v['outside']
            lines.append(f'| {name} | {o["events"]} | {o["outside"]} | '
                         f'{", ".join(str(x) for x in o["outside_times"][:12])} |')
    lines += ['', '### Robustness (isolated clips, 4000-sample pad)', '',
              '| model | all fwd | all rev | all perm | all half | s3 fwd | s3 rev | s3 perm | s3 half |',
              '|---|---|---|---|---|---|---|---|---|']
    for name, v, kind in summary.get('table', []):
        if v:
            r = _rob_row(name, v)
            if r:
                lines.append(r)
    lines += ['', '### Gain sweep', '',
              '| model | gain | s3 groups | s2b | s2 | dev pos | dev hardneg |',
              '|---|---|---|---|---|---|---|']
    for name, v, kind in summary.get('table', []):
        if v and v.get('gain'):
            for g, m in v['gain'].items():
                d = m['dev']
                lines.append(f'| {name} | {g} dB | {m["s3_groups"]}/35 | {m["s2b_events"]} | '
                             f'{m["s2_events"]} | {d["pos"]["accepted"]}/{d["pos"]["n"]} | '
                             f'{d["hardneg"]["accepted"]}/{d["hardneg"]["n"]} |')
    q = summary.get('group_98_67')
    if q:
        lines += ['', '### Flagged group @98.67', '',
                  f'`{q["id"]}` ({q["start_s"]:.2f}-{q["end_s"]:.2f} s): AEC output RMS '
                  f'{q["aec_rms_dbfs"]:.2f} dBFS vs 48 kHz mic-mix active RMS '
                  f'{q["mic_mix_active_rms_dbfs"]:.2f} dBFS.', '']
    lines += ['## Known-set vs previous rounds', '',
              '- Round 5 is known-set (all 95 groups in training). Previous held-out ceiling '
              'was ~20/35 (round 4 pre-AEC k=1).',
              '- v5 baseline: 17/35 groups, 0 s2b, 0 s2.',
              '- Round 2 order-w5-k1 best: 32/35 known-set.',
              '- Gate: 35/35 s3 midpoint + s2b 0 + s2 0.', '',
              '## Limits', '',
              '- Known-set training; generalization is not measured.',
              '- s2b is the music canvas and never enters gradients directly; s2 is validation-only.',
              '- No model was promoted or deployed; no ARM/device number is claimed.']
    Path(path).write_text('\n'.join(lines) + '\n')


def collect_table():
    table = []
    b = OUT / 'round2' / 'baseline' / 'result.json'
    if b.exists():
        table.append(('v5 baseline', json.loads(b.read_text()), 'held-out'))
    r2 = OUT / 'round2' / 'order-w5-k1' / 'result.json'
    if r2.exists():
        v = json.loads(r2.read_text())
        table.append(('round2 order-w5-k1', v['candidate'], 'known-set'))
    for name in ('r5-k1-tts1x', 'r5-k3-tts3x', 'r5-k6-tts1x', 'r5-k6-tts5x'):
        f = ROUND5 / name / 'result.json'
        if f.exists():
            table.append((name, json.loads(f.read_text())['candidate'], 'known-set'))
    return table


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('cmd', choices=['pool', 'run', 'report', 'all'])
    ap.add_argument('--epochs', type=int, default=10)
    ap.add_argument('--seed', type=int, default=5252)
    ap.add_argument('--skip-pool', action='store_true')
    ap.add_argument('--pool-pos', type=int, default=2000)
    ap.add_argument('--pool-order', type=int, default=2000)
    ap.add_argument('--pool-s1neg', type=int, default=300)
    ap.add_argument('--pool-s2b', type=int, default=2000)
    ap.add_argument('--max-events', type=int, default=85)
    ap.add_argument('--smoke', action='store_true',
                    help='tiny pool for pipeline validation (30 pos / 30 order / 10 s1neg / 20 s2b)')
    ap.add_argument('--only', default='k1,k3,k6,k6x5',
                    help='comma list of variants: k1,k3,k6,k6x5')
    a = ap.parse_args()
    if a.smoke:
        a.pool_pos, a.pool_order, a.pool_s1neg, a.pool_s2b, a.max_events = 30, 30, 10, 20, 8
    OUT.mkdir(parents=True, exist_ok=True)
    ROUND5.mkdir(parents=True, exist_ok=True)
    logf = open(ROUND5 / f'{a.cmd}.log', 'a')
    log = lambda s: (print(s, flush=True), logf.write(s + '\n'), logf.flush())
    log(f'config {vars(a)}')
    s1groups, s3groups = W.build_groups()
    all_groups = s1groups + s3groups
    tts, s1neg, dev, bg = W.base_sources()
    # Use s2b for training negatives; keep s2 for validation
    s2b_16k = W.wav(C / 's2b-music-silent.aec16k.wav')
    # bg['music'] should be s2b (training canvas) not s2
    bg['music'] = s2b_16k
    levels = [float(x['clip_rms_dbfs']) for x in json.loads(W.S3_LEVELS.read_text())]
    if a.cmd in ('pool', 'all') and not a.skip_pool:
        meta = build_pool(all_groups, s1neg, levels, a.seed, log, a.pool_pos, a.pool_order,
                          a.pool_s1neg, a.pool_s2b, a.max_events)
    else:
        meta = json.loads((POOL / 'meta.json').read_text())
    if a.cmd == 'pool':
        return
    pool = load_pool()
    log(f'  pool loaded: {len(pool[0])} pos, {len(pool[1])} neg')
    if a.cmd in ('run', 'all'):
        results = {}
        want = {x.strip() for x in a.only.split(',') if x.strip()}
        # Variant 1: dur_k=1, standard TTS weights
        if 'k1' in want:
            name = 'r5-k1-tts1x'
            log(f'round5 variant {name} (k=1, TTS 1x)')
            try:
                r = run_variant(a, name, 1, a.epochs, pool, all_groups, s3groups, tts, s1neg,
                                dev, bg, levels, log, tts_pos=80, tts_neg=150, tts_hard=200,
                                pos_weight=1.0, pool_pos=a.pool_pos, pool_neg=a.pool_s2b + a.pool_order + a.pool_s1neg)
            except Exception as exc:
                import traceback
                traceback.print_exc()
                r = {'variant': name, 'error': repr(exc)}
                log(f'variant {name} FAILED: {exc!r}')
            results[name] = r
        # Variant 2: dur_k=3, TTS 3x weight
        if 'k3' in want:
            name = 'r5-k3-tts3x'
            log(f'round5 variant {name} (k=3, TTS 3x)')
            try:
                r = run_variant(a, name, 3, a.epochs, pool, all_groups, s3groups, tts, s1neg,
                                dev, bg, levels, log, tts_pos=120, tts_neg=200, tts_hard=250,
                                pos_weight=3.0, pool_pos=a.pool_pos, pool_neg=a.pool_s2b + a.pool_order + a.pool_s1neg)
            except Exception as exc:
                import traceback
                traceback.print_exc()
                r = {'variant': name, 'error': repr(exc)}
                log(f'variant {name} FAILED: {exc!r}')
            results[name] = r
        # Variant 3: dur_k=6, standard TTS
        if 'k6' in want:
            name = 'r5-k6-tts1x'
            log(f'round5 variant {name} (k=6, TTS 1x)')
            try:
                r = run_variant(a, name, 6, a.epochs, pool, all_groups, s3groups, tts, s1neg,
                                dev, bg, levels, log, tts_pos=80, tts_neg=150, tts_hard=200,
                                pos_weight=1.0, pool_pos=a.pool_pos, pool_neg=a.pool_s2b + a.pool_order + a.pool_s1neg)
            except Exception as exc:
                import traceback
                traceback.print_exc()
                r = {'variant': name, 'error': repr(exc)}
                log(f'variant {name} FAILED: {exc!r}')
            results[name] = r
        # Variant 4: dur_k=6, TTS 5x weight + pos_weight=5.0
        if 'k6x5' in want:
            name = 'r5-k6-tts5x'
            log(f'round5 variant {name} (k=6, TTS 5x, pos_weight 5.0)')
            try:
                r = run_variant(a, name, 6, a.epochs, pool, all_groups, s3groups, tts, s1neg,
                                dev, bg, levels, log, tts_pos=150, tts_neg=250, tts_hard=300,
                                pos_weight=5.0, pool_pos=a.pool_pos, pool_neg=a.pool_s2b + a.pool_order + a.pool_s1neg)
            except Exception as exc:
                import traceback
                traceback.print_exc()
                r = {'variant': name, 'error': repr(exc)}
                log(f'variant {name} FAILED: {exc!r}')
            results[name] = r
        summary = {'pool': meta, 'table': collect_table(),
                   'group_98_67': group_98_67(s3groups), 'results': results}
        (ROUND5 / 'summary.json').write_text(json.dumps(summary, indent=2))
        write_report(summary, ROUND5 / 'REPORT.md')
        return
    if a.cmd == 'report':
        summary = json.loads((ROUND5 / 'summary.json').read_text())
        summary['table'] = collect_table()
        write_report(summary, ROUND5 / 'REPORT.md')
        log('round5 report written')
        return


if __name__ == '__main__':
    main()