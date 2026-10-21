# Builds and runs every standalone C++ test suite.
#
#   powershell -ExecutionPolicy Bypass -File tools\run_tests.ps1            # all suites
#   powershell -ExecutionPolicy Bypass -File tools\run_tests.ps1 -Suite RunProgression,OpeningBalance
#   powershell -ExecutionPolicy Bypass -File tools\run_tests.ps1 -List
#
# Each invocation compiles into a NEW folder under x64\TestRuns\, so a locked or
# read-only object file from an earlier run can never fail the build. Every
# suite runs with an isolated LOCALAPPDATA/TEMP and with any inherited
# MO_PROFILE_DIR override removed, so no test can touch a real profile.
# Every *Tests.cpp file must belong to a suite below; an unregistered one fails
# the run before anything is built, so a new suite cannot be silently skipped.
param(
    [string[]]$Suite,
    [switch]$List,
    [string]$VsInstall,
    [string]$RaylibDir,
    [switch]$IncludeContent   # also run the slow full-room-library validation
)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\MoToolchain.ps1"
$projectRoot = Split-Path $PSScriptRoot -Parent

# Room-content sources shared by the room/library suites.
$room = @('RoomLibrary.cpp','DungeonGen.cpp','TileRenderer.cpp','RoomBlueprint.cpp','RoomLayout.cpp',
          'RoomCapacity.cpp','RoomCollision.cpp','TileDefs.cpp','RoomAssetCatalog.cpp')

