# Publishes every FireRed Expansion Randomizer version as a public GitHub Release.
# Run make_release_files.py first. Oldest first; only the newest is marked "Latest".
# Versions that already have a release are skipped.
$ErrorActionPreference = 'Continue'  # gh writes normal messages to stderr; check $LASTEXITCODE instead
$repo = 'JackZicrosky/Pokemon-FireRed-Randomizer-with-every-Pokemon-Early-Access-'
$versions = '0.1', '0.2', '0.3', '0.4', '0.5', '0.5.1', '0.5.2', '0.6', '0.6.1', '0.7', '0.7.1'
$latest = $versions[-1]
$root = Resolve-Path (Join-Path $PSScriptRoot '..\..\..')
$fireRed = Split-Path $root -Parent   # the project lives in ...\Fire Red\source
$staging = Join-Path $root 'build\release'
$gh = (Get-Command gh -ErrorAction SilentlyContinue).Source
if (-not $gh) { $gh = 'C:\Program Files\GitHub CLI\gh.exe' }
foreach ($v in $versions) {
    $tag = "v$v"
    & $gh release view $tag --repo $repo *> $null
    if ($LASTEXITCODE -eq 0) { Write-Host "$tag already published, skipping"; continue }
    if ($v -eq $latest) { $flag = '--latest' } else { $flag = '--latest=false' }
    & $gh release create $tag --repo $repo --verify-tag $flag `
        --title "FireRed Expansion Randomizer $tag" `
        --notes-file (Join-Path $staging "notes $tag.md") `
        (Join-Path $fireRed "FireRed Expansion Randomizer $tag.bps") (Join-Path $staging "README $tag.txt")
    if ($LASTEXITCODE -ne 0) { throw "Publishing $tag failed" }
    Write-Host "Published $tag"
}
