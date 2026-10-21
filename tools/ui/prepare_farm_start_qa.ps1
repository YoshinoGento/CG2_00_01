param([switch]$VerifyOnly, [ValidateSet('Start','Arrival','ArrivalFix','WaterMirror','WaterMotion')][string]$Scenario = 'Start', [string]$ExecutablePath = '')
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$folder = if ($Scenario -eq 'WaterMotion') { 'water_mirror_motion_20261019' } elseif ($Scenario -eq 'WaterMirror') { 'water_mirror_20261019' } elseif ($Scenario -eq 'ArrivalFix') { 'farm_arrival_fix_20261019' } elseif ($Scenario -eq 'Arrival') { 'farm_arrival_20261019' } else { 'farm_start_20261019' }
$output = Join-Path $root "generated/codex_checks/$folder"
$runtime = Join-Path $output 'runtime'
$baseline = Join-Path $output 'protected_before.json'
function Get-ProtectedFiles {
    $files = @(Get-ChildItem -LiteralPath (Join-Path $root 'project/Settings') -File -Recurse)
    $files += @(Get-ChildItem -LiteralPath (Join-Path $root 'generated/transition_contest_20261013/ready') -File)
    $result = @{}
    foreach ($file in $files) { $result[$file.FullName.Substring($root.Length + 1)] = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash }
    return $result
}
$current = Get-ProtectedFiles
New-Item -ItemType Directory -Path $output -Force | Out-Null
if (!(Test-Path -LiteralPath $baseline)) {
    if ($VerifyOnly) { throw 'Missing preservation baseline' }
    $current | ConvertTo-Json | Set-Content -LiteralPath $baseline -Encoding utf8
}
$before = Get-Content -LiteralPath $baseline -Raw | ConvertFrom-Json -AsHashtable
if ($before.Count -ne $current.Count) { throw 'Protected file set changed' }
foreach ($key in $before.Keys) { if ($current[$key] -ne $before[$key]) { throw "Protected file changed: $key" } }
if (!$VerifyOnly) {
    if (!(Test-Path -LiteralPath $runtime)) {
        New-Item -ItemType Directory -Path $runtime | Out-Null
        $base = Join-Path $root 'generated/codex_checks/farm_item_icons_20261019/runtime'
        Get-ChildItem -LiteralPath $base | Where-Object { $_.Extension -ne '.exe' -and $_.Name -notin @('MANIFEST.json', 'StartGame.cmd') } |
            Copy-Item -Destination $runtime -Recurse
    }
    foreach ($name in @('ui', 'generated/text')) {
        Copy-Item -LiteralPath (Join-Path $root "project/Resources/$name") -Destination (Join-Path $runtime "Resources/$(Split-Path $name -Parent)") -Recurse -Force
    }
    if ($Scenario -ne 'Start') {
        Copy-Item -LiteralPath (Join-Path $root 'project/Resources/farm/arrival') -Destination (Join-Path $runtime 'Resources/farm') -Recurse -Force
    }
    if ($Scenario -in @('WaterMirror','WaterMotion')) {
        Copy-Item -LiteralPath (Join-Path $root 'project/Resources/shader') -Destination (Join-Path $runtime 'Resources') -Recurse -Force
    }
    $sourceExe = if ($ExecutablePath) { (Resolve-Path -LiteralPath $ExecutablePath).Path } else { Join-Path $root 'generated/outputs/Release/CG2_00_01.exe' }
    Copy-Item -LiteralPath $sourceExe -Destination (Join-Path $runtime 'FarmStartQA.exe') -Force
}
[pscustomobject]@{ Runtime=$runtime; ProtectedFiles=$current.Count; ExeHash=(Get-FileHash -LiteralPath (Join-Path $runtime 'FarmStartQA.exe')).Hash } | ConvertTo-Json
