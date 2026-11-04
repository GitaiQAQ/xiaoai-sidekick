import urllib.request
import urllib.error
import json
from pathlib import Path

BASE = 'http://192.168.100.210'
rows = []


def request(path, body=None, headers=None):
    try:
        with urllib.request.urlopen(urllib.request.Request(BASE + path, data=body, headers=headers or {}), timeout=18) as r:
            return r.status, r.headers, r.read()
    except urllib.error.HTTPError as e:
        return e.code, e.headers, e.read()


code, headers, data = request('/api/v1/state')
s = json.loads(data)
assert code == 200 and s['voice']['state'] in {'stopped', 'standby', 'listening', 'thinking', 'speaking', 'connecting', 'followup', 'stopping', 'error'}
rows.append('state=pass real_device=1')
H = {'Content-Type': 'application/json', 'Origin': BASE, 'X-LX06-Token': s['csrf']}


def command(method, params=None):
    return request('/api/v1/command', json.dumps({'v': 1, 'id': 'qa-1', 'method': method, 'params': params or {}}).encode(), H)


for path in ['/', '/app.css', '/app.js', '/qrcode.js']:
    code, h, b = request(path)
    assert code == 200 and len(b) > 100
    rows.append('asset ' + path + '=pass bytes=' + str(len(b)))
code, h, b = request('/../../etc/passwd')
assert code == 404
rows.append('path_traversal=rejected')
code, h, b = command('unknown.method')
assert code == 400 and json.loads(b)['id'] == 'qa-1'
rows.append('unknown_method=rejected id_echo=pass')
code, h, b = request('/api/v1/command', b'{}', {'Origin': 'https://example.org', 'X-LX06-Token': s['csrf']})
assert code == 403
rows.append('cross_origin=rejected')
code, h, b = request('/api/v1/command', b'{}', {'Origin': BASE, 'X-LX06-Token': 'wrong'})
assert code == 403
rows.append('wrong_token=rejected')
code, h, b = request('/', headers={'Host': 'xiaoai.local'})
assert code == 200
rows.append('mdns_host=accepted')
code, h, b = request('/', headers={'Host': 'doubaodoubao.local'})
assert code == 200
rows.append('mdns_alias_host=accepted')
code, h, b = request('/', headers={'Host': 'xiaoai.local.evil.example'})
assert code == 403
rows.append('mdns_host_lookalike=rejected')
code, h, b = request('/', headers={'Host': 'doubaodoubao.local.evil.example'})
assert code == 403
rows.append('mdns_alias_lookalike=rejected')
H_MDNS = {'Content-Type': 'application/json', 'Origin': 'http://xiaoai.local', 'X-LX06-Token': s['csrf']}
code, h, b = request('/api/v1/command', json.dumps({'v': 1, 'id': 'qa-mdns', 'method': 'unknown.method', 'params': {}}).encode(), H_MDNS)
assert code == 400 and json.loads(b)['id'] == 'qa-mdns'
rows.append('mdns_origin=accepted')
H_ALIAS = {'Content-Type': 'application/json', 'Origin': 'http://doubaodoubao.local', 'X-LX06-Token': s['csrf']}
code, h, b = request('/api/v1/command', json.dumps({'v': 1, 'id': 'qa-alias', 'method': 'unknown.method', 'params': {}}).encode(), H_ALIAS)
assert code == 400 and json.loads(b)['id'] == 'qa-alias'
rows.append('mdns_alias_origin=accepted')
code, h, b = request('/api/v1/events')
assert code == 200 and h['Content-Type'].startswith('text/event-stream') and b'event: state' in b
rows.append('sse_snapshot=pass')
if not s['netease'].get('session_saved'):
    code, h, b = command('accounts.netease.login.start')
    r = json.loads(b)['result']
    assert code == 200 and r['ok'] and r['url'].startswith('https://music.163.com/login?codekey=')
    rows.append('netease_qr_generation=pass')
else:
    rows.append('netease_qr_generation=skipped_preserve_existing_login')
assert not any(x in data for x in [b'MUSIC_U', b'api_key', b'cookie'])
rows.append('credential_fields_not_exposed=pass')
Path(__file__).resolve().parents[1].joinpath('logs/web-http-validation.txt').write_text('\n'.join(rows) + '\n', encoding='utf-8')
print('\n'.join(rows))
