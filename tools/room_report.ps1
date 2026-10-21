# Hand-made room report: door-shape coverage and per-room wall checks.
#
#   powershell -ExecutionPolicy Bypass -File tools\room_report.ps1
#   powershell -ExecutionPolicy Bypass -File tools\room_report.ps1 -Biome Forest
#
# Reads TestGame\Rooms\<biome>\**\*.mroom. Only Standard rooms (ROOMTYPE 0) count
# toward coverage; generated rooms under Rooms\Generated are skipped.
# Per-room checks:
#   - a door declared in DOORS with no enabled DOORZONE (or the reverse)
#   - a gap in the solid wall ring that is not covered by an enabled door zone
# Exit code is 0 even with warnings; read the list.
param(
    [string]$Biome = '',
    [string]$RoomsDir = (Join-Path $PSScriptRoot '..\TestGame\Rooms')
)
$ErrorActionPreference = 'Stop'

$shapeLabels = @('N', 'S', 'E', 'W')
$allShapes = 1..15   # every non-empty door combination (bit mask N=1 S=2 E=4 W=8 in the file order)

function Get-ShapeCode([int[]]$doors) {
    # Same order the writer uses: north, south, east, west.
    return -join (0..3 | ForEach-Object { if ($doors[$_]) { $shapeLabels[$_] } else { '.' } })
}

$biomes = if ($Biome) { @($Biome) } else { @('Forest', 'Caverns', 'Graveyard', 'Jungle') }
$warnings = New-Object System.Collections.Generic.List[string]

foreach ($b in $biomes) {
    $dir = Join-Path $RoomsDir $b
    $counts = @{}
    $files = @()
    if (Test-Path $dir) { $files = Get-ChildItem -Path $dir -Filter *.mroom -Recurse -File }

    foreach ($file in $files) {
        $lines = Get-Content -LiteralPath $file.FullName
        $type = ($lines | Where-Object { $_ -match '^ROOMTYPE\s' } | Select-Object -First 1) -replace '^ROOMTYPE\s+', ''
        $doorLine = $lines | Where-Object { $_ -match '^DOORS\s' } | Select-Object -First 1
        if (-not $doorLine) { $warnings.Add("$($file.Name): no DOORS line"); continue }
        $doors = @(($doorLine -replace '^DOORS\s+', '').Trim() -split '\s+' | ForEach-Object { [int]$_ })

        if ($type -eq '0') {
            $code = Get-ShapeCode $doors
            if (-not $counts.ContainsKey($code)) { $counts[$code] = 0 }
            $counts[$code]++
        }

        # Door zones: enabled ones are the openings.
        $zones = @()
        foreach ($line in ($lines | Where-Object { $_ -match '^DOORZONE\s' })) {
            $p = $line -split '\s+'
            if ($p[2] -eq '1') { $zones += [pscustomobject]@{ X = [int]$p[3]; Y = [int]$p[4]; W = [int]$p[5]; H = [int]$p[6] } }
        }
        $declared = ($doors | Measure-Object -Sum).Sum
        if ($declared -ne $zones.Count) {
            $warnings.Add("$b/$($file.Name): DOORS declares $declared door(s) but $($zones.Count) enabled DOORZONE(s)")
        }

        # Solid wall ring: any '0' on the outer ring that no enabled door zone covers is a gap.
        $solidStart = [array]::IndexOf($lines, 'SOLID_BEGIN')
        $solidEnd = [array]::IndexOf($lines, 'SOLID_END')
        if ($solidStart -lt 0 -or $solidEnd -le $solidStart) { $warnings.Add("$b/$($file.Name): no SOLID section"); continue }
        $grid = @($lines[($solidStart + 1)..($solidEnd - 1)] | Where-Object { $_ -match '^[01]+$' })
        if ($grid.Count -eq 0) { $warnings.Add("$b/$($file.Name): SOLID section is empty"); continue }
        $rows = $grid.Count; $cols = $grid[0].Length
        $gaps = 0
        for ($r = 0; $r -lt $rows; $r++) {
            for ($c = 0; $c -lt $cols; $c++) {
                $onRing = ($r -eq 0 -or $r -eq $rows - 1 -or $c -eq 0 -or $c -eq $cols - 1)
                if (-not $onRing -or $grid[$r][$c] -ne '0') { continue }
                $covered = $false
                foreach ($z in $zones) {
                    if ($c -ge $z.X -and $c -lt $z.X + $z.W -and $r -ge $z.Y -and $r -lt $z.Y + $z.H) { $covered = $true; break }
                }
                if (-not $covered) { $gaps++ }
            }
        }
        if ($gaps -gt 0) { $warnings.Add("$b/$($file.Name): $gaps open cell(s) in the outer wall outside a door zone") }
    }

    Write-Output ""
    Write-Output "== $b  ($($files.Count) rooms, standard rooms by door shape)"
    $covered = 0
    foreach ($mask in $allShapes) {
        $doors = @(0..3 | ForEach-Object { [int](($mask -shr $_) -band 1) })
        $code = Get-ShapeCode $doors
        $n = if ($counts.ContainsKey($code)) { $counts[$code] } else { 0 }
        if ($n -gt 0) { $covered++ }
        $flag = if ($n -eq 0) { 'MISSING' } elseif ($n -eq 1) { 'one only' } else { '' }
        Write-Output ("  {0}  {1,2}  {2}" -f $code, $n, $flag)
    }
    Write-Output "  covered: $covered of 15 door shapes"
}

Write-Output ""
if ($warnings.Count -eq 0) { Write-Output "No per-room warnings." }
else {
    Write-Output "Per-room warnings ($($warnings.Count)):"
    $warnings | ForEach-Object { Write-Output "  $_" }
}
