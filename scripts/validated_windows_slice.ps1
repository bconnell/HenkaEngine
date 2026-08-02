param(
    [Parameter(Mandatory = $true)]
    [string]$CandidateCommitSubject,

    [Parameter(Mandatory = $true)]
    [string]$SliceName,

    [string[]]$SourceAnchor = @(),

    [string[]]$ExpectedChangedPath = @()
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if ($PSVersionTable.PSVersion.Major -ne 5) {
    throw "This workflow requires Windows PowerShell 5.1."
}

. (Join-Path $PSScriptRoot "henka_script_common.ps1")

$repoRoot = Get-HenkaRepoRoot -ScriptDirectory $PSScriptRoot
$runnerPath = Join-Path $PSScriptRoot "invoke_validated_windows_slice.ps1"
$orchestratorPath = $MyInvocation.MyCommand.Path
$workflowManifestPath = Join-Path $PSScriptRoot "validated_windows_slice.sha256"
$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$safeSliceName = [Regex]::Replace($SliceName, '[^A-Za-z0-9._-]', '-')
$evidenceParent = Join-Path ([Environment]::GetFolderPath("UserProfile")) "Downloads"
$evidenceRoot = Join-Path $evidenceParent ("HenkaEngine-{0}-RUNNING-{1}" -f $safeSliceName, $timestamp)
$evidenceDirectory = Join-Path $evidenceRoot "Evidence"
$reportsDirectory = Join-Path $evidenceDirectory "Reports"
$repositoryDirectory = Join-Path $evidenceDirectory "Repository"
$handoffDirectory = Join-Path $evidenceDirectory "Handoff"
$screenshotsDirectory = Join-Path $evidenceDirectory "Screenshots"
$zipPath = $null
$status = "FAILED"
$failureMessage = "Validation did not complete."

function Assert-PowerShellFileParses {
    param([Parameter(Mandatory = $true)][string]$Path)

    $tokens = $null
    $errors = $null
    [void][System.Management.Automation.Language.Parser]::ParseFile(
        $Path,
        [ref]$tokens,
        [ref]$errors)
    if (@($errors).Count -ne 0) {
        $messages = @($errors | ForEach-Object { $_.Message }) -join "; "
        throw "PowerShell parsing failed for ${Path}: $messages"
    }
}

function Assert-WorkflowManifest {
    $expectedPaths = @(
        "scripts/invoke_validated_windows_slice.ps1",
        "scripts/validated_windows_slice.ps1"
    )
    $seen = @{}
    foreach ($line in @([System.IO.File]::ReadAllLines($workflowManifestPath))) {
        if ([string]::IsNullOrWhiteSpace($line)) {
            continue
        }
        if ($line -notmatch '^([0-9a-fA-F]{64})  ([^\r\n]+)$') {
            throw "The validation workflow manifest contains a malformed entry."
        }
        $relativePath = $Matches[2].Replace("\", "/")
        if ($relativePath -notin $expectedPaths -or $seen.ContainsKey($relativePath)) {
            throw "The validation workflow manifest contains an unexpected or duplicate path: $relativePath"
        }
        $absolutePath = Join-Path $repoRoot ($relativePath.Replace("/", "\"))
        $actualHash = (Get-FileHash -LiteralPath $absolutePath -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($actualHash -ne $Matches[1].ToLowerInvariant()) {
            throw "Validation workflow hash mismatch: $relativePath"
        }
        $seen[$relativePath] = $true
    }
    foreach ($expectedPath in $expectedPaths) {
        if (-not $seen.ContainsKey($expectedPath)) {
            throw "The validation workflow manifest omits: $expectedPath"
        }
    }
}

function Assert-SourceAnchors {
    foreach ($specification in $SourceAnchor) {
        $parts = @($specification -split '\|', 3)
        if ($parts.Count -ne 3) {
            throw "A source anchor must use relative-path|literal-text|expected-count form."
        }
        $relativePath = $parts[0].Replace("\", "/")
        $literalText = $parts[1]
        $expectedCount = 0
        if (-not [int]::TryParse($parts[2], [ref]$expectedCount) -or $expectedCount -lt 1) {
            throw "A source anchor expected count must be a positive integer."
        }
        $absolutePath = [System.IO.Path]::GetFullPath((Join-Path $repoRoot ($relativePath.Replace("/", "\"))))
        if (-not $absolutePath.StartsWith($repoRoot + [System.IO.Path]::DirectorySeparatorChar, [System.StringComparison]::OrdinalIgnoreCase)) {
            throw "A source anchor resolves outside the repository: $relativePath"
        }
        $text = [System.IO.File]::ReadAllText($absolutePath)
        $count = ([Regex]::Matches($text, [Regex]::Escape($literalText))).Count
        if ($count -ne $expectedCount) {
            throw "Source anchor count mismatch for ${relativePath}: expected $expectedCount, found $count."
        }
    }
}

function Get-RepositoryChangedPaths {
    $paths = @()
    $paths += @(& git -C $repoRoot diff --name-only)
    $paths += @(& git -C $repoRoot diff --cached --name-only)
    $paths += @(& git -C $repoRoot ls-files --others --exclude-standard)
    $normalizedPaths = @($paths | ForEach-Object {
        ([string]$_).Trim().Replace("\", "/")
    } |
        Where-Object { -not [string]::IsNullOrWhiteSpace($_) } |
        Sort-Object -Unique)
    return $normalizedPaths
}

function Assert-ExpectedRepositoryChanges {
    $expected = @($ExpectedChangedPath | ForEach-Object {
        ([string]$_).Trim().Replace("\", "/")
    } | Where-Object {
        -not [string]::IsNullOrWhiteSpace($_)
    } | Sort-Object -Unique)
    $actual = @(Get-RepositoryChangedPaths)
    if (($expected -join "`n") -ne ($actual -join "`n")) {
        throw "Repository changes do not match the exact expected path set. Expected: $($expected -join ', '); actual: $($actual -join ', ')."
    }
}

function Get-RepositoryContentDigest {
    $paths = @(& git -C $repoRoot ls-files --cached --others --exclude-standard)
    if ($LASTEXITCODE -ne 0) {
        throw "Repository source enumeration failed."
    }

    $lines = New-Object System.Collections.Generic.List[string]
    foreach ($rawPath in @($paths | Sort-Object)) {
        $relativePath = ([string]$rawPath).Trim().Replace("\", "/")
        if ([string]::IsNullOrWhiteSpace($relativePath)) {
            continue
        }
        $absolutePath = Join-Path $repoRoot ($relativePath.Replace("/", "\"))
        $hash = (Get-FileHash -LiteralPath $absolutePath -Algorithm SHA256).Hash.ToLowerInvariant()
        $lines.Add("$hash  $relativePath")
    }

    $joined = ($lines -join "`n")
    $bytes = [System.Text.Encoding]::UTF8.GetBytes($joined)
    $sha256 = [System.Security.Cryptography.SHA256]::Create()
    try {
        $digest = $sha256.ComputeHash($bytes)
    }
    finally {
        $sha256.Dispose()
    }
    return (($digest | ForEach-Object { $_.ToString("x2") }) -join "")
}

function Invoke-LoggedPowerShell {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string]$ScriptPath,
        [string[]]$Arguments = @()
    )

    $logPath = Join-Path $reportsDirectory ($Name + ".log")
    $commandArguments = @(
        "-NoLogo",
        "-NoProfile",
        "-ExecutionPolicy", "Bypass",
        "-File", $ScriptPath
    ) + $Arguments
    $output = @(& powershell.exe @commandArguments 2>&1)
    $exitCode = $LASTEXITCODE
    [System.IO.File]::WriteAllLines($logPath, @($output | ForEach-Object { [string]$_ }))
    $output | ForEach-Object { Write-Output $_ }
    if ($exitCode -ne 0) {
        throw "$Name failed with exit code $exitCode."
    }
}

function Write-RepositoryState {
    param([Parameter(Mandatory = $true)][string]$Prefix)

    @(& git -C $repoRoot status --short --branch) |
        Set-Content -LiteralPath (Join-Path $repositoryDirectory ($Prefix + "-Status.txt"))
    @(& git -C $repoRoot diff --name-only) |
        Set-Content -LiteralPath (Join-Path $repositoryDirectory ($Prefix + "-Unstaged.txt"))
    @(& git -C $repoRoot diff --cached --name-only) |
        Set-Content -LiteralPath (Join-Path $repositoryDirectory ($Prefix + "-Staged.txt"))
    @(& git -C $repoRoot ls-files --others --exclude-standard) |
        Set-Content -LiteralPath (Join-Path $repositoryDirectory ($Prefix + "-Untracked.txt"))
    $head = (& git -C $repoRoot rev-parse HEAD).Trim()
    $origin = (& git -C $repoRoot rev-parse origin/main).Trim()
    $divergence = ((& git -C $repoRoot rev-list --left-right --count HEAD...origin/main) -join " ").Trim()
    @(
        "HEAD=$head",
        "origin/main=$origin",
        "divergence=$divergence"
    ) | Set-Content -LiteralPath (Join-Path $repositoryDirectory ($Prefix + "-Refs.txt"))
}

function Save-ApplicationScreenshot {
    $packageRoot = Join-Path $repoRoot "out\HenkaSandbox3D"
    $executablePath = Join-Path $packageRoot "HenkaSandbox3D.exe"
    $stdoutPath = Join-Path $reportsDirectory "Screenshot-Application-Stdout.log"
    $stderrPath = Join-Path $reportsDirectory "Screenshot-Application-Stderr.log"

    Add-Type -AssemblyName System.Drawing
    Add-Type @'
using System;
using System.Runtime.InteropServices;
using System.Text;
public static class HenkaEvidenceNativeMethods {
    public delegate bool EnumWindowsProc(IntPtr handle, IntPtr parameter);
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left; public int Top; public int Right; public int Bottom; }
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumWindowsProc callback, IntPtr parameter);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr handle, out uint processId);
    [DllImport("user32.dll")] public static extern int GetWindowText(IntPtr handle, StringBuilder text, int capacity);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr handle, uint message, IntPtr wParam, IntPtr lParam);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr handle);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr handle, int command);
    [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr handle);
    [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr handle, IntPtr insertAfter, int x, int y, int width, int height, uint flags);
    [DllImport("dwmapi.dll")] public static extern int DwmGetWindowAttribute(IntPtr handle, int attribute, out RECT rect, int size);
    public static IntPtr FindWindow(uint processId, string title) {
        IntPtr result = IntPtr.Zero;
        EnumWindows(delegate(IntPtr handle, IntPtr parameter) {
            uint owner;
            GetWindowThreadProcessId(handle, out owner);
            if (owner == processId) {
                StringBuilder text = new StringBuilder(256);
                GetWindowText(handle, text, text.Capacity);
                if (text.ToString().Contains(title)) { result = handle; return false; }
            }
            return true;
        }, IntPtr.Zero);
        return result;
    }
    public static void ActivateWindow(IntPtr handle) {
        IntPtr topmost = new IntPtr(-1);
        IntPtr notTopmost = new IntPtr(-2);
        const uint SWP_NOSIZE = 0x0001;
        const uint SWP_NOMOVE = 0x0002;
        const uint SWP_SHOWWINDOW = 0x0040;
        ShowWindow(handle, 9);
        SetWindowPos(handle, topmost, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        SetWindowPos(handle, notTopmost, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        BringWindowToTop(handle);
        SetForegroundWindow(handle);
    }
}
'@

    $captured = Start-HenkaCapturedProcess `
        -FilePath $executablePath `
        -WorkingDirectory $packageRoot `
        -StdoutPath $stdoutPath `
        -StderrPath $stderrPath
    try {
        $handle = [System.IntPtr]::Zero
        for ($index = 0; $index -lt 80 -and $handle -eq [System.IntPtr]::Zero; ++$index) {
            Start-Sleep -Milliseconds 250
            $handle = [HenkaEvidenceNativeMethods]::FindWindow([uint32]$captured.Process.Id, "Henka Engine Sandbox 3D")
        }
        if ($handle -eq [System.IntPtr]::Zero) {
            throw "The sandbox window was not available for application-only capture."
        }
        [HenkaEvidenceNativeMethods]::ActivateWindow($handle)
        $readyDeadline = (Get-Date).AddSeconds(10)
        while ((Get-Date) -lt $readyDeadline) {
            if ((Test-Path -LiteralPath $stdoutPath) -and
                (Select-String -LiteralPath $stdoutPath -Pattern "Sandbox UI ready:" -Quiet)) {
                break
            }
            Start-Sleep -Milliseconds 200
        }
        Start-Sleep -Milliseconds 1500
        $rect = New-Object HenkaEvidenceNativeMethods+RECT
        $dwmResult = [HenkaEvidenceNativeMethods]::DwmGetWindowAttribute(
            $handle,
            9,
            [ref]$rect,
            [System.Runtime.InteropServices.Marshal]::SizeOf($rect))
        if ($dwmResult -ne 0) {
            throw "DWM visible-frame bounds failed with result $dwmResult."
        }
        $width = $rect.Right - $rect.Left
        $height = $rect.Bottom - $rect.Top
        if ($width -le 0 -or $height -le 0) {
            throw "DWM returned invalid application bounds."
        }
        $bitmap = New-Object System.Drawing.Bitmap($width, $height)
        try {
            $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
            try {
                $graphics.CopyFromScreen($rect.Left, $rect.Top, 0, 0, $bitmap.Size)
            }
            finally {
                $graphics.Dispose()
            }
            $screenshotPath = Join-Path $screenshotsDirectory "Sandbox3D-Startup.png"
            $bitmap.Save($screenshotPath, [System.Drawing.Imaging.ImageFormat]::Png)
        }
        finally {
            $bitmap.Dispose()
        }
        $captureTime = (Get-Date).ToString("o")
        $indexText = @(
            "File: Sandbox3D-Startup.png",
            "Purpose: Packaged Sandbox3D startup render evidence",
            "Timestamp: $captureTime",
            "Application state: packaged startup scene and automatic panels",
            "Capture bounds: left=$($rect.Left), top=$($rect.Top), width=$width, height=$height",
            "Capture method: DWM extended frame bounds and application-only screen copy",
            "Status: automated render evidence; manual visual QA remains incomplete"
        )
        $indexText | Set-Content -LiteralPath (Join-Path $screenshotsDirectory "Screenshot-Index.txt")
        $indexText | Set-Content -LiteralPath (Join-Path $evidenceDirectory "Screenshot-Index.txt")
        [HenkaEvidenceNativeMethods]::PostMessage($handle, 0x0010, [System.IntPtr]::Zero, [System.IntPtr]::Zero) | Out-Null
        if (-not $captured.Process.WaitForExit(10000)) {
            throw "The screenshot process did not shut down cleanly."
        }
    }
    finally {
        Close-HenkaCapturedProcess -CapturedProcess $captured
    }
}

function Complete-EvidenceArchive {
    param([Parameter(Mandatory = $true)][string]$ArchiveStatus)

    $summaryPath = Join-Path $handoffDirectory "Summary.txt"
    @(
        "Slice: $SliceName",
        "Candidate commit subject: $CandidateCommitSubject",
        "Status: $ArchiveStatus",
        "Failure: $failureMessage",
        "Manual visual QA: incomplete",
        "Generated: $((Get-Date).ToString('o'))"
    ) | Set-Content -LiteralPath $summaryPath

    $allEvidenceFiles = @(Get-ChildItem -LiteralPath $evidenceRoot -Recurse -File | Sort-Object FullName)
    $manifestLines = @($allEvidenceFiles | ForEach-Object {
        $_.FullName.Substring($evidenceRoot.Length + 1).Replace("\", "/")
    })
    $manifestLines += "MANIFEST.txt"
    $manifestLines += "SHA256SUMS.txt"
    $manifestLines = @($manifestLines | Sort-Object -Unique)
    $manifestLines | Set-Content -LiteralPath (Join-Path $evidenceRoot "MANIFEST.txt")

    $hashFiles = @(Get-ChildItem -LiteralPath $evidenceRoot -Recurse -File |
        Where-Object { $_.Name -ne "SHA256SUMS.txt" } |
        Sort-Object FullName)
    $hashLines = @($hashFiles | ForEach-Object {
        $relativePath = $_.FullName.Substring($evidenceRoot.Length + 1).Replace("\", "/")
        $hash = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        "$hash  $relativePath"
    })
    $hashLines | Set-Content -LiteralPath (Join-Path $evidenceRoot "SHA256SUMS.txt")

    $finalRoot = Join-Path $evidenceParent ("HenkaEngine-{0}-{1}-{2}" -f $safeSliceName, $ArchiveStatus, $timestamp)
    [System.IO.Directory]::Move($evidenceRoot, $finalRoot)
    $zipPath = $finalRoot + ".zip"
    Compress-Archive -Path (Join-Path $finalRoot "*") -DestinationPath $zipPath -CompressionLevel Optimal
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    [System.IO.Compression.ZipFile]::OpenRead($zipPath).Dispose()
    Write-Output "Evidence ZIP: $zipPath"
    Write-Output "Evidence ZIP SHA-256: $((Get-FileHash -LiteralPath $zipPath -Algorithm SHA256).Hash.ToLowerInvariant())"
}

try {
    [System.IO.Directory]::CreateDirectory($reportsDirectory) | Out-Null
    [System.IO.Directory]::CreateDirectory($repositoryDirectory) | Out-Null
    [System.IO.Directory]::CreateDirectory($handoffDirectory) | Out-Null
    [System.IO.Directory]::CreateDirectory($screenshotsDirectory) | Out-Null

    Assert-PowerShellFileParses -Path $runnerPath
    Assert-PowerShellFileParses -Path $orchestratorPath
    Assert-WorkflowManifest
    Assert-SourceAnchors
    Assert-ExpectedRepositoryChanges
    $contentDigestBefore = Get-RepositoryContentDigest
    $contentDigestBefore | Set-Content -LiteralPath (Join-Path $repositoryDirectory "Before-Content-Digest.txt")
    Write-RepositoryState -Prefix "Before"

    Invoke-LoggedPowerShell -Name "01-Public-Repository-Hygiene" -ScriptPath (Join-Path $PSScriptRoot "check_public_repo_hygiene.ps1") -Arguments @("-CandidateCommitSubject", $CandidateCommitSubject)
    Invoke-LoggedPowerShell -Name "02-Repository-Integrity" -ScriptPath (Join-Path $PSScriptRoot "check_repository_integrity.ps1")
    Invoke-LoggedPowerShell -Name "03-Tests" -ScriptPath (Join-Path $PSScriptRoot "test_windows.ps1")
    Invoke-LoggedPowerShell -Name "04-Debug-Build" -ScriptPath (Join-Path $PSScriptRoot "build_windows.ps1") -Arguments @("-Configuration", "Debug")
    Invoke-LoggedPowerShell -Name "05-Package" -ScriptPath (Join-Path $PSScriptRoot "package_sandbox3d_windows.ps1") -Arguments @("-ResetUserData", "-Configuration", "Debug")
    Invoke-LoggedPowerShell -Name "06-Package-Smoke" -ScriptPath (Join-Path $PSScriptRoot "check_packaged_sandbox3d_windows.ps1") -Arguments @("-NonInteractive")
    Invoke-LoggedPowerShell -Name "07-Package-Contract" -ScriptPath (Join-Path $PSScriptRoot "check_packaged_sandbox3d_windows.ps1") -Arguments @("-NonInteractive", "-ContractOnly")
    Invoke-LoggedPowerShell -Name "08-Desktop-Harness" -ScriptPath (Join-Path $PSScriptRoot "check_packaged_sandbox3d_windows.ps1")
    Save-ApplicationScreenshot
    Invoke-LoggedPowerShell -Name "09-External-Template" -ScriptPath (Join-Path $PSScriptRoot "test_external_game_template_windows.ps1")

    $diffCheck = @(& git -C $repoRoot diff --check 2>&1)
    $diffCheck | Set-Content -LiteralPath (Join-Path $reportsDirectory "10-Git-Diff-Check.log")
    if ($LASTEXITCODE -ne 0) {
        throw "git diff --check failed."
    }

    Write-RepositoryState -Prefix "After"
    Assert-ExpectedRepositoryChanges
    $contentDigestAfter = Get-RepositoryContentDigest
    $contentDigestAfter | Set-Content -LiteralPath (Join-Path $repositoryDirectory "After-Content-Digest.txt")
    if ($contentDigestAfter -ne $contentDigestBefore) {
        throw "Repository content digest changed during validation."
    }
    $status = "PASS"
    $failureMessage = "None"
}
catch {
    $failureMessage = $_.Exception.Message
    @(
        "Failure: $failureMessage",
        "Stack: $($_.ScriptStackTrace)"
    ) | Set-Content -LiteralPath (Join-Path $reportsDirectory "Failure.txt")
    try {
        Write-RepositoryState -Prefix "Failure"
    }
    catch {
        "Repository state capture also failed: $($_.Exception.Message)" |
            Set-Content -LiteralPath (Join-Path $reportsDirectory "Repository-State-Failure.txt")
    }
}
finally {
    Complete-EvidenceArchive -ArchiveStatus $status
}

if ($status -ne "PASS") {
    throw $failureMessage
}
