"""Read-only chart checks on the device. Does not change playback or login."""
import json
import urllib.request
import urllib.error
import os
from pathlib import Path

BASE = os.environ.get('LX06_BASE_URL', '').rstrip('/')
if not BASE.startswith(('http://', 'https://')):
    raise SystemExit('Set LX06_BASE_URL to the speaker console URL, for example http://<speaker-ip>')
s = json.load(urllib.request.urlopen(BASE + '/api/v1/state', timeout=15))
headers = {'Content-Type': 'application/json', 'Origin': BASE, 'X-LX06-Token': s['csrf']}

def command(method, params=None):
    data = json.dumps({'v': 1, 'id': 'charts-qa', 'method': method, 'params': params or {}}).encode()
    try:
        with urllib.request.urlopen(urllib.request.Request(BASE + '/api/v1/command', data=data, headers=headers), timeout=22) as r:
            return r.status, json.load(r)
    except urllib.error.HTTPError as e:
        return e.code, json.load(e)

code, envelope = command('music.netease.charts')
charts = envelope['result']
assert code == 200 and charts['ok'] and 1 <= len(charts['items']) <= 80
hot = next(x for x in charts['items'] if x['name'] == '热歌榜')
code, envelope = command('music.netease.chart.tracks', {'id': hot['id']})
tracks = envelope['result']
assert code == 200 and tracks['ok'] and 1 <= len(tracks['items']) <= tracks['limit']
for rank, item in enumerate(tracks['items'], 1):
    assert item['id'].isdigit() and item['title'] and item['rank'] == rank
code, _ = command('music.netease.chart.tracks', {'id': '../bad'})
assert code == 400
code, envelope = command('music.netease.prepare', {'id': tracks['items'][0]['id']})
assert code == 400  # private preparation is unavailable from LAN
code, _ = command('music.netease.commit', {'ticket': '0'*32})
assert code == 400
rows = [f'charts=pass count={len(charts["items"])}', f'chart_tracks=pass count={len(tracks["items"])}',
        'invalid_chart_id=rejected', 'private_prepare_commit_from_lan=rejected',
        'playback_and_login=unchanged_by_test']
Path(__file__).resolve().parents[1].joinpath('logs/netease-charts-validation.txt').write_text('\n'.join(rows) + '\n', encoding='utf-8')
print('\n'.join(rows))
