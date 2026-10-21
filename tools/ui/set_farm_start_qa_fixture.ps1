param([ValidateSet('Canal', 'Empty')][string]$Fixture = 'Canal', [ValidateSet('Start','Arrival','ArrivalFix')][string]$Scenario = 'Start')
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$base = Join-Path $root 'generated/codex_checks/farm_item_icons_20261019/runtime/Settings/farm/saves/farm_1790566537804_0.json'
$folder = if ($Scenario -eq 'ArrivalFix') { 'farm_arrival_fix_20261019' } elseif ($Scenario -eq 'Arrival') { 'farm_arrival_20261019' } else { 'farm_start_20261019' }
$runtime = Join-Path $root "generated/codex_checks/$folder/runtime"
$target = Join-Path $runtime 'Settings/farm/saves/farm_1790566537804_0.json'
$data = Get-Content -LiteralPath $base -Raw | ConvertFrom-Json
if ($data.tiles.Count -ne 20 -or $data.grid.width -ne 5) { throw 'Unexpected QA fixture grid' }
if ($Fixture -eq 'Canal') {
    $data.document.displayName = 'QA connected waterway'
    $data.date.day = 3
    # Source at rear left; route across the back and down the right edge.
    $data.tiles[15].feature = 'WaterSource'
    foreach ($index in @(15,16,17,18,19,14,9,4)) {
        if ($index -ne 15) { $data.tiles[$index].feature = 'Canal' }
        $data.tiles[$index].height = 1
        $data.tiles[$index].waterAmount = 0.8
    }
    foreach ($pair in @(@(1,'Carrot'),@(7,'Tomato'),@(12,'Pumpkin'))) {
        $tile=$data.tiles[$pair[0]]
        $tile.crop=$pair[1]; $tile.state='Planted'; $tile.growth=0.7; $tile.moisture=0.55
    }
}
$data | ConvertTo-Json -Depth 30 | Set-Content -LiteralPath $target -Encoding utf8
Write-Output "QA-only fixture: $Fixture"
