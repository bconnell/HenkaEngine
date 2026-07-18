Set-StrictMode -Version Latest

function Get-HenkaRepoRoot {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ScriptDirectory
    )

    return (Resolve-Path (Join-Path $ScriptDirectory "..")).Path
}

function Get-HenkaCMakePath {
    $cmakeCommand = Get-Command cmake.exe -ErrorAction SilentlyContinue
    if ($null -eq $cmakeCommand) {
        $cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
    }
    if ($null -ne $cmakeCommand) {
        return $cmakeCommand.Source
    }

    $vswhereCandidates = @(
        (Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"),
        (Join-Path $env:ProgramFiles "Microsoft Visual Studio\Installer\vswhere.exe")
    )

    foreach ($vswhere in $vswhereCandidates) {
        if ([string]::IsNullOrWhiteSpace($vswhere) -or -not (Test-Path -LiteralPath $vswhere -PathType Leaf)) {
            continue
        }

        $arguments = @(
            "-latest",
            "-products", "*",
            "-requires", "Microsoft.VisualStudio.Component.VC.CMake.Project",
            "-property", "installationPath"
        )
        $installationPaths = @(& $vswhere @arguments)
        if ($LASTEXITCODE -ne 0) {
            continue
        }

        foreach ($installationPath in $installationPaths) {
            if ([string]::IsNullOrWhiteSpace($installationPath)) {
                continue
            }

            $candidate = Join-Path $installationPath.Trim() "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
            if (Test-Path -LiteralPath $candidate -PathType Leaf) {
                return $candidate
            }
        }
    }

    $candidates = @(
        "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        "C:\Program Files\CMake\bin\cmake.exe"
    )

    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            return $candidate
        }
    }

    throw "CMake was not found on PATH, through vswhere, or in supported Visual Studio and standalone locations."
}

function Get-HenkaCTestPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$CMakePath
    )

    $sibling = Join-Path (Split-Path -Parent $CMakePath) "ctest.exe"
    if (Test-Path -LiteralPath $sibling -PathType Leaf) {
        return $sibling
    }

    $ctestCommand = Get-Command ctest.exe -ErrorAction SilentlyContinue
    if ($null -eq $ctestCommand) {
        $ctestCommand = Get-Command ctest -ErrorAction SilentlyContinue
    }
    if ($null -ne $ctestCommand) {
        return $ctestCommand.Source
    }

    throw "CTest was not found beside CMake or on PATH."
}

function Invoke-HenkaNative {
    param(
        [Parameter(Mandatory = $true)]
        [string]$FilePath,

        [string[]]$Arguments = @(),

        [Parameter(Mandatory = $true)]
        [string]$WorkingDirectory,

        [Parameter(Mandatory = $true)]
        [string]$Label
    )

    Write-Host ""
    Write-Host "==> $Label"
    Write-Host "    $FilePath $($Arguments -join ' ')"

    $previousErrorActionPreference = $ErrorActionPreference
    $exitCode = -1

    Push-Location $WorkingDirectory
    try {
        $ErrorActionPreference = "Continue"
        & $FilePath @Arguments 2>&1 |
            ForEach-Object {
                Write-Host ([string]$_)
            }
        $exitCode = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $previousErrorActionPreference
        Pop-Location
    }

    if ($exitCode -ne 0) {
        throw "$Label failed with exit code $exitCode."
    }
}

function Invoke-HenkaNativeCapture {
    param(
        [Parameter(Mandatory = $true)]
        [string]$FilePath,

        [string[]]$Arguments = @(),

        [Parameter(Mandatory = $true)]
        [string]$WorkingDirectory,

        [Parameter(Mandatory = $true)]
        [string]$Label
    )

    $captureRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("henka-native-" + [Guid]::NewGuid().ToString("N"))
    [System.IO.Directory]::CreateDirectory($captureRoot) | Out-Null
    $stdoutPath = Join-Path $captureRoot "stdout.log"
    $stderrPath = Join-Path $captureRoot "stderr.log"

    try {
        Write-Host ""
        Write-Host "==> $Label"
        Write-Host "    $FilePath $($Arguments -join ' ')"

        $argumentValues = @($Arguments | Where-Object { $null -ne $_ })
        if ($argumentValues.Count -ne @($Arguments).Count) {
            throw "$Label received a null native-process argument."
        }

        $startParameters = @{
            FilePath = $FilePath
            WorkingDirectory = $WorkingDirectory
            NoNewWindow = $true
            Wait = $true
            PassThru = $true
            RedirectStandardOutput = $stdoutPath
            RedirectStandardError = $stderrPath
        }
        if ($argumentValues.Count -gt 0) {
            $startParameters["ArgumentList"] = [string[]]$argumentValues
        }

        $process = Start-Process @startParameters
        $process.Refresh()
        $stdout = if (Test-Path -LiteralPath $stdoutPath) { [System.IO.File]::ReadAllText($stdoutPath) } else { "" }
        $stderr = if (Test-Path -LiteralPath $stderrPath) { [System.IO.File]::ReadAllText($stderrPath) } else { "" }

        if (-not [string]::IsNullOrWhiteSpace($stdout)) {
            Write-Host $stdout.TrimEnd()
        }
        if (-not [string]::IsNullOrWhiteSpace($stderr)) {
            Write-Host $stderr.TrimEnd()
        }

        if ($process.ExitCode -ne 0) {
            throw "$Label failed with exit code $($process.ExitCode)."
        }

        return [pscustomobject]@{
            ExitCode = $process.ExitCode
            Stdout = $stdout
            Stderr = $stderr
        }
    }
    finally {
        Remove-Item -LiteralPath $captureRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
}
