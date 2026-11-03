"""Generate a Chinese wake-word TTS corpus (16 kHz mono PCM16 WAV).

Usage:
  tools/tts-corpus.py --labels pos,hardneg,neg --clean
  tools/tts-corpus.py --qc --asr-sample 50
  tools/tts-corpus.py --labels pos --limit 20      # smoke test

Engines: macOS `say`, edge-tts (online Microsoft voices), and offline
sherpa-onnx VITS/Kokoro/Matcha/Piper models. Needs vendor/tts-venv and
vendor/tts-models. Writes temp/analysis/tts-corpus/{pos,hardneg,neg} plus
manifest.csv. Voice/speaker lists are fixed here for reproducibility.
Text is sent to Microsoft (edge-tts) and Google (gTTS); all other engines are offline.
"""
from pathlib import Path
import argparse, asyncio, csv, math, random, shutil, subprocess, sys, tempfile
import numpy as np, soundfile as sf
import scipy.signal as ss

R = Path(__file__).resolve().parents[1]
OUT = R / 'temp/analysis/tts-corpus'
MD = R / 'vendor/tts-models'
PY = R / 'vendor/tts-venv/bin/python'

POS_TEXTS = [
    '豆包豆包', '豆包，豆包', '豆包豆包！', '豆包豆包？',
    '豆包豆包，今天天气怎么样', '豆包豆包，放首歌', '豆包豆包，几点了', '嗯，豆包豆包',
]
HARDNEG_TEXTS = [
    '豆包', '豆豆', '包包', '宝宝', '抱抱', '豆腐豆腐', '豆瓣豆瓣', '逗包逗包',
    '都饱都饱', '豆沙包', '土豆包', '肉包豆包', '我想吃豆包', '豆包好吃吗', '包豆包豆',
    '小爱同学', '小度小度', '天猫精灵', '嘿 Siri', '斗宝斗宝', '刀疤刀疤', '多宝多宝',
    '头包头包', '豆包豆沙包', '豆豆包', '包豆包', '豆包包子', '豆包豆浆', '豆包豆奶',
    '都包都包', '斗包斗包', '逗宝逗宝', '豆宝豆宝', '多包多包', '岛包岛包',
]
NEG_TEXTS = [
    '今天天气不错，适合出门走走。', '请帮我查一下明天的天气预报。', '现在几点了？',
    '我想听一首轻松的歌。', '打开客厅的灯。', '把音量调小一点。', '今天中午吃什么好呢？',
    '给我讲一个有趣的故事。', '明天早上七点叫我起床。', '附近有什么好吃的餐厅？',
    '这条路怎么走？', '我有点累了，想休息一会儿。', '请播放一段轻音乐。',
    '今天的新闻有什么？', '帮我记一下，下午三点开会。', '这个多少钱？',
    '我想买一张火车票。', '手机没电了，充电器在哪？', '周末去公园散步吧。',
    '外面好像要下雨了。', '请把窗户关上。', '你觉得这个主意怎么样？',
    '我们一起吃晚饭吧。', '他今天看起来很开心。', '这道菜味道真不错。',
    '请问洗手间在哪里？', '我需要一杯热水。', '公交车还有多久到？',
    '孩子们在操场上玩耍。', '夜晚的城市灯火通明。',
    '春天的花开得真漂亮。', '请慢一点说，我没听清。', '我的名字叫小明。',
    '今天是星期几？', '我们下午再聊。',
]
TEXTS = {'pos': POS_TEXTS, 'hardneg': HARDNEG_TEXTS, 'neg': NEG_TEXTS}
POS_SPEED = [0.85, 1.0, 1.15, 1.0, 1.0, 0.9, 1.1, 0.95]
PITCH_HZ = ['+0Hz', '-15Hz', '+15Hz']
PBAS = [50, 40, 65]

