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

function Get-HenkaGitPath {
    $gitCommand = Get-Command git.exe -ErrorAction SilentlyContinue
    if ($null -eq $gitCommand) {
        $gitCommand = Get-Command git -ErrorAction SilentlyContinue
    }
    if ($null -eq $gitCommand) {
        throw "Git was not found on PATH."
    }
    return $gitCommand.Source
}

function Write-HenkaUtf8NoBom {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,

        [Parameter(Mandatory = $true)]
        [AllowEmptyString()]
        [string]$Text
    )

    $encoding = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($Path, $Text, $encoding)
}

function ConvertTo-HenkaNativeArgument {
    param(
        [Parameter(Mandatory = $true)]
        [AllowEmptyString()]
        [string]$Value
    )

    if ($Value.Length -gt 0 -and $Value -notmatch '[\s"]') {
        return $Value
    }

    $builder = New-Object System.Text.StringBuilder
    [void]$builder.Append('"')
    $backslashCount = 0

    foreach ($character in $Value.ToCharArray()) {
        if ($character -eq '\') {
            $backslashCount++
            continue
        }

        if ($character -eq '"') {
            if ($backslashCount -gt 0) {
                [void]$builder.Append((('\' * ($backslashCount * 2)) -join ''))
                $backslashCount = 0
            }
            [void]$builder.Append('\"')
            continue
        }

        if ($backslashCount -gt 0) {
            [void]$builder.Append((('\' * $backslashCount) -join ''))
            $backslashCount = 0
        }
        [void]$builder.Append($character)
    }

    if ($backslashCount -gt 0) {
        [void]$builder.Append((('\' * ($backslashCount * 2)) -join ''))
    }
    [void]$builder.Append('"')
    return $builder.ToString()
}

function ConvertTo-HenkaNativeArgumentString {
    param([string[]]$Arguments = @())

    $values = @($Arguments | Where-Object { $null -ne $_ })
    if ($values.Count -ne @($Arguments).Count) {
        throw "A null native-process argument was provided."
    }

    return (@($values | ForEach-Object {
        ConvertTo-HenkaNativeArgument -Value ([string]$_)
    }) -join " ")
}

function Initialize-HenkaCapturedProcessType {
    if ($null -ne ("HenkaCapturedProcess" -as [type])) {
        return
    }

    Add-Type -TypeDefinition @'
using System;
using System.Diagnostics;
using System.IO;
using System.Text;

public sealed class HenkaCapturedProcess : IDisposable
{
    private readonly object stdoutLock = new object();
    private readonly object stderrLock = new object();
    private readonly StreamWriter stdoutWriter;
    private readonly StreamWriter stderrWriter;
    private bool disposed;

    public Process Process { get; private set; }

    private HenkaCapturedProcess(
        Process process,
        StreamWriter stdoutWriter,
        StreamWriter stderrWriter)
    {
        Process = process;
        this.stdoutWriter = stdoutWriter;
        this.stderrWriter = stderrWriter;
        Process.OutputDataReceived += OnOutputDataReceived;
        Process.ErrorDataReceived += OnErrorDataReceived;
    }

    public static HenkaCapturedProcess Start(
        string filePath,
        string arguments,
        string workingDirectory,
        string stdoutPath,
        string stderrPath,
        bool createNoWindow)
    {
        ProcessStartInfo startInfo = new ProcessStartInfo();
        startInfo.FileName = filePath;
        startInfo.Arguments = arguments ?? String.Empty;
        startInfo.WorkingDirectory = workingDirectory;
        startInfo.UseShellExecute = false;
        startInfo.CreateNoWindow = createNoWindow;
        startInfo.RedirectStandardOutput = true;
        startInfo.RedirectStandardError = true;

        UTF8Encoding encoding = new UTF8Encoding(false);
        StreamWriter stdoutWriter = new StreamWriter(stdoutPath, false, encoding);
        StreamWriter stderrWriter = new StreamWriter(stderrPath, false, encoding);
        stdoutWriter.AutoFlush = true;
        stderrWriter.AutoFlush = true;

        Process process = new Process();
        process.StartInfo = startInfo;
        HenkaCapturedProcess capture = new HenkaCapturedProcess(
            process,
            stdoutWriter,
            stderrWriter);

        try
        {
            if (!process.Start())
            {
                throw new InvalidOperationException("The process did not start.");
            }
            process.BeginOutputReadLine();
            process.BeginErrorReadLine();
            return capture;
        }
        catch
        {
            capture.Dispose();
            throw;
        }
    }

    private void OnOutputDataReceived(object sender, DataReceivedEventArgs eventArgs)
    {
        if (eventArgs.Data == null)
        {
            return;
        }
        lock (stdoutLock)
        {
            stdoutWriter.WriteLine(eventArgs.Data);
        }
    }

    private void OnErrorDataReceived(object sender, DataReceivedEventArgs eventArgs)
    {
        if (eventArgs.Data == null)
        {
            return;
        }
        lock (stderrLock)
        {
            stderrWriter.WriteLine(eventArgs.Data);
        }
    }

    public bool WaitForExit(int timeoutMilliseconds)
    {
        bool exited = Process.WaitForExit(timeoutMilliseconds);
        if (exited)
        {
            Process.WaitForExit();
            lock (stdoutLock) { stdoutWriter.Flush(); }
            lock (stderrLock) { stderrWriter.Flush(); }
        }
        return exited;
    }

    public void Kill()
    {
        if (!Process.HasExited)
        {
            Process.Kill();
            Process.WaitForExit();
        }
    }

    public void Dispose()
    {
        if (disposed)
        {
            return;
        }
        disposed = true;

        try
        {
            if (Process != null && !Process.HasExited)
            {
                Process.Kill();
                Process.WaitForExit();
            }
            if (Process != null)
            {
                Process.WaitForExit();
            }
        }
        catch
        {
        }

        if (Process != null)
        {
            Process.OutputDataReceived -= OnOutputDataReceived;
            Process.ErrorDataReceived -= OnErrorDataReceived;
        }

        lock (stdoutLock) { stdoutWriter.Dispose(); }
        lock (stderrLock) { stderrWriter.Dispose(); }

        if (Process != null)
        {
            Process.Dispose();
        }
    }
}
'@
}

function Start-HenkaProcess {
    param(
        [Parameter(Mandatory = $true)]
        [string]$FilePath,

        [string[]]$Arguments = @(),

        [Parameter(Mandatory = $true)]
        [string]$WorkingDirectory,

        [switch]$CreateNoWindow
    )

    $startInfo = New-Object System.Diagnostics.ProcessStartInfo
    $startInfo.FileName = $FilePath
    $startInfo.Arguments = ConvertTo-HenkaNativeArgumentString -Arguments $Arguments
    $startInfo.WorkingDirectory = $WorkingDirectory
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = [bool]$CreateNoWindow

    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $startInfo
    if (-not $process.Start()) {
        $process.Dispose()
        throw "The process did not start: $FilePath"
    }
    return $process
}

function Start-HenkaCapturedProcess {
    param(
        [Parameter(Mandatory = $true)]
        [string]$FilePath,

        [string[]]$Arguments = @(),

        [Parameter(Mandatory = $true)]
        [string]$WorkingDirectory,

        [Parameter(Mandatory = $true)]
        [string]$StdoutPath,

        [Parameter(Mandatory = $true)]
        [string]$StderrPath,

        [switch]$CreateNoWindow
    )

    Initialize-HenkaCapturedProcessType
    $stdoutDirectory = Split-Path -Parent $StdoutPath
    $stderrDirectory = Split-Path -Parent $StderrPath
    if (-not [string]::IsNullOrWhiteSpace($stdoutDirectory)) {
        [System.IO.Directory]::CreateDirectory($stdoutDirectory) | Out-Null
    }
    if (-not [string]::IsNullOrWhiteSpace($stderrDirectory)) {
        [System.IO.Directory]::CreateDirectory($stderrDirectory) | Out-Null
    }

    return [HenkaCapturedProcess]::Start(
        $FilePath,
        (ConvertTo-HenkaNativeArgumentString -Arguments $Arguments),
        $WorkingDirectory,
        $StdoutPath,
        $StderrPath,
        [bool]$CreateNoWindow)
}

function Close-HenkaCapturedProcess {
    param($CapturedProcess)

    if ($null -ne $CapturedProcess) {
        $CapturedProcess.Dispose()
    }
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

function Read-HenkaSharedText {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return ""
    }

    $stream = $null
    $reader = $null
    try {
        $share = [System.IO.FileShare]::ReadWrite -bor [System.IO.FileShare]::Delete
        $stream = [System.IO.File]::Open(
            $Path,
            [System.IO.FileMode]::Open,
            [System.IO.FileAccess]::Read,
            $share)
        $reader = New-Object System.IO.StreamReader(
            $stream,
            [System.Text.Encoding]::UTF8,
            $true)
        return $reader.ReadToEnd()
    }
    finally {
        if ($null -ne $reader) {
            $reader.Dispose()
        }
        elseif ($null -ne $stream) {
            $stream.Dispose()
        }
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
    $capturedProcess = $null

    try {
        Write-Host ""
        Write-Host "==> $Label"
        Write-Host "    $FilePath $($Arguments -join ' ')"

        $capturedProcess = Start-HenkaCapturedProcess `
            -FilePath $FilePath `
            -Arguments $Arguments `
            -WorkingDirectory $WorkingDirectory `
            -StdoutPath $stdoutPath `
            -StderrPath $stderrPath `
            -CreateNoWindow

        if (-not $capturedProcess.WaitForExit(-1)) {
            throw "$Label did not exit."
        }

        $exitCode = $capturedProcess.Process.ExitCode
        $stdout = Read-HenkaSharedText -Path $stdoutPath
        $stderr = Read-HenkaSharedText -Path $stderrPath

        if (-not [string]::IsNullOrWhiteSpace($stdout)) {
            Write-Host $stdout.TrimEnd()
        }
        if (-not [string]::IsNullOrWhiteSpace($stderr)) {
            Write-Host $stderr.TrimEnd()
        }

        if ($exitCode -ne 0) {
            throw "$Label failed with exit code $exitCode."
        }

        return [pscustomobject]@{
            ExitCode = $exitCode
            Stdout = $stdout
            Stderr = $stderr
        }
    }
    finally {
        Close-HenkaCapturedProcess -CapturedProcess $capturedProcess
        Remove-Item -LiteralPath $captureRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
}
