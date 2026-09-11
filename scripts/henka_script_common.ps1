Set-StrictMode -Version Latest

function Get-HenkaRepoRoot {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ScriptDirectory
    )

    return (Resolve-Path (Join-Path $ScriptDirectory "..")).Path
}

function Write-HenkaGeneratedRootMarker {
    param(
        [Parameter(Mandatory = $true)] [string]$RepoRoot,
        [Parameter(Mandatory = $true)] [string]$Path,
        [Parameter(Mandatory = $true)] [string]$Purpose,
        [Parameter(Mandatory = $true)] [ValidateSet("SCRATCH", "ACTIVE_CANDIDATE", "PUBLISHED_BOUNDARY_EVIDENCE", "NEGATIVE_CONTROL", "CACHE")]
        [string]$RetentionClass,
        [Parameter(Mandatory = $true)] [bool]$Active,
        [Parameter(Mandatory = $true)] [bool]$CleanupEligible,
        [Parameter(Mandatory = $true)] [string]$CleanupCondition,
        [string]$Configuration = "n/a"
    )

    $repo = (Resolve-Path -LiteralPath $RepoRoot).Path.TrimEnd("\")
    $fullPath = [System.IO.Path]::GetFullPath($Path)
    $buildRoot = [System.IO.Path]::GetFullPath((Join-Path $repo "build")).TrimEnd("\")
    $outRoot = [System.IO.Path]::GetFullPath((Join-Path $repo "out")).TrimEnd("\")
    $underBuild = $fullPath.StartsWith($buildRoot + [System.IO.Path]::DirectorySeparatorChar, [System.StringComparison]::OrdinalIgnoreCase)
    $underOut = $fullPath.StartsWith($outRoot + [System.IO.Path]::DirectorySeparatorChar, [System.StringComparison]::OrdinalIgnoreCase)
    if (-not $underBuild -and -not $underOut) {
        throw "Generated root marker path must be under the repository build or out root: $fullPath"
    }
    [System.IO.Directory]::CreateDirectory($fullPath) | Out-Null
    $item = Get-Item -LiteralPath $fullPath -Force
    if (($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
        throw "Generated root marker path is a reparse point: $fullPath"
    }
    $git = Get-HenkaGitPath
    $sourceSha = ([string](& $git -C $repo rev-parse HEAD 2>$null)).Trim()
    if ($LASTEXITCODE -ne 0 -or $sourceSha -notmatch "^[0-9a-fA-F]{40}$") {
        throw "Could not resolve the source commit for generated root marker: $fullPath"
    }
    $marker = [ordered]@{
        schema_version = 1
        owner = "Henka repository validation"
        purpose = $Purpose
        source_sha = $sourceSha
        configuration = $Configuration
        created_utc = [DateTime]::UtcNow.ToString("o")
        retention_class = $RetentionClass
        cleanup_owner = "Henka repository validation"
        cleanup_condition = $CleanupCondition
        active = $Active
        cleanup_eligible = $CleanupEligible
    }
    $encoding = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText(
        (Join-Path $fullPath ".henka-generated.json"),
        (($marker | ConvertTo-Json -Depth 4) + [Environment]::NewLine),
        $encoding)
    return (Join-Path $fullPath ".henka-generated.json")
}

function Get-HenkaToolVersionLine {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,

        [Parameter(Mandatory = $true)]
        [ValidateSet("cmake", "ctest")]
        [string]$ToolName
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "$ToolName executable does not exist: $Path"
    }
    $output = @(& $Path --version 2>&1)
    if ($LASTEXITCODE -ne 0) {
        throw "$ToolName did not report a valid version: $Path"
    }
    $prefix = "$ToolName version "
    $versionLine = @($output | ForEach-Object { [string]$_ } |
        Where-Object { $_ -like "$prefix*" } | Select-Object -First 1)
    if ($versionLine.Count -ne 1 -or [string]::IsNullOrWhiteSpace($versionLine[0])) {
        throw "$ToolName reported an unrecognized version: $Path"
    }
    return $versionLine[0].Trim()
}

function Get-HenkaToolchain {
    param(
        [string]$CMakePath = ""
    )

    $candidates = @()
    if (-not [string]::IsNullOrWhiteSpace($CMakePath)) {
        $candidates += [System.IO.Path]::GetFullPath($CMakePath)
    }
    else {
        $cmakeCommand = Get-Command cmake.exe -ErrorAction SilentlyContinue
        if ($null -eq $cmakeCommand) {
            $cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
        }
        if ($null -ne $cmakeCommand -and
            -not [string]::IsNullOrWhiteSpace([string]$cmakeCommand.Source)) {
            $candidates += [System.IO.Path]::GetFullPath([string]$cmakeCommand.Source)
        }

        $vswhereCandidates = @(
            (Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"),
            (Join-Path $env:ProgramFiles "Microsoft Visual Studio\Installer\vswhere.exe")
        )
        foreach ($vswhere in $vswhereCandidates) {
            if ([string]::IsNullOrWhiteSpace($vswhere) -or
                -not (Test-Path -LiteralPath $vswhere -PathType Leaf)) {
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
                if (-not [string]::IsNullOrWhiteSpace([string]$installationPath)) {
                    $candidates += Join-Path ([string]$installationPath).Trim() `
                        "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
                }
            }
        }

        $candidates += @(
            "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
            "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
            "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
            "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
            "C:\Program Files\CMake\bin\cmake.exe"
        )
    }

    $failures = @()
    foreach ($candidate in @($candidates | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
            ForEach-Object { [System.IO.Path]::GetFullPath([string]$_) } | Select-Object -Unique)) {
        if (-not (Test-Path -LiteralPath $candidate -PathType Leaf)) {
            $failures += "$candidate (missing)"
            continue
        }
        $ctest = Join-Path (Split-Path -Parent $candidate) "ctest.exe"
        if (-not (Test-Path -LiteralPath $ctest -PathType Leaf)) {
            $failures += "$candidate (paired ctest.exe missing)"
            continue
        }
        try {
            $cmakeVersion = Get-HenkaToolVersionLine -Path $candidate -ToolName "cmake"
            $ctestVersion = Get-HenkaToolVersionLine -Path $ctest -ToolName "ctest"
            $cmakeValue = $cmakeVersion.Substring("cmake version ".Length)
            $ctestValue = $ctestVersion.Substring("ctest version ".Length)
            if ($cmakeValue -ne $ctestValue) {
                throw "CMake/CTest version mismatch ($cmakeValue versus $ctestValue)."
            }
            return [pscustomobject]@{
                CMakePath = $candidate
                CTestPath = $ctest
                CMakeVersion = $cmakeVersion
                CTestVersion = $ctestVersion
            }
        }
        catch {
            $failures += "$candidate ($($_.Exception.Message))"
        }
    }

    $details = if ($failures.Count -eq 0) { "no supported candidates were found" } else { $failures -join "; " }
    throw "Henka could not resolve a compatible CMake/CTest pair: $details"
}

function Get-HenkaCMakePath {
    return (Get-HenkaToolchain).CMakePath
}

function Get-HenkaCTestPath {
    param(
        [string]$CMakePath = ""
    )

    if ([string]::IsNullOrWhiteSpace($CMakePath)) {
        return (Get-HenkaToolchain).CTestPath
    }
    return (Get-HenkaToolchain -CMakePath $CMakePath).CTestPath
}

function Get-HenkaBuildArtifact {
    param(
        [Parameter(Mandatory = $true)] [string]$BuildRoot,
        [Parameter(Mandatory = $true)] [ValidateSet("Debug", "Release")][string]$Configuration,
        [string]$BuildTarget = ""
    )

    $target = [string]$BuildTarget
    if ([string]::IsNullOrWhiteSpace($target) -or $target -eq "henka_sandbox3d") {
        return [pscustomobject]@{
            Path = Join-Path $BuildRoot "examples\sandbox3d\$Configuration\henka_sandbox3d.exe"
            Kind = "executable"
            Target = if ([string]::IsNullOrWhiteSpace($target)) { "default" } else { $target }
        }
    }
    if ($target -eq "henka" -or $target -eq "henka_runtime") {
        return [pscustomobject]@{
            Path = Join-Path $BuildRoot "engine\$Configuration\$target.lib"
            Kind = "static-library"
            Target = $target
        }
    }
    if ($target.EndsWith("_tests", [System.StringComparison]::OrdinalIgnoreCase)) {
        return [pscustomobject]@{
            Path = Join-Path $BuildRoot "tests\$Configuration\$target.exe"
            Kind = "executable"
            Target = $target
        }
    }
    throw "No target-aware provenance artifact mapping exists for CMake target '$target'. Supply a supported target or extend the explicit mapping."
}

function Enter-HenkaBuildStateLock {
    param(
        [ValidateRange(0, 3600)]
        [int]$TimeoutSeconds = 900
    )

    $mutexName = "Local\HenkaEngineSharedGeneratedBuildState"
    $mutex = New-Object System.Threading.Mutex($false, $mutexName)
    $acquired = $false
    try {
        try {
            $acquired = $mutex.WaitOne([TimeSpan]::FromSeconds($TimeoutSeconds))
        } catch [System.Threading.AbandonedMutexException] {
            $acquired = $true
            Write-Warning "Recovered an abandoned shared generated build-state lock."
        }
        if (-not $acquired) {
            throw "Could not acquire the shared generated build-state lock within $TimeoutSeconds seconds. Another Henka build/configure operation may be using the shared generated tree."
        }
        return [pscustomobject]@{
            Mutex = $mutex
            Name = $mutexName
        }
    } catch {
        $mutex.Dispose()
        throw
    }
}

function Exit-HenkaBuildStateLock {
    param(
        [Parameter(Mandatory = $true)]
        [pscustomobject]$Lock
    )

    try {
        $Lock.Mutex.ReleaseMutex()
    } finally {
        $Lock.Mutex.Dispose()
    }
}

function Get-HenkaCMakeFetchContentArguments {
    param(
        [Parameter(Mandatory = $true)]
        [AllowEmptyString()]
        [string]$DependencyRoot,

        [ValidateSet("SDL3", "KTXSOFTWARE", "ENET", "LUA", "MINIAUDIO", "STB")]
        [string[]]$Providers = @("SDL3", "KTXSOFTWARE", "ENET", "LUA", "MINIAUDIO", "STB"),

        [switch]$NoLocalProviders
    )

    $definitions = @{
        SDL3 = [pscustomobject]@{
            CacheName = "SDL3"
            RelativePath = "sdl3-src"
            Marker = "CMakeLists.txt"
            Label = "SDL3"
        }
        KTXSOFTWARE = [pscustomobject]@{
            CacheName = "KTXSOFTWARE"
            RelativePath = "ktxsoftware-src"
            Marker = "CMakeLists.txt"
            Label = "KTX-Software"
        }
        ENET = [pscustomobject]@{
            CacheName = "ENET"
            RelativePath = "enet-src"
            Marker = "CMakeLists.txt"
            Label = "ENet"
        }
        LUA = [pscustomobject]@{
            CacheName = "LUA"
            RelativePath = "lua-src"
            Marker = "lua.h"
            Label = "Lua"
        }
        MINIAUDIO = [pscustomobject]@{
            CacheName = "MINIAUDIO"
            RelativePath = "miniaudio-src"
            Marker = "miniaudio.h"
            Label = "miniaudio"
        }
        STB = [pscustomobject]@{
            CacheName = "STB"
            RelativePath = "stb-src"
            Marker = "stb_vorbis.c"
            Label = "stb"
        }
    }

    $arguments = @()
    $states = @()
    $missingCount = 0

    foreach ($provider in @($Providers)) {
        if (-not $definitions.ContainsKey($provider)) {
            throw "Unknown Henka FetchContent provider: $provider"
        }

        $definition = $definitions[$provider]
        $sourceRoot = if ([string]::IsNullOrWhiteSpace($DependencyRoot)) {
            $definition.RelativePath
        } else {
            Join-Path $DependencyRoot $definition.RelativePath
        }
        $markerPath = Join-Path $sourceRoot $definition.Marker
        $available = (-not $NoLocalProviders) -and
            (Test-Path -LiteralPath $markerPath -PathType Leaf)
        $sourceValue = if ($available) { [System.IO.Path]::GetFullPath($sourceRoot) } else { "" }
        $arguments += "-DFETCHCONTENT_SOURCE_DIR_$($definition.CacheName)=$sourceValue"
        if (-not $available) {
            $missingCount++
        }
        $states += [pscustomobject]@{
            Name = $provider
            Label = $definition.Label
            SourceRoot = $sourceValue
            Available = [bool]$available
            CMakeArgument = [string]$arguments[$arguments.Count - 1]
        }
    }

    $fullyDisconnected = ($missingCount -eq 0)
    $modeArgument = if ($fullyDisconnected) {
        "-DFETCHCONTENT_FULLY_DISCONNECTED=ON"
    } else {
        "-DFETCHCONTENT_FULLY_DISCONNECTED=OFF"
    }
    $arguments += $modeArgument

    return [pscustomobject]@{
        Arguments = [string[]]$arguments
        ProviderStates = [object[]]$states
        MissingCount = $missingCount
        FullyDisconnected = $fullyDisconnected
    }
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

function Get-HenkaSourceIdentity {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RepoRoot
    )

    $repoPath = [System.IO.Path]::GetFullPath($RepoRoot).TrimEnd([char[]]@("\", "/"))
    $git = Get-HenkaGitPath
    $commitLines = @(& $git -C $repoPath rev-parse HEAD 2>$null)
    $commitExitCode = $LASTEXITCODE
    $statusLines = @(& $git -C $repoPath status --porcelain=v1 --untracked-files=all 2>$null)
    $statusExitCode = $LASTEXITCODE
    # Keep Git path enumeration line-oriented here. Native NUL-delimited
    # output is decoded differently by Windows PowerShell and PowerShell 7,
    # which made the same clean tree receive different identities depending
    # on which supported host invoked this shared helper.
    $trackedPaths = @(& $git -C $repoPath ls-files --cached 2>$null)
    $trackedExitCode = $LASTEXITCODE
    $untrackedPaths = @(& $git -C $repoPath ls-files --others --exclude-standard 2>$null)
    $untrackedExitCode = $LASTEXITCODE

    if ($commitExitCode -ne 0 -or
        $statusExitCode -ne 0 -or
        $trackedExitCode -ne 0 -or
        $untrackedExitCode -ne 0 -or
        $commitLines.Count -ne 1 -or
        [string]::IsNullOrWhiteSpace([string]$commitLines[0])) {
        throw "Git source identity query failed for $repoPath."
    }

    $relativePathList = New-Object 'System.Collections.Generic.List[string]'
    $relativePathSet = New-Object 'System.Collections.Generic.HashSet[string]' ([System.StringComparer]::Ordinal)
    foreach ($path in @($trackedPaths + $untrackedPaths)) {
        if ([string]::IsNullOrWhiteSpace([string]$path)) {
            continue
        }
        $normalizedPath = ([string]$path).Replace("\", "/")
        if ($relativePathSet.Add($normalizedPath)) {
            [void]$relativePathList.Add($normalizedPath)
        }
    }
    $relativePathList.Sort([System.StringComparer]::Ordinal)
    $relativePaths = @($relativePathList)
    $manifestBuilder = New-Object System.Text.StringBuilder
    [void]$manifestBuilder.Append("commit`0")
    [void]$manifestBuilder.Append(([string]$commitLines[0]).Trim())
    [void]$manifestBuilder.Append("`0")

    foreach ($relativePath in $relativePaths) {
        $filePath = Join-Path $repoPath ($relativePath.Replace("/", [System.IO.Path]::DirectorySeparatorChar))
        if (-not (Test-Path -LiteralPath $filePath -PathType Leaf)) {
            throw "Source identity input disappeared during hashing: $filePath"
        }
        $fileHash = (Get-FileHash -LiteralPath $filePath -Algorithm SHA256).Hash.ToLowerInvariant()
        [void]$manifestBuilder.Append($relativePath)
        [void]$manifestBuilder.Append("`0")
        [void]$manifestBuilder.Append($fileHash)
        [void]$manifestBuilder.Append("`0")
    }

    $sha256 = [System.Security.Cryptography.SHA256]::Create()
    try {
        $digest = $sha256.ComputeHash(
            [System.Text.Encoding]::UTF8.GetBytes($manifestBuilder.ToString()))
    }
    finally {
        $sha256.Dispose()
    }

    return [pscustomobject]@{
        commit_sha = ([string]$commitLines[0]).Trim()
        source_state = if ($statusLines.Count -eq 0) { "clean" } else { "working-tree" }
        source_identity = (-join ($digest | ForEach-Object { $_.ToString("x2") }))
    }
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

function Stop-HenkaProcessTree {
    param(
        [Parameter(Mandatory = $true)]
        [int]$ProcessId
    )

    if ($ProcessId -le 0) {
        return
    }

    $taskkill = Join-Path $env:SystemRoot "System32\taskkill.exe"
    if (Test-Path -LiteralPath $taskkill -PathType Leaf) {
        $previousErrorActionPreference = $ErrorActionPreference
        try {
            $ErrorActionPreference = "SilentlyContinue"
            & $taskkill /PID $ProcessId /T /F 2>$null | Out-Null
        }
        finally {
            $ErrorActionPreference = $previousErrorActionPreference
        }
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
        [string]$Label,

        [int]$TimeoutMilliseconds = -1
    )

    Write-Host ""
    Write-Host "==> $Label"
    Write-Host "    $FilePath $($Arguments -join ' ')"

    $previousErrorActionPreference = $ErrorActionPreference
    $exitCode = -1

    Push-Location $WorkingDirectory
    try {
        if ($TimeoutMilliseconds -gt 0) {
            $process = $null
            try {
                $process = Start-HenkaProcess `
                    -FilePath $FilePath `
                    -Arguments $Arguments `
                    -WorkingDirectory $WorkingDirectory `
                    -CreateNoWindow
                if (-not $process.WaitForExit($TimeoutMilliseconds)) {
                    Stop-HenkaProcessTree -ProcessId $process.Id
                    throw "$Label exceeded timeout ${TimeoutMilliseconds}ms and its process tree was terminated."
                }
                $exitCode = $process.ExitCode
            }
            finally {
                if ($null -ne $process) {
                    $process.Dispose()
                }
            }
        }
        else {
            $ErrorActionPreference = "Continue"
            & $FilePath @Arguments 2>&1 |
                ForEach-Object {
                    Write-Host ([string]$_)
                }
            $exitCode = $LASTEXITCODE
        }
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
        [string]$Label,

        [int]$TimeoutMilliseconds = -1
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

        if (-not $capturedProcess.WaitForExit($TimeoutMilliseconds)) {
            Stop-HenkaProcessTree -ProcessId $capturedProcess.Process.Id
            throw "$Label exceeded timeout ${TimeoutMilliseconds}ms and its process tree was terminated."
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

function Invoke-HenkaExpectedFailure {
    param(
        [Parameter(Mandatory = $true)]
        [string]$FilePath,

        [string[]]$Arguments = @(),

        [Parameter(Mandatory = $true)]
        [string]$WorkingDirectory,

        [Parameter(Mandatory = $true)]
        [string]$Label,

        [int]$TimeoutMilliseconds = 120000,

        [switch]$ReturnOutput
    )

    if ($TimeoutMilliseconds -le 0) {
        throw "Expected-failure process timeout must be positive."
    }

    $captureRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("henka-expected-failure-" + [Guid]::NewGuid().ToString("N"))
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

        if (-not $capturedProcess.WaitForExit($TimeoutMilliseconds)) {
            Stop-HenkaProcessTree -ProcessId $capturedProcess.Process.Id
            throw "$Label exceeded timeout ${TimeoutMilliseconds}ms and its process tree was terminated."
        }

        $stdout = Read-HenkaSharedText -Path $stdoutPath
        $stderr = Read-HenkaSharedText -Path $stderrPath
        if (-not [string]::IsNullOrWhiteSpace($stdout)) {
            Write-Host $stdout.TrimEnd()
        }
        if (-not [string]::IsNullOrWhiteSpace($stderr)) {
            Write-Host $stderr.TrimEnd()
        }

        $result = [pscustomobject]@{
            ExitCode = [int]$capturedProcess.Process.ExitCode
            Stdout = $stdout
            Stderr = $stderr
        }
        if ($ReturnOutput) {
            return $result
        }
        return $result.ExitCode
    }
    finally {
        Close-HenkaCapturedProcess -CapturedProcess $capturedProcess
        Remove-Item -LiteralPath $captureRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
}