# Selected speakers, chosen after an F0/ASR probe (see report). sid = model speaker id.
AISHELL3_SIDS = [119, 46, 167, 110, 75, 131, 81, 138, 6, 41, 114, 141, 99, 111, 122, 159, 28, 116, 44, 105]
KOKORO_SIDS = [63, 60, 102, 88, 92, 13, 57, 21, 28, 18, 38, 44, 41]
FANCHEN_SIDS = [124, 40, 75, 131, 77, 133, 55, 78, 109, 73, 155, 34, 8]
ZHLL_SIDS = [0, 1, 2, 3, 4]
KOKORO_NAMES = {63: 'zm_014', 60: 'zm_011', 102: 'zm_100', 88: 'zm_064', 92: 'zm_069',
                13: 'zf_019', 57: 'zf_099', 21: 'zf_032', 28: 'zf_044', 18: 'zf_026',
                38: 'zf_071', 44: 'zf_077', 41: 'zf_074'}

GTT_VOICES = [('gtts-zh', 'zh-CN', 'female', 'neutral')]
SAY_VOICES = [
    ('tingting', 'Tingting', 'zh_CN', 'female', 'neutral'),
    ('meijia', 'Meijia', 'zh_TW', 'female', 'neutral'),
    ('sinji', 'Sinji', 'zh_HK', 'female', 'neutral'),
]
EDGE_VOICES = [
    ('xiaoxiao', 'zh-CN-XiaoxiaoNeural', 'zh_CN', 'female', 'warm'),
    ('xiaoyi', 'zh-CN-XiaoyiNeural', 'zh_CN', 'female', 'lively'),
    ('yunjian', 'zh-CN-YunjianNeural', 'zh_CN', 'male', 'passionate'),
    ('yunxi', 'zh-CN-YunxiNeural', 'zh_CN', 'male', 'lively'),
    ('yunxia', 'zh-CN-YunxiaNeural', 'zh_CN', 'male', 'cute'),
    ('yunyang', 'zh-CN-YunyangNeural', 'zh_CN', 'male', 'news'),
    ('xiaobei', 'zh-CN-liaoning-XiaobeiNeural', 'zh_CN', 'female', 'dialect'),
    ('xiaoni', 'zh-CN-shaanxi-XiaoniNeural', 'zh_CN', 'female', 'dialect'),
    ('hiugaai', 'zh-HK-HiuGaaiNeural', 'zh_HK', 'female', 'friendly'),
    ('hiumaan', 'zh-HK-HiuMaanNeural', 'zh_HK', 'female', 'friendly'),
    ('wanlung', 'zh-HK-WanLungNeural', 'zh_HK', 'male', 'friendly'),
    ('hsiaochen', 'zh-TW-HsiaoChenNeural', 'zh_TW', 'female', 'friendly'),
    ('hsiaoyu', 'zh-TW-HsiaoYuNeural', 'zh_TW', 'female', 'friendly'),
    ('yunjhe', 'zh-TW-YunJheNeural', 'zh_TW', 'male', 'friendly'),
]
# engine -> (relative model dir, kind, license note)
MODELS = {
    'sherpa-aishell3': ('vits-zh-aishell3', 'vits'),
    'sherpa-kokoro': ('kokoro-int8-multi-lang-v1_1', 'kokoro'),
    'sherpa-fanchen': ('vits-zh-hf-fanchen-C', 'vits'),
    'sherpa-zhll': ('sherpa-onnx-vits-zh-ll', 'vits'),
    'sherpa-melo': ('vits-melo-tts-zh_en', 'vits'),
    'sherpa-matcha': ('matcha-icefall-zh-baker', 'matcha'),
    'sherpa-cantonese': ('vits-cantonese-hf-xiaomaiiwn', 'vits'),
    'sherpa-chaowen': ('vits-piper-zh_CN-chaowen-medium', 'vits'),
    'sherpa-xiaoya': ('vits-piper-zh_CN-xiao_ya-medium', 'vits'),
    'sherpa-laozhe': ('vits-zh-hf-fanchen-ZhiHuiLaoZhe', 'vits'),
}
LOCAL_VOICES = {
    'sherpa-aishell3': [(f'aishell3-{s:03d}', s, 'neutral') for s in AISHELL3_SIDS],
    'sherpa-kokoro': [(KOKORO_NAMES[s], s, 'neutral') for s in KOKORO_SIDS],
    'sherpa-fanchen': [(f'fanchen-{s:03d}', s, 'neutral') for s in FANCHEN_SIDS],
    'sherpa-zhll': [(f'zhll-{s}', s, 'neutral') for s in ZHLL_SIDS],
    'sherpa-melo': [('melo', 0, 'neutral')],
    'sherpa-matcha': [('matcha', 0, 'neutral')],
    'sherpa-cantonese': [('cantonese', 0, 'cantonese')],
    'sherpa-chaowen': [('chaowen', 0, 'neutral')],
    'sherpa-xiaoya': [('xiaoya', 0, 'child')],
    'sherpa-laozhe': [('laozhe', 0, 'elderly')],
}
# label -> list of (engine, voice, sid) entries. Same voice may appear in several labels.
SINGLES = [('sherpa-matcha', 'matcha', 0), ('sherpa-chaowen', 'chaowen', 0),
           ('sherpa-xiaoya', 'xiaoya', 0)]
