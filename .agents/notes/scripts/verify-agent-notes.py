#!/usr/bin/env python3
"""Read-only format gate for Agent Notes."""
from pathlib import Path
import re
import sys

notes = Path(__file__).resolve().parent.parent
lifecycles = ("proposed", "implemented", "rejected", "archived")
classes = ("feature", "bug-fix", "simplification", "architecture", "process", "testing")
violations = []
counts = {"note": 0, "reference": 0, "report": 0}

def fail(path, message):
    violations.append(f"{path.relative_to(notes)}: {message}")

for index in notes.rglob("INDEX.md"):
    fail(index, "centralized INDEX.md is not allowed")

for lifecycle in lifecycles:
    root = notes / lifecycle
    if not root.exists():
        continue
    for child in root.iterdir():
        if child.is_file() and child.name != "AGENTS.md":
            fail(child, "loose file at lifecycle root; notes live in a class folder")
    for path in root.rglob("*"):
        if not path.is_file() or path.name == "AGENTS.md":
            continue
        rel = path.relative_to(notes).parts
        if path.suffix != ".md":
            fail(path, "only .md notes are allowed here")
            continue
        if len(rel) < 3:
            fail(path, "note must sit in {lifecycle}/{class}/")
            continue
        if rel[1] not in classes:
            fail(path, f"unknown class '{rel[1]}'")
        if not re.fullmatch(r"\d{4}-\d{2}-\d{2}-[a-z0-9]+(?:-[a-z0-9]+)*", path.stem):
            fail(path, "filename must be yyyy-mm-dd-topic-title.md")
        counts["note"] += 1
        lines = path.read_text(encoding="utf-8").splitlines()
        first = next((line for line in lines if line.strip()), "")
        if not re.match(r"^# Agent Note: \S", first):
            fail(path, 'missing "# Agent Note: <title>" header')
        section_start = next((i for i, line in enumerate(lines) if line.startswith("## ")), len(lines))
        preamble = lines[:section_start]
        status = next((m.group(1) for line in preamble if (m := re.match(r"Status:\s*(proposed|implemented|rejected)\s*$", line))), None)
        archived = any(re.match(r"Archived:\s*\d{4}-\d{2}-\d{2}\s*$", line) for line in preamble)
        if status is None:
            fail(path, 'missing "Status:" line before the first section')
        expected = "implemented" if lifecycle == "archived" else lifecycle
        if status and status != expected:
            fail(path, f"Status '{status}' does not match lifecycle '{lifecycle}'")
        if lifecycle == "archived" and not archived:
            fail(path, 'archived note needs "Archived: YYYY-MM-DD"')
        if lifecycle != "archived" and archived:
            fail(path, '"Archived:" is only allowed in archived/')
        headings = {m.group(1) for line in lines if (m := re.match(r"^## (.+?)\s*$", line))}
        required = {"Problem", "Alternatives considered"}
        required |= {"Proposal", "Acceptance criteria", "Risks"} if lifecycle == "proposed" else set()
        required |= {"Decision", "Consequences"} if lifecycle in ("implemented", "archived") else set()
        required |= {"Proposal", "Why rejected"} if lifecycle == "rejected" else set()
        for heading in required - headings:
            fail(path, f"missing section '## {heading}'")
        if lifecycle in ("implemented", "archived"):
            for heading in {"Proposal", "Acceptance criteria", "Risks"} & headings:
                fail(path, f"proposal-era section '## {heading}' is not allowed here")

ref_root = notes / "reference"
if ref_root.exists():
    for path in ref_root.iterdir():
        if not path.is_file():
            continue
        if path.suffix != ".md":
            fail(path, "only .md reference files are allowed here")
            continue
        if not re.fullmatch(r"[a-z0-9]+(?:-[a-z0-9]+)*", path.stem):
            fail(path, "reference filename must be lowercase kebab-case")
        counts["reference"] += 1
        first = next((line for line in path.read_text(encoding="utf-8").splitlines() if line.strip()), "")
        if not re.match(r"^# Reference: \S", first):
            fail(path, 'missing "# Reference: <title>" header')

report_root = notes / "reports"
if report_root.exists():
    for path in report_root.glob("*.md"):
        if not re.fullmatch(r"[a-z0-9]+(?:-[a-z0-9]+)*", path.stem):
            fail(path, "report filename must be lowercase ASCII kebab-case")
        counts["report"] += 1
        first = next((line for line in path.read_text(encoding="utf-8").splitlines() if line.strip()), "")
        if not re.match(r"^# Report: \S", first):
            fail(path, 'missing "# Report: <title>" header')

if violations:
    print(f"agent-notes: {counts['note']} note(s), {counts['reference']} reference(s), {counts['report']} report(s) checked, {len(violations)} violation(s)")
    print("\n".join(f"  {v}" for v in violations))
    sys.exit(1)
print(f"agent-notes: {counts['note']} note(s), {counts['reference']} reference(s), {counts['report']} report(s) checked, layout and format OK")
