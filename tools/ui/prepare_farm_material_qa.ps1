param([switch]$VerifyOnly, [ValidateSet('current','baseline')][string]$Variant = 'current')
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$output = Join-Path $root 'generated/codex_checks/farm_material_20261020'
$runtime = Join-Path $output $Variant
$baseline = Join-Path $output 'protected_before.json'
$ready = Join-Path $root 'generated/start_contest_20261020/ready'
$packages = @(Get-ChildItem -LiteralPath $ready -Directory)
if ($packages.Count -ne 1) { throw 'Expected one reviewed runtime package' }
function Get-ProtectedFiles {
    $folders = @('project/Settings', 'generated/start_contest_20261020/ready',
        'generated/title_contest_20261006/ready', 'generated/transition_contest_20261013/ready')
    $result = @{}
    foreach ($folder in $folders) {
        $path = Join-Path $root $folder
        if (!(Test-Path -LiteralPath $path)) { continue }
        foreach ($file in Get-ChildItem -LiteralPath $path -File -Recurse) {
            $result[$file.FullName.Substring($root.Length + 1)] = (Get-FileHash -LiteralPath $file.FullName).Hash
        }
    }
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
foreach ($key in $before.Keys) {
    if ($current[$key] -ne $before[$key]) { throw "Protected file changed: $key" }
}
if (!$VerifyOnly) {
    $exe = Join-Path $runtime 'FarmMaterialQA.exe'
    if (Get-Process -Name FarmMaterialQA -ErrorAction SilentlyContinue | Where-Object Path -eq $exe) {
        throw 'Close the owned QA instance before refreshing its files'
    }
    if (!(Test-Path -LiteralPath $runtime)) {
        New-Item -ItemType Directory -Path $runtime | Out-Null
        Get-ChildItem -LiteralPath $packages[0].FullName |
            Where-Object { $_.Extension -ne '.exe' -and $_.Name -notin @('MANIFEST.json', 'StartGame.cmd') } |
            Copy-Item -Destination $runtime -Recurse
    }
    $sourceExe = Join-Path $packages[0].FullName 'SuiroNogyo.exe'
    if ($Variant -eq 'current') {
        $sourceExe = Join-Path $root 'generated/outputs/Release/CG2_00_01.exe'
        Copy-Item -LiteralPath (Join-Path $root 'project/Resources/shader') -Destination (Join-Path $runtime 'Resources') -Recurse -Force
    }
    Copy-Item -LiteralPath $sourceExe -Destination $exe -Force
}
[pscustomobject]@{ Runtime=$runtime; ProtectedFiles=$current.Count; Variant=$Variant } | ConvertTo-Json
