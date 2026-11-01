# Implemented Agent Notes

These notes record shipped decisions. They are current authority for the *decision*, and current facts about the shipped code.

## Sync obligation

When a later change moves a file, renames a package or command, changes a default, or replaces an interface that an implemented note names, update the note in the same change:

- update paths, names, defaults, and structure so they match what shipped;
- keep the recorded decision, alternatives, and consequences intact;
- do not add new proposal-era content, dates, or status narration;
- do not rewrite the note into a changelog.

If the decision itself is superseded, write a new note and archive the old one (see [../archived/AGENTS.md](../archived/AGENTS.md)) instead of silently editing the decision.

## Lifecycle

- Keep a note active while its rationale, alternatives, negative guarantee, ownership boundary, durable/wire semantics, security rule, or reintroduction condition still guides future work.
- Delete notes that only describe small UI adjustments or purely mechanical changes, and repair inbound links.
- Archive notes that are complete, unlikely to guide future work, and still historically valuable.
- Never archive a proposal; reject it instead.

## Format and gate

Every note keeps the `implemented/` skeleton from [../README.md](../README.md): `# Agent Note:`, `Status: implemented`, `## Problem`, `## Decision`, `## Consequences`, `## Alternatives considered`. Proposal-era headings are rejected.

Run the gate after any edit:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .agents/notes/scripts/verify-agent-notes.ps1
```
