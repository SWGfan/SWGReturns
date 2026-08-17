# =====================================================================
#  Build the SWG Genesis content bundle
#  https://github.com/SWGfan/ServerInstaller
#
#  Collects the CUSTOM game content - the Companion System and the
#  Genesis world content - hashes it, and writes a manifest the
#  launcher and installer read.
#
#  Deliberately NOT included: the base game archives (bottom.tre,
#  data_*.tre, patch_*.tre). Those are LucasArts/SOE retail files.
#  Every player supplies their own, exactly as SWGEmu requires.
#
#  Files are kept separate rather than zipped together on purpose:
#  aftermath_NGE.tre is 104 MB and rarely changes, companion_patch.tre
#  is 3 MB and changes often. Separate + checksummed means a content
#  update usually costs players 3 MB instead of 144 MB.
# =====================================================================

$ErrorActionPreference = 'Continue'
function Say($m, $c = 'Gray') { Write-Host $m -ForegroundColor $c }

# Search-tree priorities, taken from the working client config.
# Higher wins. The base game occupies 25 and below, so custom content
# sits above it - which is what makes the Companion System visible
# instead of players seeing raw text keys.
$CONTENT = @(
    @{ name = 'companion_patch.tre'; priority = 29; desc = 'Companion System' },
    @{ name = 'aftermath_1.tre';     priority = 28; desc = 'Genesis world content' },
    @{ name = 'aftermath_house.tre'; priority = 27; desc = 'Genesis housing' },
    @{ name = 'aftermath_NGE.tre';   priority = 26; desc = 'Genesis NGE content' }
)
$BASE_PRIORITY = 25      # first slot the base game may use
$MAX_PRIORITY  = 30

$OUT = 'C:\SWGGenesis\content_bundle'
$SEARCH = @(
    'C:\SWGGenesis\docs\companion_system\tools',
    'C:\SWGEmu',
    'D:\Launcher\newreturnbenserver',
    'C:\SWGGenesis'
)

Clear-Host
Say ''
Say '  ============================================================' Cyan
Say '   Build the content bundle' Cyan
Say '  ============================================================' Cyan
Say ''

New-Item -ItemType Directory -Force -Path $OUT | Out-Null
$version = Get-Date -Format 'yyyy.MM.dd'
$entries = @()
$missing = @()

foreach ($c in $CONTENT) {
    # Prefer the newest copy found - companion_patch.tre in particular
    # exists in several places and the repo copy is usually freshest.
    $hit = $SEARCH | ForEach-Object { Join-Path $_ $c.name } |
           Where-Object { Test-Path $_ } |
           Get-Item -ErrorAction SilentlyContinue |
           Sort-Object LastWriteTime -Descending | Select-Object -First 1

    if (-not $hit) { $missing += $c.name; Say ("  MISSING: {0}" -f $c.name) Red; continue }

    Say ("  {0,-24} {1,8:N1} MB   {2}" -f $c.name, ($hit.Length/1MB), $hit.LastWriteTime.ToString('yyyy-MM-dd')) Green
    Say ("  {0,-24} from {1}" -f '', $hit.DirectoryName) DarkGray

    Copy-Item $hit.FullName (Join-Path $OUT $c.name) -Force
    $sha = (Get-FileHash (Join-Path $OUT $c.name) -Algorithm SHA256).Hash.ToLower()

    $entries += [ordered]@{
        name        = $c.name
        description = $c.desc
        size        = $hit.Length
        sha256      = $sha
        priority    = $c.priority
        modified    = $hit.LastWriteTime.ToString('yyyy-MM-dd')
    }
}

if ($missing.Count -gt 0) {
    Say ''
    Say ("  Could not find: {0}" -f ($missing -join ', ')) Yellow
    Say '  The bundle will be incomplete. Find those files first.' Yellow
    Say ''
    if ((Read-Host '  Continue anyway? (y/n)') -ne 'y') { return }
}

$manifest = [ordered]@{
    version       = $version
    generated     = (Get-Date).ToString('s')
    baseUrl       = "https://github.com/SWGfan/ServerInstaller/releases/download/content-$version"
    maxPriority   = $MAX_PRIORITY
    basePriority  = $BASE_PRIORITY
    note          = 'Custom content only. The base game archives are supplied by each player.'
    files         = $entries
}
$manifest | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $OUT 'manifest.json') -Encoding UTF8

$total = ($entries | Measure-Object size -Sum).Sum
Say ''
Say '  ============================================================' Cyan
Say ("   {0} files, {1:N0} MB total, version {2}" -f $entries.Count, ($total/1MB), $version) Green
Say ("   Ready in: {0}" -f $OUT) Green
Say '  ============================================================' Cyan
Say ''
Say '  To publish, from that folder:' Gray
Say ''
Say ("    gh release create content-$version *.tre manifest.json \" ) Yellow
Say ("       --repo SWGfan/ServerInstaller \") Yellow
Say ("       --title `"Game content $version`" \") Yellow
Say ("       --notes `"Companion System and Genesis content. Base game not included.`"") Yellow
Say ''
Say '  (Or drag the files onto a new Release on the GitHub website.)' Gray
Say ''
Say '  Note: aftermath_NGE.tre is over 100 MB, so it must go on a' DarkGray
Say '  Release rather than being committed to the repository.' DarkGray
Say ''
