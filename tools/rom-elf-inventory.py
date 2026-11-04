#!/usr/bin/env python3
"""Inventory extracted ROM ELF metadata without loading or executing ROM code.

Uses only the Python standard library. Prints names and dependency metadata,
never arbitrary string-table contents, configuration values or device data.
"""
import argparse
import json
import struct
from collections import defaultdict
from pathlib import Path


def markdown(result):
    records = result["files"]
    lines = ["# Factory ROM ELF dependency inventory", "",
             "Generated from the local extracted factory ROM by `tools/rom-elf-inventory.py`.",
             "This is a static artifact census, not a runtime loader or product-feature completion report.", "",
             "Regenerate: `python3 tools/rom-elf-inventory.py temp/rootfs-factory-full --format markdown --output .agents/notes/reports/factory-rom-elf-inventory.md`", "",
             "Scope: " + result["scope"] + ".", "",
             "Limits: " + result["limits"], "",
             f"ELF files: {len(records)}. Parse errors: {len(result['errors'])}. "
             f"DT_NEEDED entries: {sum(len(row['needed']) for row in records)}.", "",
             "## Dependency names without an extracted provider", ""]
    for name, users in result["unresolved_needed"].items():
        lines.append(f"- `{name}`: " + ", ".join(f"`{path}`" for path in users))
    if not result["unresolved_needed"]:
        lines.append("None in this static scope.")
    lines += ["", "A missing provider can reflect absent alias metadata or a separately supplied object; "
              "it does not prove runtime failure. A candidate provider is matched by basename or SONAME "
              "without resolving actual loader paths, architecture compatibility or symbol versions.", "",
              "## Per-file metadata", "",
              "| Path | ELF type / architecture | Entry | SONAME | Interpreter | DT_NEEDED |",
              "| --- | --- | --- | --- | --- | --- |"]
    for row in records:
        needed = ", ".join(f"`{name}`" for name in row["needed"]) or "—"
        lines.append(f"| `{row['path']}` | {row['type']} / {row['machine']} {row['bits']} | {row['entry']} | "
                     f"{row['soname'] or '—'} | {row['interpreter'] or '—'} | {needed} |")
    lines += ["", "## Candidate provider map", "",
              "| Dependency name | Extracted candidates |", "| --- | --- |"]
    candidates = {}
    for row in records:
        candidates.update(row["dependency_candidates"])
    for name, paths in sorted(candidates.items()):
        lines.append(f"| `{name}` | " + (", ".join(f"`{path}`" for path in paths) or "unresolved") + " |")
    if result["errors"]:
        lines += ["", "## Parse errors", ""]
        lines.extend(f"- `{item['path']}`: {item['error']}" for item in result["errors"])
    return "\n".join(lines) + "\n"


def elf_metadata(path):
    data = path.read_bytes()
    if data[:4] != b"\x7fELF":
        return None
    bits, byte_order = data[4:6]
    if bits not in (1, 2) or byte_order not in (1, 2):
        raise ValueError("unsupported ELF identification")
    order = "<" if byte_order == 1 else ">"
    hdr = struct.unpack_from(order + ("HHIIIIIHHHHHH" if bits == 1 else "HHIQQQIHHHHHH"), data, 16)
    kind, machine = hdr[:2]
    phoff, shoff, phsize, phnum, shsize, shnum = hdr[4], hdr[5], hdr[8], hdr[9], hdr[10], hdr[11]
    sections = []
    for i in range(shnum):
        sections.append(struct.unpack_from(order + ("10I" if bits == 1 else "IIQQQQIIQQ"), data, shoff + i * shsize))

    def string(section, offset):
        start = section[4] + offset
        end = data.find(b"\0", start, section[4] + section[5])
        if not 0 <= offset < section[5] or end < 0:
            raise ValueError("invalid dynamic string offset")
        return data[start:end].decode("utf-8", errors="replace")

    needed, soname, paths = [], None, []
    for sec in sections:
        if sec[1] != 6:  # SHT_DYNAMIC
            continue
        strings = sections[sec[6]]
        stride = sec[9] or (8 if bits == 1 else 16)
        for offset in range(sec[4], sec[4] + sec[5], stride):
            tag, value = struct.unpack_from(order + ("iI" if bits == 1 else "qQ"), data, offset)
            if tag == 0:
                break
            if tag == 1:
                needed.append(string(strings, value))
            elif tag == 14:
                soname = string(strings, value)
            elif tag in (15, 29):
                paths.append({"kind": "RPATH" if tag == 15 else "RUNPATH", "value": string(strings, value)})
    interpreter = None
    for i in range(phnum):
        ph = struct.unpack_from(order + ("8I" if bits == 1 else "II6Q"), data, phoff + i * phsize)
        if ph[0] == 3:  # PT_INTERP
            offset, size = (ph[1], ph[4]) if bits == 1 else (ph[2], ph[5])
            interpreter = data[offset:offset + size].rstrip(b"\0").decode("utf-8", errors="replace")
    return {"bits": 32 if bits == 1 else 64, "entry": hex(hdr[3]),
            "type": {1: "ET_REL", 2: "ET_EXEC", 3: "ET_DYN"}.get(kind, str(kind)),
            "machine": {40: "ARM", 183: "AArch64"}.get(machine, str(machine)),
            "interpreter": interpreter, "soname": soname,
            "needed": needed, "search_paths": paths}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("root", type=Path, help="extracted factory rootfs (read only)")
    parser.add_argument("--output", type=Path, help="write inventory instead of stdout")
    parser.add_argument("--format", choices=("json", "markdown"), default="json")
    args = parser.parse_args()
    root = args.root.resolve()
    records, errors = [], []
    # Exact census scopes: top-level executables; recursive library/module trees.
    paths = []
    for directory in ("bin", "sbin", "usr/bin", "usr/sbin"):
        paths.extend((root / directory).glob("*"))
    for directory in ("lib", "usr/lib"):
        paths.extend((root / directory).rglob("*"))
    for path in sorted(set(paths)):
        if path.is_symlink() or not path.is_file():
            continue
        relative = path.relative_to(root).as_posix()
        try:
            with path.open("rb") as stream:
                magic = stream.read(4)
            if magic != b"\x7fELF":
                continue
            records.append({"path": relative, **elf_metadata(path)})
        except (ValueError, IndexError, struct.error, OSError) as exc:
            errors.append({"path": relative, "error": str(exc)})
    providers = defaultdict(set)
    for record in records:
        if record["type"] == "ET_DYN":
            providers[Path(record["path"]).name].add(record["path"])
            if record["soname"]:
                providers[record["soname"]].add(record["path"])
    unresolved = defaultdict(list)
    for record in records:
        record["dependency_candidates"] = {
            name: sorted(providers.get(name, ())) for name in record["needed"]}
        for name, candidates in record["dependency_candidates"].items():
            if not candidates:
                unresolved[name].append(record["path"])
    result = {"schema": 1, "scope": "top-level bin/sbin/usr/bin/usr/sbin plus recursive lib/usr/lib",
              "limits": "Static SONAME/basename candidates only; no loader search simulation, symlink recovery, dlopen or execution proof.",
              "count": len(records), "errors": errors,
              "unresolved_needed": dict(sorted(unresolved.items())), "files": records}
    output = markdown(result) if args.format == "markdown" else json.dumps(result, ensure_ascii=True, indent=2) + "\n"
    if args.output:
        args.output.write_text(output, encoding="utf-8")
    else:
        print(output, end="")
    if errors:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
