param(
    [string]$ExePath,
    [string]$DatasetRoot = ".\\datasets",
    [string]$ArtifactRoot = ".\\root",
    [string]$Backends = "masstree,rocksdb",
    [string]$SuiteDir = "VSIterate",
    [int]$Warmup = 10000,
    [int]$Repeats = 10,
    [switch]$IncludeNegative,
    [switch]$NoVerify
)

if ([string]::IsNullOrWhiteSpace($ExePath)) {
    throw "ExePath is required, for example: -ExePath .\\build\\nsbench_run"
}

$runRoot = Join-Path $ArtifactRoot $SuiteDir
New-Item -ItemType Directory -Force -Path $runRoot | Out-Null

$manifestList = Join-Path $runRoot "manifest_list.txt"
Get-ChildItem -Path $DatasetRoot -Directory |
    Where-Object { $_.Name -like "depth_*" } |
    Sort-Object Name |
    ForEach-Object { Join-Path $_.FullName "manifest.txt" } |
    Where-Object { Test-Path $_ } |
    Set-Content -Path $manifestList

$backendItems = $Backends.Split(",") | ForEach-Object { $_.Trim() } | Where-Object { $_ -ne "" }
foreach ($backend in $backendItems) {

    $csvPath = Join-Path $runRoot ($backend + "_depth_sweep.csv")
    $args = @(
        "--backend", $backend,
        "--manifest-list", $manifestList,
        "--artifact-root", $ArtifactRoot,
        "--warmup", $Warmup,
        "--repeats", $Repeats,
        "--output-csv", $csvPath
    )
    if ($IncludeNegative) {
        $args += "--include-negative"
    }
    if ($NoVerify) {
        $args += "--no-verify"
    }
    & $ExePath @args
    if ($LASTEXITCODE -ne 0) {
        throw "nsbench_run failed for backend=$backend"
    }
}
