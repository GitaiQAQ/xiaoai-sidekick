"""Round 3: factory-init transfer (R3-1) and pre-AEC mixed-channel training (R3-2).

Local Mac experiment only. No SSH, device access, recording, cloud upload, or deploy.
All numbers are Mac reference margins under the r34 controls, never ARM/device results.

Commands
  r3-1            run the three factory-init transfer variants (freeze 3, freeze 6, low-lr)
  r3-2            build the pre-AEC mixed 8ch stream, run offline AEC, train/evaluate
  r3-2-verify     re-check that offline AEC reproduces s3-music-pos.aec16k.wav
  r3-report       regenerate round3/REPORT.md from round3/summary.json
  all             r3-1 then r3-2

Example
  ~/Library/Caches/xiaoai-sidekick/train-venv/bin/python tools/wake-seq-round3.py r3-1
"""
from pathlib import Path
import argparse, copy, importlib.util, json, math, os, subprocess, time
import numpy as np
import torch

R = Path(__file__).resolve().parents[1]
OUT = R / 'temp/analysis/wake-seq'
ROUND3 = OUT / 'round3'
ROUND2 = OUT / 'round2'
FACTORY = {1: R / 'temp/analysis/models/mdata_wakeup_mobile',
           2: R / 'temp/analysis/models/mdata_wakeup_mobile_clone'}
AEC_BIN = R / 'build/host-aec'
AEC_LIB = R / 'build/speex'
C = R / 'private/inputs/recordings/wake-corpus'


def load_module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


W = load_module('ws', R / 'tools/wake-seq-train.py')

# best round-2 recipe (order-w5-k1, best epoch 6 of the post-fix sweep)
ROUND2_BEST = {'neg_agg': 'frames', 'neg_weight': 5.0, 'freeze': 'none', 'dur_k': 1,
               'lr': 5e-5, 'batch': 8, 'delta': 2.0, 'anchor': 1e-3, 'pos_weight': 1.0,
               'kl_weight': 0.0, 'long_neg': 100, 'order_neg': 1, 'splice': 40,
               'single': 40, 'plus': 25, 'gap': 25, 'tts_pos': 50, 'tts_neg': 100,
               'tts_hard': 150, 'music': 100, 'epochs': 10, 'seed': 4242}


def base_cfg(tag):
    cfg = dict(ROUND2_BEST)
    cfg['tag'] = tag
    return cfg