POS_VOICES = (
    [('sherpa-aishell3', v, s) for v, s, _ in LOCAL_VOICES['sherpa-aishell3']] +
    [('sherpa-kokoro', v, s) for v, s, _ in LOCAL_VOICES['sherpa-kokoro']] +
    [('sherpa-fanchen', v, s) for v, s, _ in LOCAL_VOICES['sherpa-fanchen']] +
    SINGLES +
    [('edge', v, None) for v, _, _, _, _ in EDGE_VOICES] +
    [('say', v, None) for v, _, _, _, _ in SAY_VOICES] +
    [('gtts', v, None) for v, _, _, _ in GTT_VOICES]
)
HARDNEG_VOICES = list(POS_VOICES)
NEG_VOICES = (
    [('sherpa-aishell3', v, s) for v, s, _ in LOCAL_VOICES['sherpa-aishell3'][:3]] +
    [('sherpa-kokoro', v, s) for v, s, _ in LOCAL_VOICES['sherpa-kokoro'][:3]] +
    [('sherpa-fanchen', v, s) for v, s, _ in LOCAL_VOICES['sherpa-fanchen'][:2]] +
    [('sherpa-zhll', 'zhll-0', 0), ('sherpa-melo', 'melo', 0)] + SINGLES +
    [('edge', v, None) for v, _, _, _, _ in EDGE_VOICES[:2]] +
    [('say', 'tingting', None)] + [('gtts', 'gtts-zh', None)]
)
LABEL_VOICES = {'pos': POS_VOICES, 'hardneg': HARDNEG_VOICES, 'neg': NEG_VOICES}


def style_of(engine, voice):
    if engine == 'gtts': return 'neutral'
    for v, _, _, _, st in EDGE_VOICES:
        if engine == 'edge' and v == voice: return st
    for v, _, _, _, st in SAY_VOICES:
        if engine == 'say' and v == voice: return st
    for v, s, st in LOCAL_VOICES.get(engine, []):
        if v == voice: return st
    return 'neutral'


def build_tts(engine):
    import sherpa_onnx as so
    d, kind = MODELS[engine]; p = MD / d
    if kind == 'kokoro':
        cfg = so.OfflineTtsKokoroModelConfig(
            model=str(p/'model.int8.onnx'), voices=str(p/'voices.bin'),
            tokens=str(p/'tokens.txt'), data_dir=str(p/'espeak-ng-data'),
            lexicon=str(p/'lexicon-zh.txt'))
        m = so.OfflineTtsModelConfig(kokoro=cfg, num_threads=4)
    elif kind == 'matcha':
        cfg = so.OfflineTtsMatchaModelConfig(
            acoustic_model=str(p/'model-steps-3.onnx'),
            vocoder=str(MD/'asr/vocos-22khz-univ.onnx'),
            lexicon=str(p/'lexicon.txt'), tokens=str(p/'tokens.txt'),
            data_dir='', dict_dir=str(p/'dict'))
        m = so.OfflineTtsModelConfig(matcha=cfg, num_threads=4)
    else:
        cfg = so.OfflineTtsVitsModelConfig(
            model=str(p/next(x for x in p.glob('*.onnx') if 'int8' not in x.name)),
            lexicon=str(p/'lexicon.txt') if (p/'lexicon.txt').exists() else '',
            tokens=str(p/'tokens.txt'),
            data_dir=str(p/'espeak-ng-data') if (p/'espeak-ng-data').exists() else '',
            dict_dir=str(p/'dict') if (p/'dict').exists() else '')
        m = so.OfflineTtsModelConfig(vits=cfg, num_threads=4)
    return so.OfflineTts(so.OfflineTtsConfig(model=m, max_num_sentences=1))


