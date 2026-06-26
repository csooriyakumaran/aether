#requires -Version 7
# scripts/release.ps1 - mechanical release driver for aether.
#
# Phase A (manual, BEFORE running this): edit CHANGELOG.md / README.md / TODO.md,
# add the "## [X.Y.Z]" changelog section for this release, and commit those edits.
# This script performs the mechanical Phase B only and refuses to run on a dirty tree.
#
# Usage:
#   pwsh scripts/release.ps1 -DryRun            # narrate every mutating step, change nothing
#   pwsh scripts/release.ps1                    # release VERSION (minus -dev), bump patch
#   pwsh scripts/release.ps1 -NextVersion 0.1.0-dev
param(
    [string]$NextVersion,   # next dev version; default = patch bump of the release
    [switch]$DryRun         # narrate every mutating step without performing it
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path

# run a native command, honour -DryRun, and fail hard on a nonzero exit code
# ($ErrorActionPreference='Stop' does NOT catch native exit codes, only cmdlets).
function Exec([string]$file, [Parameter(ValueFromRemainingArguments)][string[]]$cmdArgs) {
    Write-Host ">> $file $($cmdArgs -join ' ')" -ForegroundColor Cyan
    if ($DryRun) { Write-Host '   (dry-run, skipped)' -ForegroundColor DarkGray; return }
    & $file @cmdArgs
    if ($LASTEXITCODE -ne 0) { throw "$file exited with code $LASTEXITCODE" }
}

# --- derive versions from the VERSION file -------------------------------
$versionFile = Join-Path $root 'VERSION'
$raw = (Get-Content $versionFile -Raw).Trim()
if ($raw -notmatch '^(\d+)\.(\d+)\.(\d+)-dev$') {
    throw "VERSION is '$raw'; expected X.Y.Z-dev. Refusing to release."
}
$maj = [int]$Matches[1]; $min = [int]$Matches[2]; $pat = [int]$Matches[3]
$release = "$maj.$min.$pat"
$tag     = "v$release"
if (-not $NextVersion) { $NextVersion = "$maj.$min.$($pat + 1)-dev" }

Write-Host "Releasing $tag  (next dev: $NextVersion)" -ForegroundColor Green

# --- pre-flight (read-only; runs even under -DryRun) ---------------------
$branch = (& git -C $root rev-parse --abbrev-ref HEAD).Trim()
if ($branch -ne 'master') { throw "On '$branch', not master." }

if (& git -C $root status --porcelain) {
    throw 'Working tree is dirty. Commit your CHANGELOG/README/TODO edits first.'
}

if (& git -C $root tag --list $tag)              { throw "Tag $tag already exists locally." }
if (& git -C $root ls-remote --tags origin $tag) { throw "Tag $tag already exists on origin." }

& gh auth status | Out-Null
if ($LASTEXITCODE -ne 0) { throw 'gh is not authenticated (run: gh auth login).' }

# verify-only: the curated CHANGELOG section must exist (it becomes the release notes)
$clFile = Join-Path $root 'CHANGELOG.md'
$lines  = Get-Content $clFile
$start  = -1
for ($i = 0; $i -lt $lines.Count; $i++) {
    if ($lines[$i].StartsWith("## [$release]")) { $start = $i; break }
}
if ($start -lt 0) { throw "CHANGELOG.md has no '## [$release]' section. Write it first." }
$end = $lines.Count
for ($j = $start + 1; $j -lt $lines.Count; $j++) {
    if ($lines[$j].StartsWith('## [')) { $end = $j; break }
}
$notes     = ($lines[($start + 1)..($end - 1)] -join "`n").Trim()
$notesFile = Join-Path ([System.IO.Path]::GetTempPath()) "aether-$tag-notes.md"
$notes | Set-Content $notesFile -Encoding utf8

# --- gate: build + test, abort on red ------------------------------------
# -DCMAKE_BUILD_TYPE=Release covers Ninja (single-config); --config Release covers
# MSVC (multi-config). Each is a harmless no-op under the other generator.
$build = Join-Path $root 'build'
Exec cmake -S $root -B $build -DCMAKE_BUILD_TYPE=Release -DAETHER_BUILD_TESTS=ON -DAETHER_BUILD_INSTALL=ON
Exec cmake --build $build --config Release
Exec ctest --test-dir $build -C Release --output-on-failure

# --- release commit + annotated tag + push -------------------------------
# Tag points at this commit, so the published artifacts provably match the tag.
if (-not $DryRun) { [System.IO.File]::WriteAllText($versionFile, "$release`r`n") }
Exec git -C $root add VERSION
Exec git -C $root commit -m "release: $tag"
Exec git -C $root tag -a $tag -m "aether $tag"
Exec git -C $root push origin master
Exec git -C $root push origin $tag

# --- artifacts: install -> zip -> gh release -----------------------------
$stage = Join-Path $root "releases/$tag"
$zip   = Join-Path $root "releases/aether-$tag.zip"
Exec cmake --install $build --config Release --prefix $stage
if (-not $DryRun) { Compress-Archive -Path "$stage/*" -DestinationPath $zip -Force }
Exec gh release create $tag --title "aether $tag" --notes-file $notesFile $zip

# --- re-arm for the next cycle -------------------------------------------
if (-not $DryRun) { [System.IO.File]::WriteAllText($versionFile, "$NextVersion`r`n") }
Exec git -C $root add VERSION
Exec git -C $root commit -m "chore: bump version to $NextVersion"
Exec git -C $root push origin master

Write-Host "Done: published $tag, bumped to $NextVersion." -ForegroundColor Green
