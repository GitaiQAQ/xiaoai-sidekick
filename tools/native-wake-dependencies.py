"""Find local firmware owners of the native wake engine's undeclared imports."""
from pathlib import Path
import json
from elftools.elf.elffile import ELFFile

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / 'temp/rootfs-factory-system0'
owners = {}
with (BASE / 'usr/lib/libxaudio_engine.so').open('rb') as handle:
    elf = ELFFile(handle)
    imports = {
        s.name for s in elf.get_section_by_name('.dynsym').iter_symbols()
        if s['st_shndx'] == 'SHN_UNDEF' and s.name
        and s['st_info']['bind'] != 'STB_WEAK'
    }
for directory in ['lib', 'usr/lib', 'usr/bin']:
    for path in (BASE / directory).iterdir():
        if not path.is_file():
            continue
        with path.open('rb') as handle:
            if handle.read(4) != b'\x7fELF':
                continue
            handle.seek(0)
            elf = ELFFile(handle)
            table = elf.get_section_by_name('.dynsym')
            if table is None:
                continue
            for symbol in table.iter_symbols():
                if symbol.name in imports and symbol['st_shndx'] != 'SHN_UNDEF':
                    owners.setdefault(symbol.name, []).append(str(path.relative_to(BASE)))
output = ROOT / 'temp/analysis/native-wake-import-owners.json'
output.write_text(json.dumps(owners, indent=2), encoding='utf-8')
for name in ['WebRtcSpl_Resample48khzTo16khz', 'WebRtcSpl_ResetResample48khzTo16khz',
             'snd_pcm_hw_params_any', 'pthread_detach', '_Znwj']:
    print(name, owners.get(name, []))
