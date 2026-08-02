param(
    [ValidateRange(1, 100)]
    [int]$Iterations = 10
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "henka_script_common.ps1")

$repoRoot = Get-HenkaRepoRoot -ScriptDirectory $PSScriptRoot
$executable = Join-Path $repoRoot "out\HenkaSandbox3D\HenkaSandbox3D.exe"
$logRoot = Join-Path $repoRoot "build\test_tmp\packaged-soak"
if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
    throw "Packaged sandbox executable is missing: $executable"
}
[System.IO.Directory]::CreateDirectory($logRoot) | Out-Null

for ($iteration = 1; $iteration -le $Iterations; ++$iteration) {
    $stdoutPath = Join-Path $logRoot ("iteration-{0}.stdout.log" -f $iteration)
    $stderrPath = Join-Path $logRoot ("iteration-{0}.stderr.log" -f $iteration)
    Remove-Item -LiteralPath $stdoutPath, $stderrPath -Force -ErrorAction SilentlyContinue
    $capturedProcess = $null
    try {
        $capturedProcess = Start-HenkaCapturedProcess `
            -FilePath $executable `
            -Arguments @("--smoke-test") `
            -WorkingDirectory (Split-Path -Parent $executable) `
            -StdoutPath $stdoutPath `
            -StderrPath $stderrPath `
            -CreateNoWindow
        if (-not $capturedProcess.WaitForExit(-1)) {
            throw "Packaged sandbox smoke iteration $iteration did not exit."
        }
        $exitCode = $capturedProcess.Process.ExitCode
        $output = (Read-HenkaSharedText -Path $stdoutPath) + (Read-HenkaSharedText -Path $stderrPath)
    }
    finally {
        Close-HenkaCapturedProcess -CapturedProcess $capturedProcess
    }
    if ($exitCode -ne 0) {
        throw "Packaged sandbox smoke iteration $iteration failed with exit code $exitCode."
    }
    if ($output -notmatch "Sandbox smoke test completed\.") {
        throw "Packaged sandbox smoke iteration $iteration did not reach its completion marker."
    }
    if ($output -notmatch "memory shutdown clean: no active allocations tracked") {
        throw "Packaged sandbox smoke iteration $iteration did not report a clean memory shutdown."
    }
}

Write-Host "[pass] Packaged sandbox bounded soak completed: $Iterations iterations; clean memory shutdown reported for each run."