def finish(x, sr):
    """Resample, trim, normalize, fade. Returns float32 16 kHz samples or None."""
    x = np.asarray(x, dtype=np.float32)
    if sr != 16000: x = ss.resample_poly(x, 16000, sr).astype(np.float32)
    if len(x) < 100: return None
    x = x - x.mean()
    peak = float(np.abs(x).max())
    if peak < 1e-6: return None
    th = max(1e-4, 0.02*peak)
    idx = np.where(np.abs(x) > th)[0]
    if not len(idx): return None
    a = max(0, idx[0]-480); b = min(len(x), idx[-1]+480); x = x[a:b]
    rms = float(np.sqrt(np.mean(x*x)))
    if rms < 1e-5: return None
    x = x * (0.05/rms)
    peak = float(np.abs(x).max())
    if peak > 0.95: x *= 0.95/peak
    n = 160
    if len(x) > 2*n:
        x[:n] *= np.linspace(0, 1, n); x[-n:] *= np.linspace(1, 0, n)
    return x


def validate(path, label, text):
    try:
        info = sf.info(path)
        ok = info.samplerate == 16000 and info.channels == 1 and info.subtype == 'PCM_16'
        x, _ = sf.read(path, dtype='float32')
        dur = len(x)/16000.0
        rms = float(np.sqrt(np.mean(x*x))) if len(x) else 0
        rms_db = 20*math.log10(max(rms, 1e-9))
        clip = float(np.mean(np.abs(x) >= 0.999)) if len(x) else 1
        if label == 'pos':
            lo, hi = (0.5, 4.5) if text in POS_TEXTS[:5] else (0.8, 10)
        elif label == 'hardneg': lo, hi = 0.4, 7
        else: lo, hi = 0.8, 15
        ok = ok and lo <= dur <= hi and rms_db > -45 and clip < 0.01
        return ok, dur, rms_db
    except Exception:
        return False, 0, -999


def item_specs(label, limit=0):
    texts = TEXTS[label]
    for i, text in enumerate(texts):
        if limit and i >= limit: break
        if label == 'pos': speed = POS_SPEED[i % len(POS_SPEED)]
        else: speed = [0.9, 1.0, 1.1][i % 3]
        yield i, text, speed, i % 3


def record(label, engine, voice, i, text, speed, pidx, rate, pitch, path, stats):
    ok, dur, rms = validate(path, label, text)
    if not ok:
        stats['bad'] += 1
        rej = OUT/'rejected'; rej.mkdir(parents=True, exist_ok=True)
        try: shutil.move(str(path), str(rej/path.name))
        except Exception: pass
        return None
    stats['ok'] += 1
    return {'file': f'{label}/{path.name}', 'label': label, 'text': text,
            'engine': engine, 'voice': voice, 'rate': rate, 'pitch': pitch,
            'style': style_of(engine, voice), 'duration_s': f'{dur:.3f}',
            'rms_dbfs': f'{rms:.1f}'}


def name(label, engine, voice, speed, pidx, i):
    return f'{label}-{engine}-{voice}-r{round(speed*100):03d}p{pidx}-{i+1:02d}.wav'


def gen_local(engine, voice, sid, label, stats, rows, tts, limit=0):
    d = OUT/label
    for i, text, speed, pidx in item_specs(label, limit):
        out = d/name(label, engine, voice, speed, pidx, i)
        try:
            a = tts.generate(text, sid=sid, speed=speed)
            x = finish(a.samples, a.sample_rate)
            if x is None: stats['bad'] += 1; continue
            sf.write(out, x, 16000, subtype='PCM_16')
            r = record(label, engine, voice, i, text, speed, pidx,
                       f'{speed:.2f}x', '0', out, stats)
            if r: rows.append(r)
        except Exception as e:
            stats['err'] += 1; print(f'  ! {engine}/{voice}: {e}', file=sys.stderr)


