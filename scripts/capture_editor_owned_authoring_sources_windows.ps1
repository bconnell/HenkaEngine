param(
    [string]$ExecutablePath = "build\examples\sandbox3d\Debug\henka_sandbox3d.exe",
    [ValidateSet("Both", "Giraffe", "Rocket")]
    [string]$Subject = "Both"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = (Get-Location).Path
$executable = (Resolve-Path -LiteralPath (Join-Path $repoRoot $ExecutablePath)).Path
. (Join-Path $repoRoot "scripts\henka_script_common.ps1")
. (Join-Path $repoRoot "scripts\henka_ui_automation_helpers.ps1")
Add-Type -AssemblyName System.Windows.Forms
if (-not ("NativeMethods" -as [type])) {
    Add-Type @'
using System;
using System.Runtime.InteropServices;
using System.Text;

public static class NativeMethods {
    public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

    [StructLayout(LayoutKind.Sequential)]
    public struct RECT {
        public int Left;
        public int Top;
        public int Right;
        public int Bottom;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct POINT {
        public int X;
        public int Y;
    }

    [DllImport("user32.dll")]
    public static extern bool EnumWindows(EnumWindowsProc callback, IntPtr lParam);

    [DllImport("user32.dll")]
    public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint processId);

    [DllImport("user32.dll")]
    public static extern int GetWindowText(IntPtr hWnd, StringBuilder text, int maxLength);

    public static IntPtr FindProcessWindow(uint processId, string title) {
        IntPtr result = IntPtr.Zero;
        EnumWindows(delegate(IntPtr hWnd, IntPtr lParam) {
            uint owner;
            GetWindowThreadProcessId(hWnd, out owner);
            if (owner == processId) {
                StringBuilder text = new StringBuilder(256);
                GetWindowText(hWnd, text, text.Capacity);
                if (text.ToString().Contains(title)) {
                    result = hWnd;
                    return false;
                }
            }
            return true;
        }, IntPtr.Zero);
        return result;
    }

    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);

    [DllImport("user32.dll")]
    public static extern bool GetClientRect(IntPtr hWnd, out RECT rect);

    [DllImport("user32.dll")]
    public static extern bool ClientToScreen(IntPtr hWnd, ref POINT point);

}
'@
}

