# Verifies Agent Notes: closed tree, filename, header, lifecycle skeleton.
# Verify-don't-generate: never rewrites a note. Exit 1 on the first failing file set.

$ErrorActionPreference = 'Stop'
$notes = Split-Path -Parent $PSScriptRoot
$lifecycles = @('proposed','implemented','rejected','archived')
$classes = @('feature','bug-fix','simplification','architecture','process','testing')
$violations = New-Object System.Collections.Generic.List[string]
$checked = 0

function Add-Violation([string]$path,[string]$message) {
    $script:violations.Add(("{0}: {1}" -f $path.Replace($notes + '\',''), $message))
}

# 1. No centralized index anywhere in the corpus.
Get-ChildItem -LiteralPath $notes -Recurse -File -Filter 'INDEX.md' -ErrorAction SilentlyContinue | ForEach-Object {
    Add-Violation $_.FullName 'centralized INDEX.md is not allowed'
}

foreach ($lifecycle in $lifecycles) {
    $root = Join-Path $notes $lifecycle
    if (-not (Test-Path -LiteralPath $root)) { continue }

    # Loose files at the lifecycle root are only the instruction files.
    Get-ChildItem -LiteralPath $root -File -ErrorAction SilentlyContinue | ForEach-Object {
        if ($_.Name -ne 'AGENTS.md') { Add-Violation $_.FullName 'loose file at lifecycle root; notes live in a class folder' }
    }

    Get-ChildItem -LiteralPath $root -Recurse -File -ErrorAction SilentlyContinue | ForEach-Object {
        $file = $_
        if ($file.Name -eq 'AGENTS.md') { return }
        $relative = $file.FullName.Substring($notes.Length + 1)
        $parts = $relative -split '\\'
        if ($file.Extension -ne '.md') { Add-Violation $file.FullName 'only .md notes are allowed here'; return }
        if ($parts.Count -lt 3) { Add-Violation $file.FullName 'note must sit in {lifecycle}/{class}/'; return }

        $class = $parts[1]
        if ($classes -notcontains $class) { Add-Violation $file.FullName ("unknown class '{0}'" -f $class) }
        if ($file.BaseName -notmatch '^\d{4}-\d{2}-\d{2}-[a-z0-9]+(-[a-z0-9]+)*$') {
            Add-Violation $file.FullName 'filename must be yyyy-mm-dd-topic-title.md'
        }

        $script:checked++
        $text = [IO.File]::ReadAllText($file.FullName)
        $lines = $text -split "`r?`n"

        # Header: first non-empty line, then status before the first section.
        $first = ($lines | Where-Object { $_.Trim() -ne '' } | Select-Object -First 1)
        if ($null -eq $first -or $first -notmatch '^# Agent Note: \S') {
            Add-Violation $file.FullName 'missing "# Agent Note: <title>" header'
        }
        $status = $null
        $archivedLine = $null
        $sectionStart = $lines.Count
        for ($i = 0; $i -lt $lines.Count; $i++) {
            if ($lines[$i] -match '^## ') { $sectionStart = $i; break }
        }
        for ($i = 0; $i -lt $sectionStart; $i++) {
            if ($lines[$i] -match '^Status:\s*(proposed|implemented|rejected)\s*$') { $status = $Matches[1] }
            if ($lines[$i] -match '^Archived:\s*(\d{4}-\d{2}-\d{2})\s*$') { $archivedLine = $Matches[1] }
        }
        if ($null -eq $status) { Add-Violation $file.FullName 'missing "Status:" line before the first section' }
        $expected = if ($lifecycle -eq 'archived') { 'implemented' } else { $lifecycle }
        if ($null -ne $status -and $status -ne $expected) {
            Add-Violation $file.FullName ("Status '{0}' does not match lifecycle '{1}'" -f $status, $lifecycle)
        }
        if ($lifecycle -eq 'archived') {
            if ($null -eq $archivedLine) { Add-Violation $file.FullName 'archived note needs "Archived: YYYY-MM-DD"' }
        } elseif ($null -ne $archivedLine) {
            Add-Violation $file.FullName '"Archived:" is only allowed in archived/'
        }

        # Section skeleton.
        $headings = @()
        foreach ($line in $lines) { if ($line -match '^## (.+?)\s*$') { $headings += $Matches[1] } }
        $required = @('Problem','Alternatives considered')
        if ($lifecycle -eq 'proposed') { $required += @('Proposal','Acceptance criteria','Risks') }
        if ($lifecycle -eq 'implemented' -or $lifecycle -eq 'archived') { $required += @('Decision','Consequences') }
        if ($lifecycle -eq 'rejected') { $required += @('Proposal','Why rejected') }
        foreach ($section in $required) {
            if ($headings -notcontains $section) { Add-Violation $file.FullName ("missing section '## {0}'" -f $section) }
        }
        if ($lifecycle -eq 'implemented' -or $lifecycle -eq 'archived') {
            foreach ($banned in @('Proposal','Acceptance criteria','Risks')) {
                if ($headings -contains $banned) { Add-Violation $file.FullName ("proposal-era section '## {0}' is not allowed here" -f $banned) }
            }
        }
    }
}

# 2. Controlled reference area: kebab-case names, "Reference:" header.
$referenceChecked = 0
$refRoot = Join-Path $notes 'reference'
if (Test-Path -LiteralPath $refRoot) {
    Get-ChildItem -LiteralPath $refRoot -File -ErrorAction SilentlyContinue | ForEach-Object {
        $file = $_
        if ($file.Extension -ne '.md') { Add-Violation $file.FullName 'only .md reference files are allowed'; return }
        if ($file.BaseName -notmatch '^[a-z0-9]+(-[a-z0-9]+)*$') { Add-Violation $file.FullName 'reference filename must be lowercase kebab-case' }
        $script:referenceChecked++
        $lines = [IO.File]::ReadAllLines($file.FullName)
        $first = ($lines | Where-Object { $_.Trim() -ne '' } | Select-Object -First 1)
        if ($null -eq $first -or $first -notmatch '^# Reference: \S') {
            Add-Violation $file.FullName 'missing "# Reference: <title>" header'
        }
    }
}

# 3. Reports: "Report:" header.
$reportsChecked = 0
$reportRoot = Join-Path $notes 'reports'
if (Test-Path -LiteralPath $reportRoot) {
    Get-ChildItem -LiteralPath $reportRoot -File -Filter '*.md' -ErrorAction SilentlyContinue | ForEach-Object {
        $script:reportsChecked++
        $lines = [IO.File]::ReadAllLines($_.FullName)
        $first = ($lines | Where-Object { $_.Trim() -ne '' } | Select-Object -First 1)
        if ($null -eq $first -or $first -notmatch '^# Report: \S') {
            Add-Violation $_.FullName 'missing "# Report: <title>" header'
        }
    }
}

if ($violations.Count -gt 0) {
    Write-Output ("agent-notes: {0} note(s), {1} reference(s), {2} report(s) checked, {3} violation(s)" -f $checked, $referenceChecked, $reportsChecked, $violations.Count)
    foreach ($v in $violations) { Write-Output ("  {0}" -f $v) }
    exit 1
}
Write-Output ("agent-notes: {0} note(s), {1} reference(s), {2} report(s) checked, layout and format OK" -f $checked, $referenceChecked, $reportsChecked)
exit 0
