"""Round 4: held-out s3, pre-AEC mixed-channel training pool.

Local Mac experiment only. No SSH, device access, recording, cloud upload, or deploy.

Commands
  pool      build the fixed pre-AEC mixed pool under round4/pool/ (>=1500 pos, >=3000 neg)
  run       train/evaluate the held-out variants (k=1, k=6) and the s3 5-fold CV variant
  report    regenerate round4/REPORT.md from round4/summary.json
  all       pool + run

Example
  ~/Library/Caches/xiaoai-sidekick/train-venv/bin/python tools/wake-seq-round4.py all
"""
from pathlib import Path
import argparse, copy, importlib.util, json, math, time
import numpy as np
import torch

R = Path(__file__).resolve().parents[1]
OUT = R / 'temp/analysis/wake-seq'
ROUND4 = OUT / 'round4'
POOL = ROUND4 / 'pool'
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
            'kl_weight': 0.0, 'long_neg': 100, 'order_neg': 0, 'splice': 0,
            'single': 20, 'plus': 20, 'gap': 20, 'tts_pos': 50, 'tts_neg': 100,
            'tts_hard': 150, 'music': 0, 'epochs': 8, 'seed': 4242,
            'pool_pos': 1600, 'pool_neg': 3200}


def cfg_for(name, dur_k, epochs, pool_pos=1600, pool_neg=3200):
    c = dict(BASE_CFG)
    c.update({'tag': name, 'dur_k': dur_k, 'epochs': epochs,
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
    lo = max(0, a48 - 14400)                                       # 0.3 s left context
    hi = min(len(raw), b48 + 24000)                                # 0.5 s right context
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


def build_events(s1groups, s1neg, levels, seed, n_pos=1600, n_order=1400, n_s1neg=200):
    rng = np.random.default_rng(seed)
    raw = R3.raw_memmap('s1-pos-quiet')
    pos, neg = [], []
    # positives: every group repeated with different gains (pool >= 1500)
    per = int(math.ceil(n_pos / len(s1groups)))
    for g in s1groups:
        for _ in range(per):
            e = pos_event(g, raw, levels, rng)
            if e is not None:
                pos.append(e)
    # real-syllable order negatives (pre-AEC, same speaker/channel)
    modes = W.ORDER_MODES
    for i in range(n_order):
        g = s1groups[int(rng.integers(len(s1groups)))]
        e = order_event(g, raw, levels, rng, modes[i % len(modes)])
        if e is not None:
            neg.append(e)
    # s1 real negatives, same mixing path
    for i in range(n_s1neg):
        s = s1neg[int(rng.integers(len(s1neg)))]
        e = s1neg_event(s, raw, levels, rng)
        if e is not None:
            neg.append(e)
    rng.shuffle(pos)
    rng.shuffle(neg)
    return pos, neg


# ---------------- stream placement / AEC / extraction
def build_stream(events, sidx, rng, levels, log):
    s2 = R3.raw_memmap('s2-music-silent')
    nframes = int(.75 * len(s2))
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
        cur = o + n + int(rng.uniform(.4, 1.2) * 48000)
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
    rng = np.random.default_rng(seed)
    s2 = W.wav(C / 's2-music-silent.aec16k.wav')
    n75 = int(.75 * len(s2))
    out = []
    for i in range(count):
        n = int(rng.uniform(2.0, 4.0) * 16000)
        a = int(rng.integers(0, max(1, n75 - n)))
        out.append({'id': f's2-alone-{i:04d}', 'pcm': np.rint(s2[a:a + n]).astype(np.int16),
                    'gain_dbfs': None, 'stream': None})
    return out


def build_pool(s1groups, s1neg, levels, seed, log, n_pos=1600, n_order=1400,
               n_s1neg=200, n_s2=1600, max_events=85):
    POOL.mkdir(parents=True, exist_ok=True)
    rng = np.random.default_rng(seed)
    pos_ev, neg_ev = build_events(s1groups, s1neg, levels, seed, n_pos, n_order, n_s1neg)
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
    neg += s2_alone_windows(n_s2, seed + 1)
    log(f'  pool raw: {len(pos)} pos, {len(neg)} neg')
    # features once
    pf, pw, pl = [], [], []
    for p in pos:
        f = W.mfcc16(p['pcm'])
        if len(f) < 10:
            continue
        s, t = p['win']
        wf = (max(7, int(s / 160) - 30), min(len(f) - 1, int(t / 160) + 50))
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
            'neg_events': len(neg_ev), 's2_alone': n_s2, 'streams': sidx,
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
def build_r4_dataset(pool, s1groups, tts, s1neg, bg, levels, seed, cfg, s3_train=None, log=print):
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
        op = 's2-alone' if p['id'].startswith('s2-alone') else 'preeaec-neg'
        add({'pcm': p['pcm'], 'feat': p['feat'], 'kind': 'neg'},
            op, [p['id']], {'gain_dbfs': p['gain_dbfs']})

    # TTS positives / negatives / hard negatives: keep the round2 sets so dev does not collapse
    tts_train = [s for s in tts if s['split'] == 'train' and s['sub'] != 'homophone']
    pure = sorted([s for s in tts_train if s['kind'] == 'pos' and s['text'] in W.M.PURE], key=lambda s: s['id'])
    negs = sorted([s for s in tts_train if s['kind'] == 'neg'], key=lambda s: s['id'])
    hard = sorted([s for s in tts_train if s['sub'] == 'hardneg'], key=lambda s: s['id'])
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

    # s1-only constructed negatives (same as round2 but without s3 parents)
    pool_other = [s for s in tts if s['split'] == 'train' and s['kind'] in ('neg', 'hardneg')] + s1neg
    if cfg['single'] or cfg['gap'] or cfg['plus']:
        for _ in range(cfg['single']):
            g = s1groups[int(rng.integers(len(s1groups)))]
            e = W.single_neg(g, rng, bg, levels)
            if e is not None:
                e['kind'] = 'neg'; e['feat'] = W.mfcc16(e['pcm'])
                add(e, 'single-kw', [g['id']])
        for _ in range(cfg['gap']):
            g1 = s1groups[int(rng.integers(len(s1groups)))]
            g2 = s1groups[int(rng.integers(len(s1groups)))]
            if g1['id'] != g2['id']:
                e = W.gap_neg(g1, g2, rng, bg, levels)
                if e is not None:
                    e['kind'] = 'neg'; e['feat'] = W.mfcc16(e['pcm'])
                    add(e, 'kw-gap-1s', [g1['id'], g2['id']])
        for _ in range(cfg['plus']):
            g = s1groups[int(rng.integers(len(s1groups)))]
            o = pool_other[int(rng.integers(len(pool_other)))]
            e = W.plus_neg(g, o, rng, bg, levels)
            if e is not None:
                e['kind'] = 'neg'; e['feat'] = W.mfcc16(e['pcm'])
                add(e, 'kw-plus-speech', [g['id'], o['id']])

    # long music+speech negatives from the first 75% of s2
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

    # CV variant only: add the 28 non-test s3 groups (never the test fold)
    if s3_train:
        for g in s3_train:
            for k in range(2):
                e = W.pos_from_group(g, rng, bg, levels)
                if e is not None:
                    e['kind'] = 'pos'; e['feat'] = W.mfcc16(e['pcm'])
                    e['wf'] = (max(7, int(e['win'][0] / 160) - 30),
                               min(len(e['feat']) - 1, int(e['win'][1] / 160) + 50))
                    add(e, 's3-train-pos', [g['id']], {'copy': k})
            e = W.order_neg(g, rng, bg, levels, 'reverse')
            if e is not None:
                e['kind'] = 'neg'; e['feat'] = W.mfcc16(e['pcm'])
                add(e, 's3-train-order', [g['id']])

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
def select_key_r4(m):
    # never uses s3: the headline metric must stay held-out
    return (0 if m['s2val_events'] == 0 else 1, m['dev']['neg']['accepted'],
            m['dev']['hardneg']['accepted'])


def train_online_r4(net0, build_fn, prior, step, epochs, cfg, seed, log, eval_fn, manifest_fn):
    net = copy.deepcopy(net0)
    net.train()
    trainable = [p for p in net.parameters() if p.requires_grad]
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
        key = select_key_r4(m)
        if best is None or key < best[0]:
            best = (key, ep, copy.deepcopy(net.state_dict()), m)
        log(f'  epoch {ep+1} eval: groups {m["s3_groups"]}/35 (held-out), s2b {m["s2b_events"]}, '
            f's2 {m["s2_events"]}, s2val {m["s2val_events"]}, '
            f'dev pos {m["dev"]["pos"]["accepted"]}/{m["dev"]["pos"]["n"]}, '
            f'hardneg {m["dev"]["hardneg"]["accepted"]}/{m["dev"]["hardneg"]["n"]}, '
            f'neg {m["dev"]["neg"]["accepted"]}/{m["dev"]["neg"]["n"]}')
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


def run_variant(args, name, dur_k, epochs, pool, s1groups, s3groups, tts, s1neg, dev,
                bg, levels, log, pool_pos=1600, pool_neg=3200):
    cfg = cfg_for(name, dur_k, epochs, pool_pos, pool_neg)
    out = ROUND4 / name
    out.mkdir(parents=True, exist_ok=True)
    (out / 'epochs').mkdir(parents=True, exist_ok=True)
    s2train, s2val = W.round2_parts()
    bg_train = {'quiet': bg['quiet'], 'music': s2train}
    _, _, prior, step = W.r34_controls(1)
    base = W.load_net(1, W.BASE[1] / 'net.pt')
    cache = W.make_eval_cache(1, s3groups, dev, bg_train, s2val)

    def build_fn(ep):
        return build_r4_dataset(pool, s1groups, tts, s1neg, bg_train, levels,
                                args.seed + ep, cfg, s3_train=None, log=log)

    def manifest_fn(ep, manifest, counts):
        payload = {'epoch': ep + 1, 'seed': args.seed + ep, 'counts': counts,
                   'examples': [{'id': e['id'], 'kind': e['kind'], 'op': e['op'],
                                 'parents': e['parents'], 'meta': e.get('meta', {})}
                                for e in manifest]}
        (out / 'epochs' / f'epoch{ep+1:02d}-manifest.json').write_text(json.dumps(payload, indent=2))

    t0 = time.time()
    net, history, best_ep, best_m = train_online_r4(
        base, build_fn, prior, step, epochs, cfg, args.seed + 1, log,
        lambda n, ep: W.eval_cache_metrics(n, cache, prior, step, dur_k), manifest_fn)
    seconds = time.time() - t0
    log(f'  {name}: training+selection {seconds:.1f}s, best epoch {best_ep+1}')
    src = out / 'epochs' / f'epoch{best_ep+1:02d}-manifest.json'
    if src.exists():
        (out / 'manifest-best.json').write_text(src.read_text())
    cand, gm, mm = W.eval_net(net, 1, out / 'best', s3groups, dev, f'{name}-best', log,
                              dev_bg=bg_train, s2val_pcm=s2val, dur_k=dur_k,
                              robust=True, gain_sweep=True, s1groups=s1groups)
    cand['outside'] = outside_events(s3groups, mm['s3'])
    miss = W.miss_analysis([g for g in s3groups
                            if not any(r['id'] == g['id'] and r['mac_hit'] for r in gm)], mm['s3'])
    (out / 'missed-groups.json').write_text(json.dumps(miss, indent=2))
    gate = cand['s3_groups'] == 35 and cand['s2b_events'] == 0 and cand['s2_events'] == 0
    res = {'variant': name, 'cfg': cfg, 'seconds': seconds, 'epochs': epochs,
           'best_epoch': best_ep + 1, 'history': history, 'candidate': cand,
           'gate': gate, 'missed': miss, 'held_out': True,
           'gate_rule': 'L1 35/35 midpoint AND s2b 0 AND s2 0'}
    (out / 'result.json').write_text(json.dumps(res, indent=2))
    return res


def run_cv(args, pool, s1groups, s3groups, tts, s1neg, dev, bg, levels, log, dur_k=1,
           epochs=4, pool_pos=800, pool_neg=1600):
    out = ROUND4 / f'preeaec-k{dur_k}-cv'
    out.mkdir(parents=True, exist_ok=True)
    s2train, s2val = W.round2_parts()
    bg_train = {'quiet': bg['quiet'], 'music': s2train}
    _, _, prior, step = W.r34_controls(1)
    base = W.load_net(1, W.BASE[1] / 'net.pt')
    cache = W.make_eval_cache(1, s3groups, dev, bg_train, s2val)
    folds = []
    total_hits = total_s2b = total_s2 = total_out = 0
    for k, test_ids in enumerate(W.fold_ids(s3groups)):
        name = f'preeaec-k{dur_k}-cv-fold{k}'
        cfg = cfg_for(name, dur_k, epochs, pool_pos, pool_neg)
        d = out / f'fold{k}'
        d.mkdir(parents=True, exist_ok=True)
        (d / 'epochs').mkdir(parents=True, exist_ok=True)
        s3_train = [g for g in s3groups if g['id'] not in test_ids]

        def build_fn(ep):
            return build_r4_dataset(pool, s1groups, tts, s1neg, bg_train, levels,
                                    args.seed + 1000 + ep, cfg, s3_train=s3_train, log=log)

        def manifest_fn(ep, manifest, counts):
            (d / 'epochs' / f'epoch{ep+1:02d}-manifest.json').write_text(
                json.dumps({'epoch': ep + 1, 'counts': counts}, indent=2))

        net, history, best_ep, best_m = train_online_r4(
            base, build_fn, prior, step, epochs, cfg, args.seed + 200 + k, log,
            lambda n, ep: W.eval_cache_metrics(n, cache, prior, step, dur_k), manifest_fn)
        cand, gm, mm = W.eval_net(net, 1, d / 'best', s3groups, dev, name, log,
                                  dev_bg=bg_train, s2val_pcm=s2val, dur_k=dur_k,
                                  robust=False, gain_sweep=False, s1groups=None)
        hits = sum(r['mac_hit'] for r in gm if r['id'] in test_ids)
        oe = outside_events([g for g in s3groups if g['id'] in test_ids], mm['s3'])
        folds.append({'fold': k, 'test_ids': sorted(test_ids), 'hits': hits,
                      's2b_events': cand['s2b_events'], 's2_events': cand['s2_events'],
                      'outside': oe, 'best_epoch': best_ep + 1})
        total_hits += hits
        total_s2b += cand['s2b_events']
        total_s2 += cand['s2_events']
        total_out += oe['outside']
        log(f'  cv fold{k}: test {len(test_ids)} groups, hits {hits}/{len(test_ids)}, '
            f's2b {cand["s2b_events"]}, s2 {cand["s2_events"]}, outside {oe["outside"]}')
    res = {'variant': f'preeaec-k{dur_k}-cv', 'dur_k': dur_k, 'epochs': epochs,
           'pool_pos': pool_pos, 'pool_neg': pool_neg,
           'folds': folds, 'held_out_hits': total_hits, 'total_groups': 35,
           's2b_events': total_s2b, 's2_events': total_s2,
           'outside_events': total_out, 'held_out': True,
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
    s1, s3 = r['s1'], r['s3']
    return (f'| {name} | {s1["forward"]}/{s1["n"]} | {s1["reversed"]}/{s1["n"]} | '
            f'{s1["permuted"]}/{s1["n_permuted"]} | {s1["first_half"]}/{s1["n_first_half"]} | '
            f'{s3["forward"]}/{s3["n"]} | {s3["reversed"]}/{s3["n"]} | '
            f'{s3["permuted"]}/{s3["n_permuted"]} | {s3["first_half"]}/{s3["n_first_half"]} |')


def write_report(summary, path):
    pool = summary.get('pool') or {}
    lines = ['# Round 4: held-out s3 with pre-AEC mixed-channel training', '',
             '2026-11-03. Mac-only. No SSH, device access, recording, cloud upload or deployment. '
             'All scores are Mac reference margins under r34 controls (L1 FST forward weight 2.25, '
             'min-self-loop 0); not ARM/device results.', '',
             '## Method', '',
             '- **Pool.** A fixed pool of pre-AEC mixed windows: s1-pos-quiet 48 kHz 8-channel mic '
             'signal plus s1 real negatives plus real-syllable order negatives (reverse/permute/'
             'truncate/single/syllable-reverse) summed into the s2-music-silent 48 kHz 8-channel '
             'stream (reference channels untouched), at random offsets and gains drawn from the '
             '35 s3 group RMS distribution, then offline device AEC (`tools/lx06-aec-offline.c`). '
             'Only the first 75% of s2 is used; the last 25% stays validation-only. s2b never enters '
             'training. The pool is generated once and cached under `round4/pool/`.',
             f'- **Pool size.** {pool.get("pos", "?")} positive and {pool.get("neg", "?")} negative '
             f'windows from {pool.get("streams", "?")} mixed 225 s streams ('
             f'{pool.get("pos_events", "?")} positive events, {pool.get("neg_events", "?")} mixed '
             f'negative events, {pool.get("s2_alone", "?")} s2-alone windows). Manifest: '
             '`round4/<variant>/epochs/epochNN-manifest.json`, best epoch copied to '
             '`round4/<variant>/manifest-best.json`.',
             '- **Held-out rule.** The headline variants train on the s1-derived pool + TTS '
             'positives/negatives/hard negatives + s1 constructed negatives + long music/speech '
             'negatives only. The 35 s3 groups are never used for training or per-epoch selection; '
             'they are the held-out test. Per-epoch selection uses s2val and dev false-accept '
             'counts only (`select_key_r4`).',
             '- **Loss.** Differentiable Viterbi identical to `F.viterbi` (r34 FST, prior, scale 0.1); '
             'positive loss `relu(min_t margin + 2)` over [start-0.3 s, end+0.5 s], negative loss '
             '`relu(2 - margin)` averaged over frames, neg weight 5, small L2 anchor to the v5 '
             'initialisation, lr 5e-5, batch 8, gradient clipping 1.0. k=1 uses the r34 FST '
             'recurrence; k=6 uses the min-duration-k decoder and is evaluation-only because the '
             'r34 FST cannot express k>1.',
             '- **CV variant.** One variant adds the 28 non-test s3 groups (2 positives + 1 reverse '
             'order negative each) to training and rotates the 7-group test fold (time-ordered 35 '
             'groups); the summed held-out hits are the multi-model generalization number.', '',
             '## Results (held-out s3)', '',
             '| model | s3 midpoint | in-window | s2b | s2 | dev pos | dev hardneg | dev neg | dev homophone |',
             '|---|---|---|---|---|---|---|---|---|']
    for name, v, kind in summary.get('table', []):
        if v:
            lines.append(_row(f'{name} ({kind})', v))
    lines += ['', 'Gate: L1 35/35 midpoint AND s2b 0 AND s2 0.', '',
              '| variant | selected epoch | s3 midpoint | s2b | s2 | gate |',
              '|---|---|---|---|---|---|']
    for name in ('preeaec-k1', 'preeaec-k6'):
        f = ROUND4 / name / 'result.json'
        if not f.exists():
            continue
        r = json.loads(f.read_text())
        if 'error' in r:
            lines.append(f'| {name} | - | - | - | - | ERROR |')
            continue
        c = r['candidate']
        lines.append(f'| {name} | {r["best_epoch"]} | {c["s3_groups"]}/35 | {c["s2b_events"]} | '
                     f'{c["s2_events"]} | {"PASS" if r["gate"] else "FAIL"} |')
    cv = summary.get('cv')
    if cv:
        lines += ['', f'CV summed held-out: {cv["held_out_hits"]}/35 groups, '
                  f's2b {cv["s2b_events"]}, s2 {cv["s2_events"]}, outside-window events '
                  f'{cv["outside_events"]}.', '',
                  '| fold | test groups | hits | s2b | s2 | outside events |', '|---|---|---|---|---|---|']
        for f in cv['folds']:
            lines.append(f'| {f["fold"]} | {f["test_ids"][0]}..{f["test_ids"][-1]} | '
                         f'{f["hits"]}/{len(f["test_ids"])} | {f["s2b_events"]} | {f["s2_events"]} | '
                         f'{f["outside"]["outside"]} |')
    # per-epoch diagnostics (the s3 column is NOT used for selection)
    for name in ('preeaec-k1', 'preeaec-k6'):
        f = ROUND4 / name / 'result.json'
        if not f.exists():
            continue
        r = json.loads(f.read_text())
        h = r.get('history') or []
        if not h:
            continue
        best_unselected = max(range(len(h)), key=lambda i: h[i]['s3_groups'])
        lines += ['', f'### Per-epoch curve: {name} (diagnostic only; s3 is not used for selection)', '',
                  '| epoch | s3 midpoint | s2b | s2 | s2val | dev pos | dev hardneg | dev neg |',
                  '|---|---|---|---|---|---|---|---|']
        for i, m in enumerate(h):
            mark = ' (selected)' if i + 1 == r.get('best_epoch') else ''
            lines.append(f'| {i+1}{mark} | {m["s3_groups"]}/35 | {m["s2b_events"]} | '
                         f'{m["s2_events"]} | {m["s2val_events"]} | '
                         f'{m["dev"]["pos"]["accepted"]}/{m["dev"]["pos"]["n"]} | '
                         f'{m["dev"]["hardneg"]["accepted"]}/{m["dev"]["hardneg"]["n"]} | '
                         f'{m["dev"]["neg"]["accepted"]}/{m["dev"]["neg"]["n"]} |')
        lines += ['', f'Best unselected diagnostic epoch: {best_unselected+1} '
                  f'({h[best_unselected]["s3_groups"]}/35). This is reported for honesty only; '
                  'choosing it would leak held-out information.', '']
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
              '| model | s1 fwd | s1 rev | s1 perm | s1 half | s3 fwd | s3 rev | s3 perm | s3 half |',
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
                  f'{q["mic_mix_active_rms_dbfs"]:.2f} dBFS. The AEC suppressed this utterance; '
                  'it is kept in the 35 count and flagged separately.', '']
    lines += ['## Known-set vs held-out', '',
              '- `pre-AEC k=1/k=6` are held-out: no s3 group was used in training or selection.',
              '- `round2 order-w5-k1` is known-set (trained on all 35 groups) and is shown only as a '
              'reference; it is not a generalization number.',
              '- The CV row is the only multi-model held-out estimate; its 35 hits are summed over '
              'five independently trained models, each tested on its own 7 groups.',
              '- Per-epoch s3 curves are diagnostic only. Selecting the best s3 epoch would violate '
              'the held-out rule, so the headline uses the s2val/dev-false-accept selection.', '',
              '## Conclusion', '']
    k1 = next((v for n, v, k in summary.get('table', []) if n == 'preeaec-k1'), None)
    k6 = next((v for n, v, k in summary.get('table', []) if n == 'preeaec-k6'), None)
    base = next((v for n, v, k in summary.get('table', []) if n == 'v5 baseline'), None)
    if base:
        lines.append(f'- v5 baseline under the same r34 controls: {base["s3_groups"]}/35, '
                     f's2b {base["s2b_events"]}, s2 {base["s2_events"]}.')
    if k1:
        lines.append(f'- Held-out pre-AEC k=1 (selected epoch): {k1["s3_groups"]}/35, '
                     f's2b {k1["s2b_events"]}, s2 {k1["s2_events"]}.')
    if k6:
        lines.append(f'- Held-out pre-AEC k=6 (selected epoch): {k6["s3_groups"]}/35, '
                     f's2b {k6["s2b_events"]}, s2 {k6["s2_events"]}.')
    if cv:
        lines.append(f'- 5-fold s3 CV summed held-out hits: {cv["held_out_hits"]}/35, '
                     f's2b {cv["s2b_events"]}, s2 {cv["s2_events"]}.')
    lines += ['', '## Limits', '',
              '- Pool windows come from s1 speakers only; s3 speakers are never used to build the '
              'pool. Cross-speaker behavior is therefore the honest test.',
              '- s2b is the independent music veto and never enters gradients; the last 25% of s2 is '
              'validation-only.',
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
    for name, k in (('preeaec-k1', 1), ('preeaec-k6', 6)):
        f = ROUND4 / name / 'result.json'
        if f.exists():
            table.append((name, json.loads(f.read_text())['candidate'], 'held-out'))
    return table


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('cmd', choices=['pool', 'run', 'report', 'all'])
    ap.add_argument('--epochs', type=int, default=8)
    ap.add_argument('--cv-epochs', type=int, default=4)
    ap.add_argument('--seed', type=int, default=4242)
    ap.add_argument('--skip-pool', action='store_true')
    ap.add_argument('--pool-pos', type=int, default=1600)
    ap.add_argument('--pool-order', type=int, default=1400)
    ap.add_argument('--pool-s1neg', type=int, default=200)
    ap.add_argument('--pool-s2', type=int, default=1600)
    ap.add_argument('--max-events', type=int, default=85)
    ap.add_argument('--smoke', action='store_true',
                    help='tiny pool for pipeline validation (20 pos / 20 order / 5 s1neg / 10 s2)')
    ap.add_argument('--only', default='k1,k6,cv',
                    help='comma list of variants to run: k1,k6,cv')
    a = ap.parse_args()
    if a.smoke:
        a.pool_pos, a.pool_order, a.pool_s1neg, a.pool_s2, a.max_events = 20, 20, 5, 10, 8
    OUT.mkdir(parents=True, exist_ok=True)
    ROUND4.mkdir(parents=True, exist_ok=True)
    logf = open(ROUND4 / f'{a.cmd}.log', 'a')
    log = lambda s: (print(s, flush=True), logf.write(s + '\n'), logf.flush())
    log(f'config {vars(a)}')
    s1groups, s3groups = W.build_groups()
    tts, s1neg, dev, bg = W.base_sources()
    levels = [float(x['clip_rms_dbfs']) for x in json.loads(W.S3_LEVELS.read_text())]
    if a.cmd in ('pool', 'all') and not a.skip_pool:
        meta = build_pool(s1groups, s1neg, levels, a.seed, log, a.pool_pos, a.pool_order,
                          a.pool_s1neg, a.pool_s2, a.max_events)
    else:
        meta = json.loads((POOL / 'meta.json').read_text())
    if a.cmd == 'pool':
        return
    pool = load_pool()
    log(f'  pool loaded: {len(pool[0])} pos, {len(pool[1])} neg')
    if a.cmd in ('run', 'all'):
        results = {}
        want = {x.strip() for x in a.only.split(',') if x.strip()}
        for name, k, key in (('preeaec-k1', 1, 'k1'), ('preeaec-k6', 6, 'k6')):
            if key not in want:
                continue
            log(f'round4 variant {name} (held-out s3)')
            try:
                r = run_variant(a, name, k, a.epochs, pool, s1groups, s3groups, tts, s1neg,
                                dev, bg, levels, log)
            except Exception as exc:
                import traceback
                traceback.print_exc()
                r = {'variant': name, 'error': repr(exc)}
                log(f'variant {name} FAILED: {exc!r}')
            results[name] = r
        if 'cv' in want:
            log('round4 CV variant preeaec-k1-cv (28 train / 7 test s3 groups)')
            try:
                results['cv'] = run_cv(a, pool, s1groups, s3groups, tts, s1neg, dev, bg, levels,
                                       log, dur_k=1, epochs=a.cv_epochs, pool_pos=800,
                                       pool_neg=1600)
            except Exception as exc:
                import traceback
                traceback.print_exc()
                results['cv'] = {'variant': 'preeaec-k1-cv', 'error': repr(exc)}
                log(f'CV FAILED: {exc!r}')
        summary = {'pool': meta, 'table': collect_table(), 'cv': results.get('cv'),
                   'group_98_67': group_98_67(s3groups), 'results': results}
        (ROUND4 / 'summary.json').write_text(json.dumps(summary, indent=2))
        write_report(summary, ROUND4 / 'REPORT.md')
        return
    if a.cmd == 'report':
        summary = json.loads((ROUND4 / 'summary.json').read_text())
        summary['table'] = collect_table()
        write_report(summary, ROUND4 / 'REPORT.md')
        log('round4 report written')
        return


if __name__ == '__main__':
    main()
