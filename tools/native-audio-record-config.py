from pathlib import Path
import struct,json
r=Path(__file__).resolve().parents[1];p=r/'private/inputs/live/live-audio-record-config.bin'
b=p.read_bytes()
if len(b)!=32:raise RuntimeError(f'Expected 32 configuration bytes, received {len(b)}')
keys=['capture_sample_bytes','capture_rate','capture_channels','capture_alsa_format','output_sample_bytes','output_rate','output_channels','output_alsa_format']
d={'source':'mipns libxaudio AudioRecord global control fields; no audio samples','elf_global_base':'0x4254e0','fields':dict(zip(keys,struct.unpack('<8I',b)))}
(r/'private/inputs/live/live-audio-record-config.json').write_text(json.dumps(d,indent=2),encoding='utf-8')
print(json.dumps(d,indent=2))