def gen_say(voice, label, stats, rows, limit=0):
    vname = dict((v, n) for v, n, _, _, _ in SAY_VOICES)[voice]
    for i, text, speed, pidx in item_specs(label, limit):
        out = OUT/label/name(label, 'say', voice, speed, pidx, i)
        t = f'[[pbas {PBAS[pidx]}]]{text}' if pidx else text
        cmd = ['say', '-v', vname, '-r', str(int(180*speed)),
               '--data-format=LEI16@16000', '-o', str(out), t]
        try:
            subprocess.run(cmd, check=True, capture_output=True)
            x, _ = sf.read(out, dtype='float32')
            x = finish(x, 16000)
            if x is None: stats['bad'] += 1; continue
            sf.write(out, x, 16000, subtype='PCM_16')
            r = record(label, 'say', voice, i, text, speed, pidx,
                       f'{int(180*speed)}wpm', f'pbas{PBAS[pidx]}', out, stats)
            if r: rows.append(r)
        except Exception as e:
            stats['err'] += 1; print(f'  ! say/{voice}: {e}', file=sys.stderr)


def gen_gtts(voice, label, stats, rows, limit=0):
    from gtts import gTTS
    for i, text, speed, pidx in item_specs(label, limit):
        out = OUT/label/name(label, 'gtts', voice, speed, pidx, i)
        mp3 = out.with_suffix('.mp3')
        try:
            gTTS(text, lang='zh-CN', slow=speed < 0.9).save(str(mp3))
            subprocess.run(['afconvert', '-f', 'WAVE', '-d', 'LEI16@16000', '-c', '1',
                            str(mp3), str(out)], check=True, capture_output=True)
            x, _ = sf.read(out, dtype='float32')
            x = finish(x, 16000)
            if x is None: stats['bad'] += 1; continue
            sf.write(out, x, 16000, subtype='PCM_16')
            r = record(label, 'gtts', voice, i, text, speed, pidx,
                       '0.7x-slow' if speed < 0.9 else '1.0x', '0', out, stats)
            if r: rows.append(r)
        except Exception as e:
            stats['err'] += 1; print(f'  ! gtts/{voice}: {e}', file=sys.stderr)
        finally:
            mp3.unlink(missing_ok=True)


async def gen_edge(voice, label, stats, rows, limit=0):
    import edge_tts
    vid = dict((v, i) for v, i, _, _, _ in EDGE_VOICES)[voice]
    sem = asyncio.Semaphore(4)
    async def one(i, text, speed, pidx):
        out = OUT/label/name(label, 'edge', voice, speed, pidx, i)
        mp3 = out.with_suffix('.mp3')
        rate = f'{(speed-1)*100:+.0f}%'
        async with sem:
            for attempt in range(3):
                try:
                    await edge_tts.Communicate(text, vid, rate=rate, pitch=PITCH_HZ[pidx]).save(str(mp3))
                    subprocess.run(['afconvert', '-f', 'WAVE', '-d', 'LEI16@16000', '-c', '1',
                                    str(mp3), str(out)], check=True, capture_output=True)
                    x, _ = sf.read(out, dtype='float32')
                    x = finish(x, 16000)
                    if x is None: stats['bad'] += 1; return
                    sf.write(out, x, 16000, subtype='PCM_16')
                    r = record(label, 'edge', voice, i, text, speed, pidx, rate, PITCH_HZ[pidx], out, stats)
                    if r: rows.append(r)
                    return
                except Exception as e:
                    if attempt == 2:
                        stats['err'] += 1; print(f'  ! edge/{voice}: {e}', file=sys.stderr)
                    await asyncio.sleep(1)
    await asyncio.gather(*[one(*s) for s in item_specs(label, limit)])
    for p in (OUT/label).glob(f'{label}-edge-*.mp3'):
        p.unlink(missing_ok=True)


