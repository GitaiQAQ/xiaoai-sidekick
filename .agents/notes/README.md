# Agent Notes

One kind of design doc lives here. An **Agent Note** records a decision that affects this codebase — the *why* and *what we gave up*, the parts code and docs cannot carry. This file defines where notes live, when to write one, and the in-file format. The Chinese counterpart is [README.zh.md](README.zh.md).

## Layout and naming

Every Agent Note has two axes, both encoded in its path — `{lifecycle}/{class}/yyyy-mm-dd-topic-title.md`:

- **Lifecycle** is the note's status, and the note moves between folders as that status changes:
  - `proposed/` — a proposal under review, not built yet (or only partly).
  - `implemented/` — the decision shipped. The file records what was decided and what was rejected, and is **kept current with what shipped**: when code later moves a file, renames a package, or changes a default, the note is updated in the same change (facts only — paths, names, structure — not the decision). See [implemented/AGENTS.md](implemented/AGENTS.md).
  - `rejected/` — considered and declined. Keep it only while its rationale prevents a tempting, meaningful mistake; otherwise delete it.
  - `archived/` — frozen historical record of an implemented decision that no longer guides current work. See [archived/AGENTS.md](archived/AGENTS.md).
- **Class** is the kind of decision, from the closed set below.

The date is when the topic was first proposed. Cross-references use relative markdown links, never bare prose, so they survive moves. Do not add a centralized `INDEX.md`: the folder tree is the inventory.

## Controlled areas

Two more areas are path-encoded and machine-checked, for material that is not a decision:

- `reference/<topic>.md` — manuals, protocols, policies, deployment scopes, the living state file, and the reading guide. Header: `# Reference: <title>`; filenames are lowercase kebab-case. A reference is current documentation and may change every round; it is not a frozen decision.
- `reports/<topic>.md` — evidence and verification records. Header: `# Report: <title>`; filenames use ASCII lowercase kebab-case. Organize reports by technical topic; keep round and date details in the title/body instead of relying on round order as the only navigation.

Reference material and reports get no lifecycle or class and are never moved into the Agent Note tree. The knowledge guide (`reference/knowledge.md`) is a curated entry point, not a generated index.

Raw material and generator scripts live outside this directory: `rom/` (public ROM sources and checksums), `private/` (device-specific and personal data), `temp/` (regenerable derived artifacts) and `tools/` (scripts, with secrets under `tools/local/`). Directory and file names in this repository must use ASCII characters; document prose may use Chinese.

## Classification

Each note belongs to exactly one class encoded in its folder. The closed set is defined by `scripts/verify-agent-notes.py`; adding a class requires updating the script and this table.

| Class | What it covers |
|---|---|
| `feature` | A new user- or model-facing capability. |
| `bug-fix` | Corrects a defect or closes a gap a postmortem surfaced. |
| `simplification` | Removes code, behavior, or surface area without adding a capability. |
| `architecture` | A structural decision about the shipped source — how components relate, what the runtime vocabulary is. |
| `process` | Tooling, policy, or workflow around the code — gates, build, deployment — not runtime behavior. |
| `testing` | Test infrastructure and strategy. |

`architecture` is about the source we ship; `process` is the surrounding tooling and workflow. `refactor` is deliberately absent — its observable-behavior question is already covered by `simplification`.

## When to write one

Write a note when a decision has lasting value beyond the code:

- the choice is not obvious from reading the implementation;
- there were real alternatives and one was given up;
- it establishes a negative guarantee, ownership boundary, wire/durable semantics, security rule, or a reintroduction condition;
- it records a defect whose cause will tempt a repeat.

Do not write a note for changelogs, task status, per-file documentation, or a restatement of code. Small mechanical changes do not qualify merely because they were implemented.

## The file format

Every active note has this header block, checked by the gate:

```markdown
# Agent Note: <title>

Status: proposed | implemented | rejected
```

Archived notes keep `Status: implemented` and add `Archived: YYYY-MM-DD` immediately below it.

The body skeleton depends on the lifecycle; required headings use these exact English tokens, while the prose may be Chinese:

- **All notes**: `## Problem` and `## Alternatives considered` (the last section).
- **`proposed/`**: plus `## Proposal`, `## Acceptance criteria`, `## Risks`.
- **`implemented/`**: plus `## Decision`, `## Consequences`. Proposal-era headings (`## Proposal`, `## Acceptance criteria`, `## Risks`) are rejected here.
- **`rejected/`**: plus `## Proposal`, `## Why rejected`.
- **`archived/`**: same skeleton as implemented.

Bespoke technical sections may sit between the required ones. Keep facts and dates out of prose where the filename or git history already carries them.

`Alternatives considered` is mandatory: list the options that were actually weighed and why they lost. Never invent alternatives to fill the section; if they were not recorded, say so explicitly.

## Archiving and deletion

- **Delete** an implemented note that only describes a small UI adjustment or a purely mechanical change; repair or remove inbound links. A local bug fix, a new capability, or a substantive behavior decision does not qualify merely because its implementation was small.
- **Archive** an implemented note when the decision is complete, its rationale is unlikely to guide future work, and the record still has historical value.
- **Keep active** while its alternatives, ownership boundary, negative guarantee, durable semantics, security rule, or reintroduction condition still guides future work.
- **Never archive a proposed note**; reject it if it is no longer worth pursuing.
- **Rejected notes** stay only as guardrails against a plausible mistake.

Archival path: `archived/{class}/yyyy-mm-dd-topic-title.md` — `implemented` is absent because only implemented notes enter the archive. Archiving moves the file, keeps `Status: implemented`, inserts `Archived: YYYY-MM-DD`, and repairs inbound links. After that the note is frozen: do not edit, reformat, translate, update, or delete it, and do not treat it as authority for current behavior. Gates skip archived sources.

## Gates

The convention is machine-checked; a convention without a gate rots.


The verifier checks the closed lifecycle and class sets, the filename pattern, the header block and its agreement with the folder, the per-lifecycle section skeleton, the banned proposal-era headings in implemented notes, the presence of `## Alternatives considered`, the absence of a centralized `INDEX.md`, and the controlled areas (`reference/` names and `# Reference:` headers, `reports/` `# Report:` headers). It is verify-don't-generate: it never rewrites a note.

```sh
python3 .agents/notes/scripts/verify-agent-notes.py
```

## Adding a class

Update the closed set in `scripts/verify-agent-notes.py` and the table above in the same change.