# Name, sources (TestGame-relative unless rooted), preprocessor defines, and
# whether raylib must be linked. 'Cwd' = 'sandbox' runs inside the suite's
# private folder (for suites that write files relative to the working dir).
$suites = @(
    @{ Name='JourneyTransition';   Sources=@('JourneyTransitionTests.cpp') }
    @{ Name='TextureCache';        Sources=@('TextureCacheTests.cpp') }
    @{ Name='AudioTransition';     Sources=@('AudioTransitionTests.cpp','AudioManager.cpp') }
    @{ Name='AudioMusicRouting';   Sources=@('AudioMusicRoutingTests.cpp','AudioManager.cpp'); Raylib=$true }
    @{ Name='OpeningBalance';      Sources=@('OpeningBalanceTests.cpp','PlayerClass.cpp') }
    @{ Name='RunProgression';      Sources=@('RunProgressionTests.cpp','PlayerClass.cpp'); Raylib=$true }
    @{ Name='DamagePolicy';        Sources=@('DamagePolicyTests.cpp') }
    @{ Name='RelicBalance';        Sources=@('RelicBalanceTests.cpp','Relic.cpp','PlayerClass.cpp') }
    @{ Name='MetaProgression';     Sources=@('..\Tests\MetaProgressionTests.cpp','MetaProgression.cpp','SaveIds.cpp','SaveStore.cpp','Json.cpp','Sha256.cpp','Grimoire.cpp','Relic.cpp','PlayerClass.cpp'); Cwd='sandbox' }
    @{ Name='UserDataPath';        Sources=@('UserDataPathTests.cpp') }
    @{ Name='Sha256';              Sources=@('Sha256Tests.cpp','Sha256.cpp') }
    @{ Name='Json';                Sources=@('JsonTests.cpp','Json.cpp') }
    @{ Name='SaveStore';           Sources=@('SaveStoreTests.cpp','SaveStore.cpp','Json.cpp','Sha256.cpp'); Cwd='sandbox' }
    @{ Name='RunCheckpoint';       Sources=@('RunCheckpointTests.cpp','RunCheckpoint.cpp','SaveStore.cpp','Json.cpp','Sha256.cpp','SaveIds.cpp','Grimoire.cpp','Relic.cpp','PlayerClass.cpp'); Cwd='sandbox' }
    @{ Name='RunLog';              Sources=@('RunLogTests.cpp','RunLog.cpp','SaveStore.cpp','Json.cpp','Sha256.cpp','SaveIds.cpp','Grimoire.cpp','Relic.cpp','PlayerClass.cpp'); Cwd='sandbox' }
    @{ Name='TileRendererResource';Sources=@('TileRendererResourceTests.cpp','TileRenderer.cpp','RoomAssetCatalog.cpp','TileDefs.cpp') }
    @{ Name='ClassUnlockRules';    Sources=@('ClassUnlockRulesTests.cpp','ClassUnlockRules.cpp') }
    @{ Name='CombatPresentation';  Sources=@('CombatPresentationTests.cpp') }
    @{ Name='CombatRhythm';        Sources=@('CombatRhythmTests.cpp','CombatEngagement.cpp','ReinforcementPacing.cpp','CombatSeparation.cpp'); Raylib=$true }
    @{ Name='CombatSystems';       Sources=@('CombatSystemsTests.cpp','DamageNumberManager.cpp','EncounterPlanner.cpp'); Defines=@('COMBAT_SYSTEMS_TEST_MAIN'); Raylib=$true }
    @{ Name='ContentCore';         Sources=@('ContentCoreTests.cpp','ContentDocument.cpp','ContentTypes.cpp','ContentSerializers.cpp','ContentValidation.cpp','ContentDatabase.cpp'); Defines=@('CONTENT_CORE_TEST_MAIN'); Cwd='sandbox' }
    @{ Name='DayNight';            Sources=@('DayNightTests.cpp','DayNight.cpp') }
    @{ Name='CharacterTuning';     Sources=@('CharacterTuningTests.cpp','CharacterTuning.cpp','AttackTuning.cpp') }
    @{ Name='DungeonGenEntrance';  Sources=@('DungeonGenEntranceTests.cpp','DungeonGen.cpp'); Raylib=$true }
    @{ Name='EliteSignature';      Sources=@('EliteSignatureTests.cpp','EliteSignature.cpp','EncounterPattern.cpp'); Defines=@('ELITE_SIGNATURE_TEST_MAIN') }
    @{ Name='EncounterPlanner';    Sources=@('EncounterPlannerTests.cpp','EncounterPlanner.cpp') + $room; Raylib=$true }
    @{ Name='PrologueController';  Sources=@('PrologueControllerTests.cpp','PrologueController.cpp') }
    @{ Name='RunSession';          Sources=@('RunSessionTests.cpp','RunSession.cpp') }
    @{ Name='Credits';             Sources=@('CreditsTests.cpp') }
    @{ Name='Grimoire';            Sources=@('GrimoireTests.cpp','Grimoire.cpp','Relic.cpp','PlayerClass.cpp') }
    @{ Name='SettingsDefaults';    Sources=@('SettingsDefaultsTests.cpp','SettingsManager.cpp','SaveStore.cpp','Json.cpp','Sha256.cpp'); Raylib=$true; Cwd='sandbox' }
    @{ Name='VillageLayoutData';   Sources=@('VillageLayoutDataTests.cpp','VillageLayoutData.cpp'); Raylib=$true }
    @{ Name='RoomCollision';       Sources=@('RoomCollisionTests.cpp') + $room; Raylib=$true }
    @{ Name='RoomCapacity';        Sources=@('RoomCapacityTests.cpp') + $room; Raylib=$true }
    @{ Name='RoomBlueprint';       Sources=@('RoomBlueprintTests.cpp') + $room; Raylib=$true }
    @{ Name='RoomAssetCatalog';    Sources=@('RoomAssetCatalogTests.cpp') + $room; Raylib=$true }
    @{ Name='RoomLibrary';         Sources=@('RoomLibraryTests.cpp') + $room; Raylib=$true }
    @{ Name='RoomEditor';          Sources=@('RoomEditorTests.cpp','RoomEditor.cpp') + $room; Raylib=$true }
    @{ Name='TileAssetMetadata';   Sources=@('TileAssetMetadataTests.cpp') + $room; Raylib=$true }
    @{ Name='TileRenderSource';    Sources=@('TileRenderSourceTests.cpp') + $room; Raylib=$true }
    @{ Name='ForestVariants';      Sources=@('ForestVariantValidationTests.cpp') + $room; Raylib=$true }
    @{ Name='RoomSelection';       Sources=@('RoomSelectionTests.cpp') + $room; Raylib=$true }
    @{ Name='BiomeRooms';          Sources=@('BiomeRoomTests.cpp') + $room; Raylib=$true }
    @{ Name='GeneratedVariants';   Sources=@('GeneratedVariantValidationTests.cpp') + $room; Raylib=$true; Content=$true }
)

