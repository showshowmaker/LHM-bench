param(
    [string]$BuildDatasetExe,
    [string]$RunExe,
    [string]$RootDir = ".\\root",
    [string]$SuiteDir = "VSIterate",
    [string]$Depths = "1,2,4,8,16,32,64",
    [int]$SiblingsPerDir = 256,
    [int]$FilesPerLeaf = 256,
    [int]$PositiveQueries = 50000,
    [int]$NegativeQueries = 50000,
    [int]$Warmup = 10000,
    [int]$Repeats = 10,
    [string]$Backends = "masstree,rocksdb",
    [switch]$IncludeNegative,
    [switch]$NoVerify
)

if ([string]::IsNullOrWhiteSpace($BuildDatasetExe) -or [string]::IsNullOrWhiteSpace($RunExe)) {
    throw "Usage: run_nsbench_full_pipeline.ps1 -BuildDatasetExe <path> -RunExe <path> [-RootDir <root>]"
}

New-Item -ItemType Directory -Force -Path $RootDir | Out-Null

& $BuildDatasetExe `
    --root $RootDir `
    --depths $Depths `
    --siblings-per-dir $SiblingsPerDir `
    --files-per-leaf $FilesPerLeaf `
    --positive-queries $PositiveQueries `
    --negative-queries $NegativeQueries

if ($LASTEXITCODE -ne 0) {
    throw "nsbench_build_dataset failed"
}

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
& (Join-Path $scriptDir "run_nsbench_depth_sweep.ps1") `
    -ExePath $RunExe `
    -DatasetRoot (Join-Path (Join-Path $RootDir $SuiteDir) "datasets") `
    -ArtifactRoot $RootDir `
    -SuiteDir $SuiteDir `
    -Backends $Backends `
    -Warmup $Warmup `
    -Repeats $Repeats `
    -IncludeNegative:$IncludeNegative `
    -NoVerify:$NoVerify

if ($LASTEXITCODE -ne 0) {
    throw "run_nsbench_depth_sweep failed"
}