$logDirectory = Join-Path $repoRoot "build\test_tmp\editor-owned-authoring-sources"
$stdoutPath = Join-Path $logDirectory "stdout.log"
$stderrPath = Join-Path $logDirectory "stderr.log"
$runtimeDirectory = Join-Path $logDirectory ("runtime-" + [Guid]::NewGuid().ToString("N"))
$runtimeExecutable = Join-Path $runtimeDirectory "henka_sandbox3d.exe"
$authoringDirectory = Join-Path $repoRoot "assets\authoring"
$automationInputPath = Join-Path $runtimeDirectory "automation-input.events"
New-Item -ItemType Directory -Path $logDirectory -Force | Out-Null
New-Item -ItemType Directory -Path $runtimeDirectory -Force | Out-Null
New-Item -ItemType File -Path $automationInputPath -Force | Out-Null
New-Item -ItemType Directory -Path $authoringDirectory -Force | Out-Null
Copy-Item -LiteralPath $executable -Destination $runtimeExecutable
Copy-Item `
    -LiteralPath (Join-Path (Split-Path -Parent $executable) "assets") `
    -Destination (Join-Path $runtimeDirectory "assets") `
    -Recurse
Remove-Item `
    -LiteralPath (Join-Path $runtimeDirectory "assets\authoring") `
    -Recurse `
    -Force `
    -ErrorAction SilentlyContinue
Remove-Item -LiteralPath @($stdoutPath, $stderrPath) -ErrorAction SilentlyContinue

$automationEncoding = New-Object System.Text.UTF8Encoding($false)
function Send-AutomationEvent {
    param([Parameter(Mandatory = $true)][string]$EventLine)
    $bytes = $script:automationEncoding.GetBytes(
        $EventLine + [Environment]::NewLine)
    $stream = [System.IO.File]::Open(
        $script:automationInputPath,
        [System.IO.FileMode]::OpenOrCreate,
        [System.IO.FileAccess]::Write,
        [System.IO.FileShare]::ReadWrite)
    try {
        $stream.Seek(0, [System.IO.SeekOrigin]::End) | Out-Null
        $stream.Write($bytes, 0, $bytes.Length)
        $stream.Flush($true)
    }
    finally {
        $stream.Dispose()
    }
    Start-Sleep -Milliseconds 120
}

function Send-AutomationKey {
    param([Parameter(Mandatory = $true)][string]$KeyName)
    Send-AutomationEvent -EventLine ("key {0} down" -f $KeyName)
    Send-AutomationEvent -EventLine ("key {0} up" -f $KeyName)
}

function Click-FramebufferPoint {
    param(
        [Parameter(Mandatory = $true)][System.IntPtr]$Handle,
        [Parameter(Mandatory = $true)][int]$FramebufferWidth,
        [Parameter(Mandatory = $true)][int]$FramebufferHeight,
        [Parameter(Mandatory = $true)][double]$FramebufferX,
        [Parameter(Mandatory = $true)][double]$FramebufferY
    )
    if ($FramebufferX -lt 0.0 -or $FramebufferY -lt 0.0 -or
        $FramebufferX -gt [double]$FramebufferWidth -or
        $FramebufferY -gt [double]$FramebufferHeight) {
        throw "Automation framebuffer point is outside the reported application bounds."
    }
    $clientRect = New-Object NativeMethods+RECT
    if (-not [NativeMethods]::GetClientRect($Handle, [ref]$clientRect)) {
        throw "The authoring capture client bounds could not be read."
    }
    $clientWidth = $clientRect.Right - $clientRect.Left
    $clientHeight = $clientRect.Bottom - $clientRect.Top
    $windowPoint = Convert-HenkaFramebufferPointToWindowPoint `
        -FramebufferWidth $FramebufferWidth `
        -FramebufferHeight $FramebufferHeight `
        -WindowWidth $clientWidth `
        -WindowHeight $clientHeight `
        -FramebufferX $FramebufferX `
        -FramebufferY $FramebufferY
    Send-AutomationEvent -EventLine ("move {0} {1}" -f $windowPoint.X, $windowPoint.Y)
    Send-AutomationEvent -EventLine ("button left down {0} {1}" -f $windowPoint.X, $windowPoint.Y)
    Send-AutomationEvent -EventLine ("button left up {0} {1}" -f $windowPoint.X, $windowPoint.Y)
    Start-Sleep -Milliseconds 180
}

function Scroll-FramebufferPoint {
    param(
        [Parameter(Mandatory = $true)][System.IntPtr]$Handle,
        [Parameter(Mandatory = $true)][int]$FramebufferWidth,
        [Parameter(Mandatory = $true)][int]$FramebufferHeight,
        [Parameter(Mandatory = $true)][double]$FramebufferX,
        [Parameter(Mandatory = $true)][double]$FramebufferY,
        [Parameter(Mandatory = $true)][int]$WheelDelta
    )
    if ($WheelDelta -eq 0 -or $FramebufferX -lt 0.0 -or $FramebufferY -lt 0.0 -or
        $FramebufferX -gt [double]$FramebufferWidth -or
        $FramebufferY -gt [double]$FramebufferHeight) {
        throw "Automation scroll target is invalid for the reported application bounds."
    }
    $clientRect = New-Object NativeMethods+RECT
    if (-not [NativeMethods]::GetClientRect($Handle, [ref]$clientRect)) {
        throw "The authoring capture client bounds could not be read for scrolling."
    }
    $clientWidth = $clientRect.Right - $clientRect.Left
    $clientHeight = $clientRect.Bottom - $clientRect.Top
    $windowPoint = Convert-HenkaFramebufferPointToWindowPoint `
        -FramebufferWidth $FramebufferWidth `
        -FramebufferHeight $FramebufferHeight `
        -WindowWidth $clientWidth `
        -WindowHeight $clientHeight `
        -FramebufferX $FramebufferX `
        -FramebufferY $FramebufferY
    Send-AutomationEvent -EventLine ("move {0} {1}" -f $windowPoint.X, $windowPoint.Y)
    Send-AutomationEvent -EventLine ("wheel 0 {0}" -f $WheelDelta)
    Start-Sleep -Milliseconds 220
}

function Get-SourceRow {
    param([Parameter(Mandatory = $true)][string]$Subject)
    $text = Get-Content -LiteralPath $stdoutPath -Raw -ErrorAction SilentlyContinue
    if ([string]::IsNullOrEmpty($text)) {
        return $null
    }
    $pattern = "(?m)^Native authoring source row: name=Showcase $Subject .*? x=(?<x>[-0-9.]+) y=(?<y>[-0-9.]+) width=(?<width>[-0-9.]+) height=28\.0\."
    $matches = [Regex]::Matches($text, $pattern)
    if ($matches.Count -eq 0) {
        return $null
    }
    return $matches[0]
}

function Get-ProjectControls {
    param([Parameter(Mandatory = $true)][string]$Subject)
    $text = Get-Content -LiteralPath $stdoutPath -Raw -ErrorAction SilentlyContinue
    if ([string]::IsNullOrEmpty($text)) {
        return $null
    }
    $pattern = "(?m)^Native authoring project controls: name=Showcase $Subject .*? save_x=(?<saveX>[-0-9.]+) save_y=(?<saveY>[-0-9.]+) reload_x=(?<reloadX>[-0-9.]+) reload_y=(?<reloadY>[-0-9.]+) width=(?<width>[-0-9.]+) height=24\.0\."
    $matches = [Regex]::Matches($text, $pattern)
    if ($matches.Count -eq 0) {
        return $null
    }
    return $matches[$matches.Count - 1]
}

function Get-ExportControl {
    param([Parameter(Mandatory = $true)][string]$Subject)
    $text = Get-Content -LiteralPath $stdoutPath -Raw -ErrorAction SilentlyContinue
    if ([string]::IsNullOrEmpty($text)) {
        return $null
    }
    $pattern = "(?m)^Native authoring export control: name=Showcase $Subject .*? x=(?<x>[-0-9.]+) y=(?<y>[-0-9.]+) width=(?<width>[-0-9.]+) height=24\.0\."
    $matches = [Regex]::Matches($text, $pattern)
    if ($matches.Count -eq 0) {
        return $null
    }
    return $matches[$matches.Count - 1]
}

function Get-SelectionTools {
    param([Parameter(Mandatory = $true)][string]$Subject)
    $text = Get-Content -LiteralPath $stdoutPath -Raw -ErrorAction SilentlyContinue
    if ([string]::IsNullOrEmpty($text)) {
        return $null
    }
    $pattern = "(?m)^Native authoring selection tools: name=Showcase $Subject .*? grow_x=(?<growX>[-0-9.]+) scale_x=(?<scaleX>[-0-9.]+) y=(?<y>[-0-9.]+) width=120\.0 height=24\.0\."
    $matches = [Regex]::Matches($text, $pattern)
    if ($matches.Count -eq 0) {
        return $null
    }
    return $matches[$matches.Count - 1]
}

function Get-FaceModeControl {
    param([Parameter(Mandatory = $true)][string]$Subject)
    $text = Get-Content -LiteralPath $stdoutPath -Raw -ErrorAction SilentlyContinue
    if ([string]::IsNullOrEmpty($text)) {
        return $null
    }
    $pattern = "(?m)^Native authoring face controls: name=Showcase $Subject .*? face_x=(?<x>[-0-9.]+) face_y=(?<y>[-0-9.]+) width=88\.0 height=24\.0\."
    $matches = [Regex]::Matches($text, $pattern)
    if ($matches.Count -eq 0) {
        return $null
    }
    return $matches[$matches.Count - 1]
}

function Get-ConnectedSelectionControl {
    param([Parameter(Mandatory = $true)][string]$Subject)
    $text = Get-Content -LiteralPath $stdoutPath -Raw -ErrorAction SilentlyContinue
    if ([string]::IsNullOrEmpty($text)) {
        return $null
    }
    $pattern = "(?m)^Native authoring connected selection control: name=Showcase $Subject .*? x=(?<x>[-0-9.]+) y=(?<y>[-0-9.]+) width=140\.0 height=24\.0\."
    $matches = [Regex]::Matches($text, $pattern)
    if ($matches.Count -eq 0) {
        return $null
    }
    return $matches[$matches.Count - 1]
}

function Get-BevelControl {
    param([Parameter(Mandatory = $true)][string]$Subject)
    $text = Get-Content -LiteralPath $stdoutPath -Raw -ErrorAction SilentlyContinue
    if ([string]::IsNullOrEmpty($text)) {
        return $null
    }
    $pattern = "(?m)^Native authoring bevel control: name=Showcase $Subject .*? x=(?<x>[-0-9.]+) y=(?<y>[-0-9.]+) width=54\.0 height=24\.0\."
    $matches = [Regex]::Matches($text, $pattern)
    if ($matches.Count -eq 0) {
        return $null
    }
    return $matches[$matches.Count - 1]
}

function Get-FaceEditTools {
    param([Parameter(Mandatory = $true)][string]$Subject)
    $text = Get-Content -LiteralPath $stdoutPath -Raw -ErrorAction SilentlyContinue
    if ([string]::IsNullOrEmpty($text)) {
        return $null
    }
    $pattern = "(?m)^Native authoring face edit tools: name=Showcase $Subject .*? extrude_x=(?<extrudeX>[-0-9.]+) inset_x=(?<insetX>[-0-9.]+) y=(?<y>[-0-9.]+) width=54\.0 height=24\.0\."
    $matches = [Regex]::Matches($text, $pattern)
    if ($matches.Count -eq 0) {
        return $null
    }
    return $matches[$matches.Count - 1]
}

function Get-FaceNormalControls {
    param([Parameter(Mandatory = $true)][string]$Subject)
    $text = Get-Content -LiteralPath $stdoutPath -Raw -ErrorAction SilentlyContinue
    if ([string]::IsNullOrEmpty($text)) {
        return $null
    }
    $pattern = "(?m)^Native authoring face normal controls: name=Showcase $Subject .*? positive_x=(?<positiveX>[-0-9.]+) negative_x=(?<negativeX>[-0-9.]+) y=(?<y>[-0-9.]+) width=54\.0 height=24\.0\."
    $matches = [Regex]::Matches($text, $pattern)
    if ($matches.Count -eq 0) {
        return $null
    }
    return $matches[$matches.Count - 1]
}

function Get-DetailsGeometry {
    return Get-LastLogRegexMatch `
        -Path $stdoutPath `
        -Pattern 'Workspace UI geometry: .*details=(?<x>[-0-9.]+),(?<y>[-0-9.]+),(?<width>[-0-9.]+),(?<height>[-0-9.]+)\.'
}

function Get-ViewportGeometry {
    $match = Get-LastLogRegexMatch `
        -Path $stdoutPath `
        -Pattern 'Sandbox viewport: origin (?<x>[0-9]+),(?<y>[0-9]+) size (?<width>[0-9]+)x(?<height>[0-9]+)\.'
    if ($null -eq $match) {
        return $null
    }
    return $match
}

function Reset-DetailsScroll {
    param(
        [Parameter(Mandatory = $true)][System.IntPtr]$Handle,
        [Parameter(Mandatory = $true)][int]$FramebufferWidth,
        [Parameter(Mandatory = $true)][int]$FramebufferHeight
    )
    $details = Get-DetailsGeometry
    if ($null -eq $details -or [double]$details.Groups["width"].Value -le 0.0) {
        throw "The Object Details geometry was not reported while restoring the authoring controls."
    }
    # Selection changes reset the editor-owned details offset to zero.  Do
    # not synthesize wheel input here: it would make the capture depend on
    # platform wheel direction and could move the newly selected object's
    # controls away from their deterministic starting layout.
    Start-Sleep -Milliseconds 250
}

function Get-AuthoringDisclosure {
    param([Parameter(Mandatory = $true)][string]$Subject)
    $text = Get-Content -LiteralPath $stdoutPath -Raw -ErrorAction SilentlyContinue
    if ([string]::IsNullOrEmpty($text)) {
        return $null
    }
    $pattern = "(?m)^Native authoring disclosure: name=Showcase $Subject .*? x=(?<x>[-0-9.]+) y=(?<y>[-0-9.]+) width=(?<width>[-0-9.]+) height=28\.0 expanded=(?<expanded>[01])\."
    $matches = [Regex]::Matches($text, $pattern)
    if ($matches.Count -eq 0) {
        return $null
    }
    return $matches[$matches.Count - 1]
}

function Ensure-AuthoringExpanded {
    param(
        [Parameter(Mandatory = $true)][string]$Subject,
        [Parameter(Mandatory = $true)][System.IntPtr]$Handle,
        [Parameter(Mandatory = $true)][int]$FramebufferWidth,
        [Parameter(Mandatory = $true)][int]$FramebufferHeight
    )
    # Face picking and topology edits can leave the Object Details body at a
    # stale scroll position while the disclosure telemetry still contains its
    # last visible coordinates.  Restore the top through the same application
    # input path before trusting those coordinates; this keeps the click
    # deterministic across resized and docked layouts.
    for ($restoreAttempt = 0; $restoreAttempt -lt 16; ++$restoreAttempt) {
        $details = Get-DetailsGeometry
        if ($null -eq $details -or [double]$details.Groups["width"].Value -le 0.0) {
            throw "The Object Details geometry was not reported while restoring the authoring disclosure."
        }
        Scroll-FramebufferPoint `
            -Handle $Handle `
            -FramebufferWidth $FramebufferWidth `
            -FramebufferHeight $FramebufferHeight `
            -FramebufferX ([double]$details.Groups["x"].Value + [double]$details.Groups["width"].Value - 18.0) `
            -FramebufferY ([double]$details.Groups["y"].Value + [double]$details.Groups["height"].Value * 0.55) `
            -WheelDelta 4
    }
    $disclosure = Get-AuthoringDisclosure -Subject $Subject
    if ($null -eq $disclosure) {
        throw "The visible $Subject Authoring disclosure was not exposed while restoring the panel state."
    }
    if ($disclosure.Groups["expanded"].Value -eq "0") {
        Click-FramebufferPoint `
            -Handle $Handle `
            -FramebufferWidth $FramebufferWidth `
            -FramebufferHeight $FramebufferHeight `
            -FramebufferX ([double]$disclosure.Groups["x"].Value + [double]$disclosure.Groups["width"].Value * 0.5) `
            -FramebufferY ([double]$disclosure.Groups["y"].Value + 14.0)
        $deadline = (Get-Date).AddSeconds(5)
        do {
            $disclosure = Get-AuthoringDisclosure -Subject $Subject
            if ($null -ne $disclosure -and $disclosure.Groups["expanded"].Value -eq "1") {
                return $disclosure
            }
            Start-Sleep -Milliseconds 100
        } while ((Get-Date) -lt $deadline)
        throw "The visible $Subject Authoring disclosure did not reopen through the application input path."
    }
    return $disclosure
}

