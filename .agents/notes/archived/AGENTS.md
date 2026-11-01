# Archived Agent Notes

This tree holds implemented Agent Notes that no longer guide current work but still carry historical decision value. Once sealed, a note is **frozen**.

## What may enter

- Only `implemented` notes. `proposed` notes are never archived; reject them instead.
- The decision is complete, its rationale is unlikely to guide future work, and the record still has historical value.
- Notes that still carry an ownership rule, negative guarantee, wire/durable semantics, security rule, or reintroduction condition stay active, regardless of age or length.

## Sealing a note

1. Move `implemented/{class}/yyyy-mm-dd-topic-title.md` to `archived/{class}/yyyy-mm-dd-topic-title.md`. The `implemented` segment is absent.
2. Make no body edits. Insert `Archived: YYYY-MM-DD` immediately below `Status: implemented`, using the archival date.
3. Repair inbound links in active prose: point them at the current authority, or at the archived path when the history is intentionally cited. Remove dead links.

These moves and the added line are the only permitted changes.

## After sealing

- Do not edit, reformat, translate, update facts, fix outbound links, or delete an archived note.
- Do not treat archived notes as authority for current behavior.
- Gates skip archived notes as sources; active prose may still link into the archive to cite history.

## Format

Archived notes keep the `implemented` skeleton plus the `Archived:` line. The gate checks the lifecycle, class folder, filename, header, and sections; it does not check translation or freshness.