def run(labels, limit, engine_filter='', append=False):
    rows = []; stats = {'ok': 0, 'bad': 0, 'err': 0}; cache = {}
    manifest = OUT/'manifest.csv'
    if append and manifest.exists():
        rows = list(csv.DictReader(open(manifest, encoding='utf-8')))
    for label in labels:
        (OUT/label).mkdir(parents=True, exist_ok=True)
        for engine, voice, sid in LABEL_VOICES[label]:
            if engine_filter and engine != engine_filter: continue
            print(f'[{label}] {engine}/{voice}', flush=True)
            if engine == 'say': gen_say(voice, label, stats, rows, limit)
            elif engine == 'edge': asyncio.run(gen_edge(voice, label, stats, rows, limit))
            elif engine == 'gtts': gen_gtts(voice, label, stats, rows, limit)
            else:
                if engine not in cache: cache[engine] = build_tts(engine)
                gen_local(engine, voice, sid, label, stats, rows, cache[engine], limit)
    seen = {}
    for r in rows: seen[r['file']] = r
    rows = list(seen.values())
    with open(manifest, 'w', newline='', encoding='utf-8') as f:
        w = csv.DictWriter(f, fieldnames=['file', 'label', 'text', 'engine', 'voice',
                                          'rate', 'pitch', 'style', 'duration_s', 'rms_dbfs'])
        w.writeheader(); w.writerows(rows)
    print(f'wrote {len(rows)} files, rejected {stats["bad"]}, errors {stats["err"]}: {manifest}')


def qc():
    manifest = OUT/'manifest.csv'
    rows = list(csv.DictReader(open(manifest, encoding='utf-8')))
    good, bad = [], []
    for r in rows:
        p = OUT/r['file']
        ok, dur, rms = validate(p, r['label'], r['text'])
        if ok:
            r['duration_s'] = f'{dur:.3f}'; r['rms_dbfs'] = f'{rms:.1f}'; good.append(r)
        else:
            bad.append(r['file'])
    with open(manifest, 'w', newline='', encoding='utf-8') as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0].keys())); w.writeheader(); w.writerows(good)
    print(f'qc: {len(good)} ok, {len(bad)} dropped')
    for p in bad: print('  drop', p)


def asr_sample(n):
    import sherpa_onnx as so
    from difflib import SequenceMatcher
    p = MD/'asr/sherpa-onnx-paraformer-zh-small-2024-03-09'
    if not (p/'model.int8.onnx').exists():
        print('asr: model missing, skipped'); return
    rec = so.OfflineRecognizer.from_paraformer(
        paraformer=str(p/'model.int8.onnx'), tokens=str(p/'tokens.txt'),
        num_threads=4, provider='cpu')
    def transcribe(path):
        x, sr = sf.read(path, dtype='float32')
        if sr != 16000: x = ss.resample_poly(x, 16000, sr).astype(np.float32)
        s = rec.create_stream(); s.accept_waveform(16000, x); rec.decode_stream(s)
        return s.result.text.replace(' ', '')
    rows = list(csv.DictReader(open(OUT/'manifest.csv', encoding='utf-8')))
    random.seed(20261101)
    for label in ('pos', 'hardneg', 'neg'):
        sub = [r for r in rows if r['label'] == label]
        if not sub: continue
        sample = random.sample(sub, min(n, len(sub)))
        hits = 0
        for r in sample:
            hyp = transcribe(OUT/r['file'])
            if label == 'pos':
                hits += ('豆包豆包' in hyp) or SequenceMatcher(None, hyp, '豆包豆包').ratio() >= 0.75
            else:
                hits += ('豆包豆包' not in hyp)
        print(f'asr {label}: {hits}/{len(sample)} correct ({hits/len(sample):.1%})')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--labels', default='pos,hardneg,neg')
    ap.add_argument('--limit', type=int, default=0)
    ap.add_argument('--clean', action='store_true')
    ap.add_argument('--qc', action='store_true')
    ap.add_argument('--asr-sample', type=int, default=0)
    ap.add_argument('--engine', default='')
    ap.add_argument('--append', action='store_true')
    a = ap.parse_args()
    if a.clean:
        for d in ('pos', 'hardneg', 'neg', 'rejected'):
            shutil.rmtree(OUT/d, ignore_errors=True)
        (OUT/'manifest.csv').unlink(missing_ok=True)
    if not a.qc:
        run([x for x in a.labels.split(',') if x], a.limit, a.engine, a.append)
    else:
        qc()
    if a.asr_sample: asr_sample(a.asr_sample)


if __name__ == '__main__':
    main()