function Wait-SourceRow {
    param([Parameter(Mandatory = $true)][string]$Subject)
    $deadline = (Get-Date).AddSeconds(5)
    do {
        $match = Get-SourceRow -Subject $Subject
        if ($null -ne $match) {
            return $match
        }
        Start-Sleep -Milliseconds 100
    } while ((Get-Date) -lt $deadline)
    throw "The visible Scene Objects workflow did not expose a $Subject authoring row."
}

function Click-SceneObjectsNext {
    param(
        [Parameter(Mandatory = $true)][System.IntPtr]$Handle,
        [Parameter(Mandatory = $true)][int]$FramebufferWidth,
        [Parameter(Mandatory = $true)][int]$FramebufferHeight
    )
    $geometry = Get-LastLogRegexMatch -Path $stdoutPath -Pattern 'Workspace UI geometry: .*scene_objects=(?<x>[-0-9.]+),(?<y>[-0-9.]+),(?<width>[-0-9.]+),(?<height>[-0-9.]+)\.'
    if ($null -eq $geometry) {
        throw "The Scene Objects panel geometry was not reported by the running editor."
    }
    Click-FramebufferPoint `
        -Handle $Handle `
        -FramebufferWidth $FramebufferWidth `
        -FramebufferHeight $FramebufferHeight `
        -FramebufferX ([double]$geometry.Groups["x"].Value + [double]$geometry.Groups["width"].Value - 50.0) `
        -FramebufferY ([double]$geometry.Groups["y"].Value + [double]$geometry.Groups["height"].Value - 22.0)
}

$capturedProcess = $null
$process = $null
$handle = [System.IntPtr]::Zero
$previousExportRoot = $env:HENKA_SANDBOX_AUTHORING_EXPORT_DIR
$previousAutomationOwned = $env:HENKA_AUTOMATION_INPUT_OWNED
$previousAutomationFile = $env:HENKA_AUTOMATION_INPUT_FILE
try {
    $env:HENKA_SANDBOX_AUTHORING_EXPORT_DIR = $authoringDirectory
    $env:HENKA_AUTOMATION_INPUT_OWNED = "1"
    $env:HENKA_AUTOMATION_INPUT_FILE = $automationInputPath
    $capturedProcess = Start-HenkaCapturedProcess `
        -FilePath $runtimeExecutable `
        -WorkingDirectory $runtimeDirectory `
        -StdoutPath $stdoutPath `
        -StderrPath $stderrPath `
        -CreateNoWindow
    $process = $capturedProcess.Process
    for ($attempt = 0; $attempt -lt 80 -and $handle -eq [System.IntPtr]::Zero; ++$attempt) {
        Start-Sleep -Milliseconds 250
        $process.Refresh()
        $handle = [NativeMethods]::FindProcessWindow([uint32]$process.Id, "Henka Engine Sandbox 3D")
    }
    if ($handle -eq [System.IntPtr]::Zero) {
        throw "The editor window did not become available."
    }
    if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Sandbox UI ready:" -TimeoutMilliseconds 30000)) {
        throw "The editor did not report UI readiness."
    }
    $framebuffer = Get-LastLogRegexMatch -Path $stdoutPath -Pattern 'Sandbox UI ready:.*framebuffer (?<width>\d+)x(?<height>\d+)'
    if ($null -eq $framebuffer) {
        throw "The editor framebuffer dimensions were not reported."
    }
    $framebufferWidth = [int]$framebuffer.Groups["width"].Value
    $framebufferHeight = [int]$framebuffer.Groups["height"].Value

    Set-HenkaAutomationForeground -Handle $handle
    # Keep the Standard layout for the capture workflow.  Scene Objects and
    # Object Details are the visible authoring controls used by this script;
    # switching to Focus Viewport here hides both panels before the first
    # selection and makes the subsequent row click target a non-interactive
    # stale log coordinate.
    if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Sandbox layout: Standard" -TimeoutMilliseconds 5000)) {
        throw "The visible editor did not remain in the Standard authoring layout."
    }

    $lastEditableControl = $null
    $lastMoveControl = $null
    $lastProjectControls = $null
    $subjects = @("Giraffe", "Rocket")
    if ($Subject -ne "Both") {
        $subjects = @($Subject)
    }
    foreach ($subject in $subjects) {
        $row = Get-SourceRow -Subject $subject
        for ($pageAttempt = 0; $pageAttempt -lt 16 -and $null -eq $row; ++$pageAttempt) {
            Click-SceneObjectsNext -Handle $handle -FramebufferWidth $framebufferWidth -FramebufferHeight $framebufferHeight
            Start-Sleep -Milliseconds 300
            $row = Get-SourceRow -Subject $subject
        }
        if ($null -eq $row) {
            throw "The visible Scene Objects pages did not expose the $subject authoring row."
        }
        Click-FramebufferPoint `
            -Handle $handle `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -FramebufferX ([double]$row.Groups["x"].Value + [double]$row.Groups["width"].Value / 2.0) `
            -FramebufferY ([double]$row.Groups["y"].Value + $(if ($subject -eq "Rocket") { 6.0 } else { 14.0 }))
        if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Native authoring row clicked: name=Showcase $subject" -TimeoutMilliseconds 3000)) {
            throw "The visible $subject row was not selected."
        }

        Reset-DetailsScroll `
            -Handle $handle `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight

        $editable = Get-LastLogRegexMatch -Path $stdoutPath -Pattern "Native authoring Make Editable control: name=Showcase $subject .*? x=(?<x>[-0-9.]+) y=(?<y>[-0-9.]+) width=180\.0 height=24\.0\."
        if ($null -eq $editable) {
            $disclosure = Get-AuthoringDisclosure -Subject $subject
            if ($null -eq $disclosure) {
                throw "The visible $subject Authoring disclosure was not exposed."
            }
            for ($disclosureAttempt = 0; $disclosureAttempt -lt 3 -and $null -eq $editable; ++$disclosureAttempt) {
                foreach ($xOffset in @(30.0, 90.0, 150.0)) {
                    foreach ($yOffset in @(6.0, 14.0, 22.0)) {
                        if ($null -eq $editable) {
                            Click-FramebufferPoint `
                                -Handle $handle `
                                -FramebufferWidth $framebufferWidth `
                                -FramebufferHeight $framebufferHeight `
                                -FramebufferX ([double]$disclosure.Groups["x"].Value + $xOffset) `
                                -FramebufferY ([double]$disclosure.Groups["y"].Value + $yOffset)
                            $editable = Get-LastLogRegexMatch -Path $stdoutPath -Pattern "Native authoring Make Editable control: name=Showcase $subject .*? x=(?<x>[-0-9.]+) y=(?<y>[-0-9.]+) width=180\.0 height=24\.0\."
                        }
                    }
                }
            }
        }
        if ($null -eq $editable) {
            throw "The visible $subject Make Editable control was not exposed."
        }
        $editableClicked = $false
        for ($editableAttempt = 0; $editableAttempt -lt 3 -and -not $editableClicked; ++$editableAttempt) {
            foreach ($xOffset in @(30.0, 90.0, 150.0)) {
                foreach ($yOffset in @(6.0, 12.0, 18.0)) {
                    if (-not $editableClicked) {
                        Click-FramebufferPoint `
                            -Handle $handle `
                            -FramebufferWidth $framebufferWidth `
                            -FramebufferHeight $framebufferHeight `
                            -FramebufferX ([double]$editable.Groups["x"].Value + $xOffset) `
                            -FramebufferY ([double]$editable.Groups["y"].Value + $yOffset)
                        $editableClicked = Wait-FileContains `
                            -Path $stdoutPath `
                            -Pattern "Native authoring workflow: Make Editable converted .*Showcase $subject" `
                            -TimeoutMilliseconds 500
                    }
                }
            }
        }
        if (-not $editableClicked) {
            throw "The visible $subject Make Editable operation did not complete."
        }
        $move = Get-LastLogRegexMatch -Path $stdoutPath -Pattern "Native authoring move control: name=Showcase $subject .*? x=(?<x>[-0-9.]+) y=(?<y>[-0-9.]+) width=88\.0 height=24\.0\."
        if ($null -eq $move) {
            throw "The visible $subject component Move control was not exposed."
        }
        Click-FramebufferPoint -Handle $handle -FramebufferWidth $framebufferWidth -FramebufferHeight $framebufferHeight -FramebufferX ([double]$move.Groups["x"].Value + 44.0) -FramebufferY ([double]$move.Groups["y"].Value + 12.0)
        if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Native authoring workflow: component move edited Showcase $subject .* source_state=HENKA_NATIVE_EDITED_FIXTURE design_authority=EDITOR_DERIVED_FIXTURE" -TimeoutMilliseconds 10000)) {
            throw "The visible $subject generic modeling edit did not report the expected editor-derived fixture state."
        }

        $selectionTools = Get-SelectionTools -Subject $subject
        if ($null -eq $selectionTools) {
            throw "The visible $subject Grow Selection and Scale Selected controls were not exposed."
        }
        Click-FramebufferPoint -Handle $handle -FramebufferWidth $framebufferWidth -FramebufferHeight $framebufferHeight -FramebufferX ([double]$selectionTools.Groups["growX"].Value + 60.0) -FramebufferY ([double]$selectionTools.Groups["y"].Value + 12.0)
        if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Native authoring selection grown: name=Showcase $subject .*result=success" -TimeoutMilliseconds 10000)) {
            throw "The visible $subject Grow Selection operation did not complete."
        }
        $selectionTools = Get-SelectionTools -Subject $subject
        Click-FramebufferPoint `
            -Handle $handle `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -FramebufferX ([double]$selectionTools.Groups["scaleX"].Value + 60.0) `
            -FramebufferY ([double]$selectionTools.Groups["y"].Value + 12.0)
        if (-not (Wait-FileContains `
                -Path $stdoutPath `
                -Pattern "Native authoring workflow: selected topology scaled Showcase $subject" `
                -TimeoutMilliseconds 10000)) {
            throw "The visible $subject Scale Selected operation did not complete."
        }

        $connectedSelection = Get-ConnectedSelectionControl -Subject $subject
        if ($null -eq $connectedSelection) {
            throw "The visible $subject Select Connected control was not exposed."
        }
        Click-FramebufferPoint `
            -Handle $handle `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -FramebufferX ([double]$connectedSelection.Groups["x"].Value + 70.0) `
            -FramebufferY ([double]$connectedSelection.Groups["y"].Value + 12.0)
        if (-not (Wait-FileContains `
                -Path $stdoutPath `
                -Pattern "Native authoring connected selection: name=Showcase $subject .*result=(success|limit)" `
                -TimeoutMilliseconds 10000)) {
            throw "The visible $subject Select Connected operation did not complete."
        }
        $connectedLog = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern "Native authoring connected selection: name=Showcase $subject .*result=(?<result>success|limit) mode=(?<mode>Vertex|Edge|Face)"
        if ($null -eq $connectedLog) {
            throw "The visible $subject Select Connected result was not reported."
        }
        # The connected Face selection is already a real user-facing topology
        # selection. Exercise Bevel before viewport picking can reflow the
        # details panel; viewport picking is validated separately below.
        $bevelCompleted = $false
        $bevel = Get-BevelControl -Subject $subject
        if ($null -ne $bevel) {
            for ($bevelAttempt = 0; $bevelAttempt -lt 3 -and -not $bevelCompleted; ++$bevelAttempt) {
                foreach ($xOffset in @(20.0, 41.0, 62.0)) {
                    foreach ($yOffset in @(6.0, 12.0, 18.0)) {
                        if (-not $bevelCompleted) {
                            Click-FramebufferPoint `
                                -Handle $handle `
                                -FramebufferWidth $framebufferWidth `
                                -FramebufferHeight $framebufferHeight `
                                -FramebufferX ([double]$bevel.Groups["x"].Value + $xOffset) `
                                -FramebufferY ([double]$bevel.Groups["y"].Value + $yOffset)
                            $bevelCompleted = Wait-FileContains `
                                -Path $stdoutPath `
                                -Pattern "Native authoring workflow: face bevel edited Showcase $subject" `
                                -TimeoutMilliseconds 500
                        }
                    }
                }
            }
        }
        # A bounded connected selection is evidence of the topology workflow;
        # do not automatically mutate it here.  A component that reaches the
        # editor selection cap may be a partial surface region, and applying a
        # transform to that partial region would manufacture a visible seam in
        # the showcase source.  The user-facing Scale Selected control remains
        # available for an intentional, inspected edit.

        # Exercise the generic Face -> viewport pick -> Bevel path through the
        # visible editor.  This deliberately avoids the showcase-only profile
        # refinement control: the saved source must prove a user-facing
        # topology operation on the subject itself.
        $faceControl = Get-FaceModeControl -Subject $subject
        if ($null -eq $faceControl) {
            throw "The visible $subject Face selection control was not exposed."
        }
        if ($connectedLog.Groups["mode"].Value -ne "Face") {
            Click-FramebufferPoint `
                -Handle $handle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ([double]$faceControl.Groups["x"].Value + 44.0) `
                -FramebufferY ([double]$faceControl.Groups["y"].Value + 12.0)
            if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Native authoring topology mode:.*mode=Face" -TimeoutMilliseconds 5000)) {
                throw "The visible $subject Face selection mode did not become active."
            }
        }
        $extrudeCompleted = $false
        $faceEditTools = Get-FaceEditTools -Subject $subject
        if ($null -ne $faceEditTools) {
            for ($extrudeAttempt = 0; $extrudeAttempt -lt 3 -and -not $extrudeCompleted; ++$extrudeAttempt) {
                foreach ($xOffset in @(20.0, 41.0, 62.0)) {
                    foreach ($yOffset in @(6.0, 12.0, 18.0)) {
                        if (-not $extrudeCompleted) {
                            Click-FramebufferPoint `
                                -Handle $handle `
                                -FramebufferWidth $framebufferWidth `
                                -FramebufferHeight $framebufferHeight `
                                -FramebufferX ([double]$faceEditTools.Groups["extrudeX"].Value + $xOffset) `
                                -FramebufferY ([double]$faceEditTools.Groups["y"].Value + $yOffset)
                            $extrudeCompleted = Wait-FileContains `
                                -Path $stdoutPath `
                                -Pattern "Native authoring workflow: face extrude edited Showcase $subject" `
                                -TimeoutMilliseconds 500
                        }
                    }
                }
            }
        }
        if (-not $extrudeCompleted) {
            throw "The visible $subject Face Extrude control was not actionable before viewport picking."
        }
        $viewport = Get-ViewportGeometry
        if ($null -eq $viewport) {
            throw "The visible $subject viewport geometry was not reported."
        }
        $viewportX = [double]$viewport.Groups["x"].Value
        $viewportY = [double]$viewport.Groups["y"].Value
        $viewportWidth = [double]$viewport.Groups["width"].Value
        $viewportHeight = [double]$viewport.Groups["height"].Value
        $picked = $false
        foreach ($xFraction in @(0.10, 0.20, 0.30, 0.40, 0.50, 0.60, 0.70, 0.80, 0.90)) {
            foreach ($yFraction in @(0.15, 0.25, 0.35, 0.45, 0.55, 0.65, 0.75, 0.85)) {
                if (-not $picked) {
                    Click-FramebufferPoint `
                        -Handle $handle `
                        -FramebufferWidth $framebufferWidth `
                        -FramebufferHeight $framebufferHeight `
                        -FramebufferX ($viewportX + $viewportWidth * $xFraction) `
                        -FramebufferY ($viewportY + $viewportHeight * $yFraction)
                    $picked = Wait-FileContains `
                        -Path $stdoutPath `
                        -Pattern "Native authoring component picked: name=Showcase $subject .* mode=Face" `
                        -TimeoutMilliseconds 700
                }
            }
        }
        if (-not $picked) {
            throw "The visible $subject Face viewport picker did not select a face."
        }
        Ensure-AuthoringExpanded `
            -Subject $subject `
            -Handle $handle `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight | Out-Null
        $bevel = Get-BevelControl -Subject $subject
        for ($bevelScrollAttempt = 0; $bevelScrollAttempt -lt 8 -and $null -eq $bevel -and -not $bevelCompleted; ++$bevelScrollAttempt) {
            $details = Get-DetailsGeometry
            if ($null -eq $details -or [double]$details.Groups["width"].Value -le 0.0) {
                throw "The visible $subject Object Details geometry was not reported while exposing Bevel."
            }
            Scroll-FramebufferPoint `
                -Handle $handle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ([double]$details.Groups["x"].Value + [double]$details.Groups["width"].Value - 18.0) `
                -FramebufferY ([double]$details.Groups["y"].Value + [double]$details.Groups["height"].Value * 0.55) `
                -WheelDelta -4
            Start-Sleep -Milliseconds 150
            $bevel = Get-BevelControl -Subject $subject
        }
        if ($null -eq $bevel -and -not $bevelCompleted) {
            throw "The visible $subject Bevel control was not exposed after Face selection."
        }
        $bevelClicked = $bevelCompleted
        if (-not $bevelCompleted) {
            for ($bevelAttempt = 0; $bevelAttempt -lt 3 -and -not $bevelClicked; ++$bevelAttempt) {
                foreach ($xOffset in @(20.0, 41.0, 62.0)) {
                    foreach ($yOffset in @(6.0, 12.0, 18.0)) {
                        if (-not $bevelClicked) {
                            Click-FramebufferPoint `
                                -Handle $handle `
                                -FramebufferWidth $framebufferWidth `
                                -FramebufferHeight $framebufferHeight `
                                -FramebufferX ([double]$bevel.Groups["x"].Value + $xOffset) `
                                -FramebufferY ([double]$bevel.Groups["y"].Value + $yOffset)
                            $bevelClicked = Wait-FileContains `
                                -Path $stdoutPath `
                                -Pattern "Native authoring workflow: face bevel edited Showcase $subject" `
                                -TimeoutMilliseconds 500
                        }
                    }
                }
            }
        }
        if (-not $bevelClicked -and -not $bevelCompleted) {
            throw "The visible $subject Face Bevel operation did not complete."
        }


        $controls = Get-ProjectControls -Subject $subject
        for ($projectScrollAttempt = 0; $projectScrollAttempt -lt 8 -and $null -eq $controls; ++$projectScrollAttempt) {
            $details = Get-DetailsGeometry
            if ($null -eq $details -or [double]$details.Groups["width"].Value -le 0.0) {
                throw "The visible $subject Object Details geometry was not reported while exposing Save Project."
            }
            Scroll-FramebufferPoint `
                -Handle $handle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ([double]$details.Groups["x"].Value + [double]$details.Groups["width"].Value - 18.0) `
                -FramebufferY ([double]$details.Groups["y"].Value + [double]$details.Groups["height"].Value * 0.55) `
                -WheelDelta -4
            Start-Sleep -Milliseconds 150
            $controls = Get-ProjectControls -Subject $subject
        }
        if ($null -eq $controls) {
            throw "The visible $subject Save Project control was not exposed."
        }
        Click-FramebufferPoint -Handle $handle -FramebufferWidth $framebufferWidth -FramebufferHeight $framebufferHeight -FramebufferX ([double]$controls.Groups["saveX"].Value + [double]$controls.Groups["width"].Value * 0.5) -FramebufferY ([double]$controls.Groups["saveY"].Value + 12.0)
        if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Native authoring workflow: project saved for Showcase $subject .* source_state=HENKA_NATIVE_EDITED_FIXTURE" -TimeoutMilliseconds 5000)) {
            throw "The visible $subject Save Project operation did not persist the expected editor-derived fixture state."
        }
        $controls = Get-ProjectControls -Subject $subject
        if ($null -eq $controls) {
            throw "The visible $subject Reload Project control was not exposed."
        }
        Click-FramebufferPoint -Handle $handle -FramebufferWidth $framebufferWidth -FramebufferHeight $framebufferHeight -FramebufferX ([double]$controls.Groups["reloadX"].Value + [double]$controls.Groups["width"].Value * 0.5) -FramebufferY ([double]$controls.Groups["reloadY"].Value + 12.0)
        if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Native authoring workflow: project reloaded for Showcase $subject .* source_state=HENKA_NATIVE_EDITED_FIXTURE" -TimeoutMilliseconds 5000)) {
            throw "The visible $subject Reload Project operation did not restore the expected editor-derived fixture state."
        }

        $export = Get-ExportControl -Subject $subject
        for ($exportScrollAttempt = 0; $exportScrollAttempt -lt 8 -and $null -eq $export; ++$exportScrollAttempt) {
            $details = Get-DetailsGeometry
            if ($null -eq $details -or [double]$details.Groups["width"].Value -le 0.0) {
                throw "The visible $subject Object Details geometry was not reported while exposing Export Source."
            }
            Scroll-FramebufferPoint `
                -Handle $handle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ([double]$details.Groups["x"].Value + [double]$details.Groups["width"].Value - 18.0) `
                -FramebufferY ([double]$details.Groups["y"].Value + [double]$details.Groups["height"].Value * 0.55) `
                -WheelDelta -4
            Start-Sleep -Milliseconds 150
            $export = Get-ExportControl -Subject $subject
        }
        if ($null -eq $export) {
            throw "The visible $subject Export Source control was not exposed."
        }
        Click-FramebufferPoint `
            -Handle $handle `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -FramebufferX ([double]$export.Groups["x"].Value + [double]$export.Groups["width"].Value * 0.5) `
            -FramebufferY ([double]$export.Groups["y"].Value + 12.0)
        if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Native authoring source export: name=Showcase $subject .* result=success source_state=HENKA_NATIVE_EDITED_FIXTURE" -TimeoutMilliseconds 10000)) {
            throw "The visible $subject Export Source operation did not complete."
        }
        $destination = Join-Path $authoringDirectory ("showcase_{0}.hams" -f $subject.ToLowerInvariant())
        if (-not (Test-Path -LiteralPath $destination -PathType Leaf)) {
            throw "The visible $subject Export Source operation did not produce the expected HAMS artifact."
        }
        $hash = (Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash
        Write-Output "[pass] $subject editor-owned source saved, reloaded, and exported from the visible workflow: $hash"
    }
}
finally {
    if ($null -eq $previousExportRoot) {
        Remove-Item Env:HENKA_SANDBOX_AUTHORING_EXPORT_DIR -ErrorAction SilentlyContinue
    }
    else {
        $env:HENKA_SANDBOX_AUTHORING_EXPORT_DIR = $previousExportRoot
    }
    if ($null -eq $previousAutomationOwned) {
        Remove-Item Env:HENKA_AUTOMATION_INPUT_OWNED -ErrorAction SilentlyContinue
    }
    else {
        $env:HENKA_AUTOMATION_INPUT_OWNED = $previousAutomationOwned
    }
    if ($null -eq $previousAutomationFile) {
        Remove-Item Env:HENKA_AUTOMATION_INPUT_FILE -ErrorAction SilentlyContinue
    }
    else {
        $env:HENKA_AUTOMATION_INPUT_FILE = $previousAutomationFile
    }
    if ($null -ne $capturedProcess) {
        Close-HenkaCapturedProcess -CapturedProcess $capturedProcess
    }
}