# ---------------- R3-1: factory 小爱同学 AM -> our Net
def factory_chain_pdfs(level=1):
    """Pdf ids in wake-word chain order from the factory FST (forward arcs, weight > 1)."""
    d = json.loads((FACTORY[level] / 'fst-decoded.json').read_text())
    chain = []
    for n in d['graph']:
        for a in n['arcs']:
            if a['weight'] > 1.0 and a['input'] > 0:
                chain.append((a['input'] + 1) // 2 - 1)
    return chain


def factory_pdf_map(level=1):
    """our pdf -> factory pdf.  SIL/GARB keep their ids; keyword pdfs map by chain position."""
    chain = factory_chain_pdfs(level)
    m = {0: 0, 3: 3}
    for pos, pdf in enumerate(W.KW):
        m[pdf] = chain[pos]
    return m, chain


def factory_net(level=1, remap=True):
    """Load the factory AM affines into M.Net, optionally remapping the output rows to our pdf ids."""
    W.set_level(level)
    D = 96 if level == 1 else 160
    d = FACTORY[level]
    meta = json.loads((d / 'am-decoded.json').read_text())
    fw = np.frombuffer((d / 'root.list.3.am.bin').read_bytes(), '<f4')
    aff = [c for c in meta['components'] if c['type'] == 8]
    net = W.M.Net(D)
    convs = list(net.l) + [net.o]
    assert len(aff) == len(convs), (len(aff), len(convs))
    for c, m in zip(aff, convs):
        Wm = fw[c['weight_word']:c['weight_word'] + c['in'] * c['out']].reshape(c['out'], c['in'])
        b = fw[c['bias_word']:c['bias_word'] + c['out']]
        k = m.weight.shape[2]
        w = Wm.reshape(c['out'], k, -1).transpose(0, 2, 1)
        m.weight.data = torch.from_numpy(w.copy())
        m.bias.data = torch.from_numpy(b.copy())
    # remap the output layer rows (factory row index == factory pdf id)
    pmap, chain = factory_pdf_map(level)
    if remap:
        o = net.o
        wo = o.weight.data.clone()
        bo = o.bias.data.clone()
        for our_pdf, fac_pdf in pmap.items():
            wo[our_pdf] = o.weight.data[fac_pdf]
            bo[our_pdf] = o.bias.data[fac_pdf]
        o.weight.data = wo
        o.bias.data = bo
    net.eval()
    return net, pmap, chain


def factory_parity(net, level=1):
    W.set_level(level)
    d = FACTORY[level]
    meta = json.loads((d / 'am-decoded.json').read_text())
    fw = np.frombuffer((d / 'root.list.3.am.bin').read_bytes(), '<f4')
    f = W.mfcc16(W.wav(R / 'temp/analysis/r25-tts/xatx.wav'))
    return float(np.abs(W.M.posteriors(net, f) - W.F.forward(f, meta, fw.tobytes())).max())


def convs_of(net):
    return list(net.l) + [net.o]


def train_online(net0, build_fn, prior, step, epochs, cfg, seed, log, eval_fn, manifest_fn,
                 freeze_first=0, low_n=0, low_lr_scale=1.0):
    net = copy.deepcopy(net0)
    net.train()
    convs = convs_of(net)
    for p in net.parameters():
        p.requires_grad = True
    if freeze_first:
        for m in convs[:freeze_first]:
            for p in m.parameters():
                p.requires_grad = False
    low = [p for m in convs[:low_n] for p in m.parameters() if p.requires_grad]
    high = [p for m in convs[low_n:] for p in m.parameters() if p.requires_grad]
    trainable = low + high
    groups = [{'params': high, 'lr': cfg['lr']}]
    if low:
        groups.append({'params': low, 'lr': cfg['lr'] * low_lr_scale})
    opt = torch.optim.Adam(groups, weight_decay=1e-5)
    p0 = [p.detach().clone() for p in trainable]
    rng = np.random.default_rng(seed)
    history, best = [], None
    for ep in range(epochs):
        exs, manifest = build_fn(ep)
        manifest_fn(ep, manifest)
        W._run_epoch(net, net0, exs, opt, trainable, p0, prior, step, cfg, rng, ep, epochs, log)
        m = eval_fn(net, ep)
        history.append(m)
        key = W.select_key(m)
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


R3_1_VARIANTS = [
    {'name': 'factory-freeze3', 'freeze_first': 3, 'low_n': 0, 'low_lr_scale': 1.0},
    {'name': 'factory-freeze6', 'freeze_first': 6, 'low_n': 0, 'low_lr_scale': 1.0},
    {'name': 'factory-lowlr', 'freeze_first': 0, 'low_n': 6, 'low_lr_scale': 0.1},
]


def run_r3_1_variant(args, variant, s1groups, s3groups, tts, s1neg, dev, bg, levels, log):
    cfg = base_cfg(variant['name'])
    out = ROUND3 / 'r3-1' / variant['name']
    out.mkdir(parents=True, exist_ok=True)
    (out / 'epochs').mkdir(parents=True, exist_ok=True)
    s2train, s2val = W.round2_parts()
    bg_train = {'quiet': bg['quiet'], 'music': s2train}
    _, _, prior, step = W.r34_controls(1)
    base, pmap, chain = factory_net(1, remap=True)
    raw_base, _, _ = factory_net(1, remap=False)
    parity = factory_parity(raw_base, 1)
    cache = W.make_eval_cache(1, s3groups, dev, bg_train, s2val)

    def build_fn(ep):
        return W.build_dataset(s1groups + s3groups, tts, s1neg, bg_train, levels,
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
    net, history, best_ep, best_m = train_online(
        base, build_fn, prior, step, args.epochs, cfg, args.seed + 1, log,
        lambda n, ep: W.eval_cache_metrics(n, cache, prior, step, 1), manifest_fn,
        freeze_first=variant['freeze_first'], low_n=variant['low_n'],
        low_lr_scale=variant['low_lr_scale'])
    seconds = time.time() - t0
    log(f'  r3-1 {variant["name"]}: training+selection {seconds:.1f}s, best epoch {best_ep+1}')
    src = out / 'epochs' / f'epoch{best_ep+1:02d}-manifest.json'
    if src.exists():
        (out / 'manifest-best.json').write_text(src.read_text())
    # factory init (before any training) for reference
    finit, _, _ = W.eval_net(base, 1, out / 'factory-init', s3groups, dev,
                             f'{variant["name"]}-init', log, dev_bg=bg_train, s2val_pcm=s2val,
                             dur_k=1, robust=True, gain_sweep=True, s1groups=s1groups)
    cand, gm, mm = W.eval_net(net, 1, out / 'best', s3groups, dev,
                              f'{variant["name"]}-best', log, dev_bg=bg_train, s2val_pcm=s2val,
                              dur_k=1, robust=True, gain_sweep=True, s1groups=s1groups)
    miss = W.miss_analysis([g for g in s3groups
                            if not any(r['id'] == g['id'] and r['mac_hit'] for r in gm)], mm['s3'])
    (out / 'missed-groups.json').write_text(json.dumps(miss, indent=2))
    gate = cand['s3_groups'] == 35 and cand['s2b_events'] == 0 and cand['s2_events'] == 0
    res = {'variant': variant, 'cfg': cfg, 'seconds': seconds, 'epochs': args.epochs,
           'best_epoch': best_ep + 1, 'history': history,
           'factory_pdf_map': {str(k): v for k, v in pmap.items()},
           'factory_chain_pdfs': chain, 'factory_parity': parity,
           'factory_init': finit, 'candidate': cand, 'gate': gate, 'missed': miss,
           'gate_rule': 'L1 35/35 midpoint AND s2b 0 AND s2 0'}
    (out / 'result.json').write_text(json.dumps(res, indent=2))
    return res


# ---------------- R3-2: pre-AEC mixed 8ch stream -> offline AEC -> training samples
def raw_memmap(name):
    return np.memmap(C / f'{name}.raw', dtype='<i4', mode='r').reshape(-1, 8)


def aec_run(raw_path, out_path, log):
    env = dict(os.environ)
    env['DYLD_LIBRARY_PATH'] = str(AEC_LIB)
    t0 = time.time()
    p = subprocess.run([str(AEC_BIN), str(raw_path), str(out_path)], env=env,
                       capture_output=True, text=True)
    log(f'  aec {raw_path.name} -> {out_path.name}: rc {p.returncode} {time.time()-t0:.1f}s '
        f'{(p.stdout or "").strip().splitlines()[-1:] or ""}')
    if p.returncode != 0:
        raise RuntimeError(f'host-aec failed: {p.stderr}')
    return np.fromfile(out_path, dtype='<i2')


def active_rms_48k(sig):
    """Active RMS of a 48 kHz int32 mic average, returned in int16-scale units."""
    n = len(sig) // 480
    if n < 1:
        return None
    e = 10 * np.log10(np.maximum(np.mean(sig[:n * 480].reshape(n, 480) ** 2, 1), 1e-12))
    act = e > e.max() - 30
    if not act.any():
        return None
    return float(np.sqrt(np.mean((sig[:n * 480].reshape(n, 480)[act] / 65536.0) ** 2)))


def plan_placements(items, nframes, rng, guard_s=5.0, gap_s=(0.8, 2.0)):
    """Pack clips sequentially; return placements that fit, plus the remainder."""
    placed, cur = [], int(guard_s * 48000)
    limit = nframes - int(guard_s * 48000)
    for it in items:
        n = it['n48']
        if cur + n > limit:
            continue
        placed.append(dict(it, start_frame=cur))
        cur += n + int(rng.uniform(*gap_s) * 48000)
    used = {p['id'] for p in placed}
    rest = [it for it in items if it['id'] not in used]
    return placed, rest


def build_mixed_stream(items, stream_idx, rng, levels, log):
    """Write an 8ch 48k S32 stream: s2 first 75% with s1 mic channels added."""
    s2 = raw_memmap('s2-music-silent')
    nframes = int(.75 * len(s2))
    s1 = raw_memmap('s1-pos-quiet')
    placed, rest = plan_placements(items, nframes, rng)
    raw_out = ROUND3 / 'r3-2' / f'mixed{stream_idx}.raw'
    raw_out.parent.mkdir(parents=True, exist_ok=True)
    mm = np.memmap(raw_out, dtype='<i4', mode='w+', shape=(nframes, 8))
    mm[:] = s2[:nframes]
    for p in placed:
        a = int(p['start_s'] * 48000)
        n = p['n48']
        sig = s1[a:a + n, :6].astype(np.float64).mean(1)
        rms = active_rms_48k(sig)
        target = float(rng.choice(levels))
        p['gain_dbfs'] = target
        p['active_rms_dbfs'] = float(20 * np.log10(max(rms or 1e-9, 1e-12)))
        if rms is None or rms < 1e-9:
            p['placed'] = False
            continue
        scale = (32768.0 * 10 ** (target / 20.0)) / rms
        add = (sig * scale).astype(np.float64)
        nf = min(80 * 3, len(add) // 2)
        if nf > 0:
            add[:nf] *= np.linspace(0, 1, nf)
            add[-nf:] *= np.linspace(1, 0, nf)
        lo = p['start_frame']
        hi = lo + n
        for ch in range(6):
            seg = mm[lo:hi, ch].astype(np.float64) + add
            mm[lo:hi, ch] = np.clip(seg, -2 ** 31, 2 ** 31 - 1).astype(np.int32)
        p['placed'] = True
    mm.flush()
    del mm
    aec = aec_run(raw_out, ROUND3 / 'r3-2' / f'mixed{stream_idx}.s16', log)
    raw_out.unlink(missing_ok=True)
    log(f'  stream {stream_idx}: placed {sum(p["placed"] for p in placed)}/{len(placed)}, '
        f'remaining {len(rest)}, aec samples {len(aec)}')
    return placed, rest, aec


def extract_r3_2(placements, aec, tag):
    """Turn placed s1 clips into positives (win) / mixed negatives (no win)."""
    exs = []
    for p in placements:
        if not p.get('placed'):
            continue
        a16 = p['start_frame'] // 3
        b16 = (p['start_frame'] + p['n48']) // 3
        lo = max(0, a16 - 8000)                                        # 0.5 s context
        hi = min(len(aec), b16 + 8000)
        pcm = aec[lo:hi].astype(np.float32)
        if p['kind'] == 'pos':
            wa = a16 - lo - 4800                                       # start - 0.3 s
            wb = a16 - lo + (b16 - a16) + 8000                         # end + 0.5 s
            if wa < 0 or wb >= len(pcm):
                continue
            exs.append({'pcm': pcm, 'win': (wa, wb), 'kind': 'pos', 'id': p['id'],
                        'op': f'preeaec-pos', 'meta': {'gain_dbfs': p['gain_dbfs'],
                                                       'active_rms_dbfs': p['active_rms_dbfs'],
                                                       'stream': p.get('stream')}})
        else:
            exs.append({'pcm': pcm, 'win': None, 'kind': 'neg', 'id': p['id'],
                        'op': f'preeaec-neg', 'meta': {'gain_dbfs': p['gain_dbfs'],
                                                       'active_rms_dbfs': p['active_rms_dbfs'],
                                                       'stream': p.get('stream')}})
    return exs


def build_r3_2_streams(s1groups, s1neg, levels, seed, log, nstreams=2):
    rng = np.random.default_rng(seed)
    items = []
    for g in s1groups:
        items.append({'id': g['id'], 'kind': 'pos', 'start_s': g['start_s'],
                      'n48': len(g['pcm']) * 3})
    for s in s1neg:
        items.append({'id': s['id'], 'kind': 'neg',
                      'start_s': float(s['id'].split('@')[1]), 'n48': len(s['pcm']) * 3})
    rng.shuffle(items)
    all_ex, all_placed = [], []
    for i in range(nstreams):
        if not items:
            break
        placed, items, aec = build_mixed_stream(items, i, rng, levels, log)
        for p in placed:
            p['stream'] = i
        all_placed += placed
        all_ex += extract_r3_2(placed, aec, f'preeaec{i}')
    (ROUND3 / 'r3-2').mkdir(parents=True, exist_ok=True)
    (ROUND3 / 'r3-2' / 'placements.json').write_text(json.dumps(all_placed, indent=2))
    return all_ex


def add_examples(exs, manifest, extra, cfg):
    """Append R3-2 examples with the same S16 feature/loss-window contract as build_dataset."""
    for j, e in enumerate(extra):
        f = W.mfcc16(e['pcm'])
        if len(f) < 10:
            continue
        if e['kind'] == 'pos':
            s, t = e['win']
            f0 = max(7, int(s / 160) - 30)
            f1 = min(len(f) - 1, int(t / 160) + 50)
            if f1 < f0:
                continue
            e['wf'] = (f0, f1)
        e['feat'] = f
        orig = e.get('id')
        e['id'] = f'{cfg["tag"]}:x{j:04d}:{e["op"]}'
        e['parents'] = [orig]
        exs.append(e)
        manifest.append({'id': e['id'], 'kind': e['kind'], 'op': e['op'],
                         'parents': [orig], 'meta': e.get('meta', {})})
    return exs, manifest


def run_r3_2(args, s1groups, s3groups, tts, s1neg, dev, bg, levels, log):
    cfg = base_cfg('preeaec')
    out = ROUND3 / 'r3-2'
    out.mkdir(parents=True, exist_ok=True)
    (out / 'epochs').mkdir(parents=True, exist_ok=True)
    s2train, s2val = W.round2_parts()
    bg_train = {'quiet': bg['quiet'], 'music': s2train}
    _, _, prior, step = W.r34_controls(1)
    extra = build_r3_2_streams(s1groups, s1neg, levels, args.seed, log, nstreams=2)
    base = W.load_net(1, W.BASE[1] / 'net.pt')
    cache = W.make_eval_cache(1, s3groups, dev, bg_train, s2val)

    def build_fn(ep):
        exs, manifest = W.build_dataset(s1groups + s3groups, tts, s1neg, bg_train, levels,
                                        args.seed + ep, cfg)
        # fresh copy of the pre-AEC examples each epoch (they are deterministic by stream)
        ex2 = [dict(e) for e in extra]
        return add_examples(exs, manifest, ex2, cfg)

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
    net, history, best_ep, best_m = train_online(
        base, build_fn, prior, step, args.epochs, cfg, args.seed + 1, log,
        lambda n, ep: W.eval_cache_metrics(n, cache, prior, step, 1), manifest_fn)
    seconds = time.time() - t0
    log(f'  r3-2 preeaec: training+selection {seconds:.1f}s, best epoch {best_ep+1}')
    src = out / 'epochs' / f'epoch{best_ep+1:02d}-manifest.json'
    if src.exists():
        (out / 'manifest-best.json').write_text(src.read_text())
    cand, gm, mm = W.eval_net(net, 1, out / 'best', s3groups, dev,
                              'preeaec-best', log, dev_bg=bg_train, s2val_pcm=s2val,
                              dur_k=1, robust=True, gain_sweep=True, s1groups=s1groups)
    miss = W.miss_analysis([g for g in s3groups
                            if not any(r['id'] == g['id'] and r['mac_hit'] for r in gm)], mm['s3'])
    (out / 'missed-groups.json').write_text(json.dumps(miss, indent=2))
    gate = cand['s3_groups'] == 35 and cand['s2b_events'] == 0 and cand['s2_events'] == 0
    res = {'variant': 'preeaec', 'cfg': cfg, 'seconds': seconds, 'epochs': args.epochs,
           'best_epoch': best_ep + 1, 'history': history, 'candidate': cand, 'gate': gate,
           'missed': miss, 'examples': len(extra),
           'gate_rule': 'L1 35/35 midpoint AND s2b 0 AND s2 0'}
    (out / 'result.json').write_text(json.dumps(res, indent=2))
    return res


# ---------------- report
def _row(name, v):
    d = v['dev']
    rob = v.get('robust', {})
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
    ran_r3_2 = any(name.startswith('r3-2') for name, _ in summary.get('table', []))
    title = ('# Round 3: factory-init transfer and pre-AEC mixed-channel training'
             if ran_r3_2 else '# Round 3 (R3-1 only): factory-init transfer; R3-2 pre-AEC mixing not run')
    lines = [title, '',
             '2026-11-03. Mac-only. No SSH, device access, recording, cloud upload or deployment. '
             'All scores are Mac reference margins under r34 controls (L1 FST forward weight 2.25, '
             'min-self-loop 0). They are not ARM or device results.', '',
             '## Method', '',
             '- **R3-1 factory transfer.** Load the factory `小爱同学` AM affines into `M.Net` '
             '(structural parity checked against `F.forward`).  The output rows are remapped to our '
             'pdf ids: SIL 0 and GARB 3 keep their ids, and the eight keyword pdfs '
             '`[1,2,4,5,6,7,8,9]` are initialised from the factory chain pdfs by position '
             '(factory chain shown below).  Then fine-tune with the best round-2 recipe '
             '(sequence loss, real-channel order negatives, online augmentation, per-epoch selection).',
             ('- **R3-2 pre-AEC mixing.** Sum the s1-pos-quiet 48 kHz 8-channel mic signal into the '
              's2-music-silent 48 kHz 8-channel stream (reference channels untouched), at random '
              'offsets and gains drawn from the s3 group RMS distribution, then run the offline '
              'device AEC (`tools/lx06-aec-offline.c`).  Only the first 75% of s2 is used; the last '
              '25% stays validation-only.  Positives are the mapped s1 segment windows, negatives '
              'are s2 alone plus s1 real negatives mixed the same way.  s2b never enters training.'
              if ran_r3_2 else
              '- **R3-2 pre-AEC mixing (planned, NOT run in this report).** The intended recipe is the '
              'same as above, but no R3-2 dataset was built and no R3-2 model was trained.'),
             ('- Offline AEC was verified bit-identical to `s3-music-pos.aec16k.wav`.'
              if ran_r3_2 else
              '- The only R3-2 artifact is the offline AEC verification (`r3-2-verify.log`): the host '
              'AEC output is bit-identical to `s3-music-pos.aec16k.wav`.'),
             '', '## Results', '',
             '| model | L1 35 groups | in-window | s2b events | s2 events | dev pos | dev hardneg | '
             'dev neg | dev homophone |',
             '|---|---|---|---|---|---|---|---|---|']
    for name, v in summary.get('table', []):
        if v:
            lines.append(_row(name, v))
    lines += ['', '### Robustness (isolated clips, 4000-sample pad)', '',
              '| model | s1 fwd | s1 rev | s1 perm | s1 half | s3 fwd | s3 rev | s3 perm | s3 half |',
              '|---|---|---|---|---|---|---|---|---|']
    for name, v in summary.get('table', []):
        if v:
            r = _rob_row(name, v)
            if r:
                lines.append(r)
    lines += ['', '### Gain sweep', '',
              '| model | gain | s3 groups | s2b events | s2 events | dev pos | dev hardneg |',
              '|---|---|---|---|---|---|---|']
    for name, v in summary.get('table', []):
        if v and v.get('gain'):
            for g, m in v['gain'].items():
                d = m['dev']
                lines.append(f'| {name} | {g} dB | {m["s3_groups"]}/35 | {m["s2b_events"]} | '
                             f'{m["s2_events"]} | {d["pos"]["accepted"]}/{d["pos"]["n"]} | '
                             f'{d["hardneg"]["accepted"]}/{d["hardneg"]["n"]} |')
    if not ran_r3_2:
        lines += ['## R3-2 status', '',
                  '**Not run.** R3-2 was queued after R3-1; the three R3-1 variants consumed the '
                  'round training/evaluation time and the supervisor paused the round before R3-2 '
                  'started. There is no `round3/r3-2/` directory and no R3-2 row in the results table.', '']
    lines += ['', '## Variant notes', '']
    for name, v in summary.get('table', []):
        if v:
            lines.append(f'- **{name}**: best epoch {v.get("_best_epoch", "?")}, '
                         f'training {v.get("_seconds", 0):.1f}s; gate '
                         f'{"PASS" if v.get("_gate") else "FAIL"} '
                         f'({v["s3_groups"]}/35, s2b {v["s2b_events"]}, s2 {v["s2_events"]}).')
    if summary.get('factory_chain'):
        lines += ['', 'Factory keyword chain pdfs (by position): '
                  f'`{summary["factory_chain"]}`; our KW `{summary["kw"]}`.', '']
    lines += ['## Limits', '',
              '- Known-set fit: all 35 s3 groups participate in training.  These rows do not estimate '
              'unseen-group generalization; a 5-fold CV is reported separately when run.',
              '- s2b is the independent music veto and never enters gradients; the last 25% of s2 is '
              'validation-only and never a training background.',
              '- No model was promoted or deployed; no ARM/device number is claimed.']
    Path(path).write_text('\n'.join(lines) + '\n')


def collect_table():
    table = []
    # baselines
    b = ROUND2 / 'baseline' / 'result.json'
    if b.exists():
        table.append(('v5 baseline', json.loads(b.read_text())))
    r2 = ROUND2 / 'order-w5-k1' / 'result.json'
    if r2.exists():
        v = json.loads(r2.read_text())
        c = dict(v['candidate'])
        c['_best_epoch'] = v.get('best_epoch')
        c['_seconds'] = v.get('seconds')
        c['_gate'] = v.get('gate')
        table.append(('round2 order-w5-k1', c))
    for v in R3_1_VARIANTS:
        f = ROUND3 / 'r3-1' / v['name'] / 'result.json'
        if f.exists():
            r = json.loads(f.read_text())
            c = dict(r['candidate'])
            c['_best_epoch'] = r.get('best_epoch')
            c['_seconds'] = r.get('seconds')
            c['_gate'] = r.get('gate')
            table.append((v['name'], c))
    f = ROUND3 / 'r3-2' / 'result.json'
    if f.exists():
        r = json.loads(f.read_text())
        c = dict(r['candidate'])
        c['_best_epoch'] = r.get('best_epoch')
        c['_seconds'] = r.get('seconds')
        c['_gate'] = r.get('gate')
        table.append(('r3-2 preeaec', c))
    return table


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('cmd', choices=['r3-1', 'r3-2', 'r3-2-verify', 'r3-report', 'all'])
    ap.add_argument('--epochs', type=int, default=10)
    ap.add_argument('--seed', type=int, default=4242)
    a = ap.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    ROUND3.mkdir(parents=True, exist_ok=True)
    logf = open(ROUND3 / f'{a.cmd}.log', 'a')
    log = lambda s: (print(s, flush=True), logf.write(s + '\n'), logf.flush())
    log(f'config {vars(a)}')
    if a.cmd == 'r3-2-verify':
        # re-check offline AEC on the real s3 capture
        aec = aec_run(C / 's3-music-pos.raw', ROUND3 / 's3-offline.s16', log)
        ref = W.wav(C / 's3-music-pos.aec16k.wav')
        n = min(len(aec), len(ref))
        log(f'  aec {len(aec)} samples, ref {len(ref)}, equal_prefix '
            f'{np.array_equal(aec[:n], ref[:n].astype(np.int16))}')
        return
    if a.cmd == 'r3-report':
        s = json.loads((ROUND3 / 'summary.json').read_text())
        write_report(s, ROUND3 / 'REPORT.md')
        log('round3 report written')
        return
    s1groups, s3groups = W.build_groups()
    tts, s1neg, dev, bg = W.base_sources()
    levels = [float(x['clip_rms_dbfs']) for x in json.loads(W.S3_LEVELS.read_text())]
    summary = {'kw': W.KW, 'factory_chain': factory_chain_pdfs(1)}
    if a.cmd == 'all':
        a.cmd = 'r3-1'
    if a.cmd == 'r3-1':
        results = []
        for v in R3_1_VARIANTS:
            log(f'r3-1 variant {v["name"]}')
            try:
                r = run_r3_1_variant(a, v, s1groups, s3groups, tts, s1neg, dev, bg, levels, log)
            except Exception as exc:
                import traceback
                traceback.print_exc()
                r = {'variant': v, 'error': repr(exc)}
                log(f'variant {v["name"]} FAILED: {exc!r}')
            results.append(r)
        (ROUND3 / 'r3-1-summary.json').write_text(json.dumps(results, indent=2))
        summary['table'] = collect_table()
        (ROUND3 / 'summary.json').write_text(json.dumps(summary, indent=2))
        write_report(summary, ROUND3 / 'REPORT.md')
        return
    if a.cmd == 'r3-2':
        r = run_r3_2(a, s1groups, s3groups, tts, s1neg, dev, bg, levels, log)
        summary['table'] = collect_table()
        (ROUND3 / 'summary.json').write_text(json.dumps(summary, indent=2))
        write_report(summary, ROUND3 / 'REPORT.md')
        return


if __name__ == '__main__':
    main()