# Discovery check: every test source on disk must be registered above.
$registered = @($suites | ForEach-Object { $_.Sources } | ForEach-Object { Split-Path $_ -Leaf })
$testSources = @(Get-ChildItem -Path (Join-Path $projectRoot 'TestGame'), (Join-Path $projectRoot 'Tests') `
                     -Filter '*Tests.cpp' -File -ErrorAction SilentlyContinue | ForEach-Object Name)
$unregistered = @($testSources | Where-Object { $registered -notcontains $_ })
if ($unregistered) { throw "Test source(s) not registered in tools\run_tests.ps1: $($unregistered -join ', ')" }

if ($List) { $suites | ForEach-Object { $_.Name }; return }
# "-Suite A,B" arrives as one string when the script is run with -File.
if ($Suite) { $Suite = @($Suite | ForEach-Object { $_ -split ',' } | ForEach-Object { $_.Trim() } | Where-Object { $_ }) }
if ($Suite) {
    $unknown = $Suite | Where-Object { $n = $_; -not ($suites | Where-Object { $_.Name -eq $n }) }
    if ($unknown) { throw "Unknown suite(s): $($unknown -join ', '). Use -List." }
    $suites = @($suites | Where-Object { $Suite -contains $_.Name })
} elseif (-not $IncludeContent) {
    $suites = @($suites | Where-Object { -not $_.Content })
}

$vs = Resolve-MoVisualStudio $VsInstall
$raylib = Resolve-MoRaylib $RaylibDir
Import-MoVcVars $vs

$runRoot = Join-Path $projectRoot ('x64\TestRuns\' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
New-Item -ItemType Directory -Path $runRoot | Out-Null
$results = @()
$previousLocalData = $env:LOCALAPPDATA
$previousTemp = $env:TEMP
$previousTmp = $env:TMP
# UserDataPath gives MO_PROFILE_DIR precedence over LOCALAPPDATA, so an
# override inherited from a playtest shell would point suites at that profile.
$previousProfileDir = $env:MO_PROFILE_DIR
Remove-Item Env:MO_PROFILE_DIR -ErrorAction SilentlyContinue
try {
    foreach ($s in $suites) {
        $dir = Join-Path $runRoot $s.Name
        New-Item -ItemType Directory -Path $dir | Out-Null
        $sources = $s.Sources | ForEach-Object { Join-Path $projectRoot (Join-Path 'TestGame' $_) }
        $missing = $sources | Where-Object { -not (Test-Path -LiteralPath $_) }
        if ($missing) {
            $results += [pscustomobject]@{ Suite=$s.Name; Result='MISSING'; Detail=($missing -join ', ') }
            continue
        }
        # /UNDEBUG keeps assert() live even if the CL environment variable adds /DNDEBUG.
        $args = @('/nologo','/std:c++17','/EHsc','/MD','/O1','/utf-8','/wd4819','/UNDEBUG',
                  '/I', (Join-Path $projectRoot 'TestGame'), '/I', (Join-Path $raylib 'include'))
        foreach ($d in @($s.Defines)) { if ($d) { $args += "/D$d" } }
        $args += $sources
        $args += @("/Fo$dir\", "/Fe$dir\$($s.Name).exe", '/link', '/INCREMENTAL:NO', '/OPT:REF')
        if ($s.Raylib) {
            $args += @("/LIBPATH:$(Join-Path $raylib 'lib')", 'raylib.lib','winmm.lib','gdi32.lib','opengl32.lib','user32.lib','shell32.lib')
        }
        $buildLog = Join-Path $dir 'build.log'
        & cl.exe @args *> $buildLog
        if ($LASTEXITCODE -ne 0) {
            $results += [pscustomobject]@{ Suite=$s.Name; Result='BUILD FAILED'; Detail=$buildLog }
            continue
        }
        $env:LOCALAPPDATA = Join-Path $dir 'local-app-data'
        New-Item -ItemType Directory -Path $env:LOCALAPPDATA | Out-Null
        $env:TEMP = Join-Path $dir 'temp'
        $env:TMP = $env:TEMP
        New-Item -ItemType Directory -Path $env:TEMP | Out-Null
        $cwd = if ($s.Cwd -eq 'sandbox') { $dir } else { $projectRoot }
        $runLog = Join-Path $dir 'run.log'
        Push-Location $cwd
        try { & (Join-Path $dir "$($s.Name).exe") *> $runLog; $code = $LASTEXITCODE } finally { Pop-Location }
        $env:LOCALAPPDATA = $previousLocalData
        $env:TEMP = $previousTemp
        $env:TMP = $previousTmp
        $last = (Get-Content -LiteralPath $runLog -Tail 1 -ErrorAction SilentlyContinue)
        $results += [pscustomobject]@{ Suite=$s.Name; Result=($(if ($code -eq 0) { 'PASS' } else { "FAIL ($code)" })); Detail=$last }
    }
} finally {
    $env:LOCALAPPDATA = $previousLocalData
    $env:TEMP = $previousTemp
    $env:TMP = $previousTmp
    if ($null -ne $previousProfileDir) { $env:MO_PROFILE_DIR = $previousProfileDir }
}

$results | Format-Table -AutoSize -Wrap | Out-String -Width 200 | Write-Output
Write-Output "Logs: $runRoot"
$failed = @($results | Where-Object Result -ne 'PASS')
if ($failed.Count -gt 0) { Write-Output "$($failed.Count) of $($results.Count) suite(s) did not pass."; exit 1 }
Write-Output "ALL $($results.Count) SUITES PASSED"
