param(
    [string]$ExecutablePath = "build\examples\sandbox3d\Debug\henka_sandbox3d.exe",
    [ValidateSet("Both", "Giraffe", "Rocket")]
    [string]$Subject = "Both"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = (Get-Location).Path
$executable = (Resolve-Path -LiteralPath (Join-Path $repoRoot $ExecutablePath)).Path
$helperText = Get-Content -LiteralPath (Join-Path $repoRoot "scripts\check_packaged_sandbox3d_windows.ps1") -Raw
$helperStart = $helperText.IndexOf("Set-StrictMode -Version Latest", [System.StringComparison]::Ordinal)
$helperEnd = $helperText.IndexOf('$repoRoot =', $helperStart, [System.StringComparison]::Ordinal)
if ($helperStart -lt 0 -or $helperEnd -le $helperStart) {
    throw "The shared packaged UI automation helpers could not be loaded."
}
$ContractOnly = $false
$NonInteractive = $false
$scriptDirectoryLiteral = "'" + ((Join-Path $repoRoot "scripts").Replace("'", "''")) + "'"
$helperLibrary = $helperText.Substring($helperStart, $helperEnd - $helperStart).Replace('$PSScriptRoot', $scriptDirectoryLiteral)
Invoke-Expression $helperLibrary
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

    public const uint MOUSEEVENTF_LEFTDOWN = 0x0002;
    public const uint MOUSEEVENTF_LEFTUP = 0x0004;
    public const uint MOUSEEVENTF_WHEEL = 0x0800;

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

    [DllImport("user32.dll")]
    public static extern bool SetCursorPos(int x, int y);

    [DllImport("user32.dll")]
    public static extern void mouse_event(uint dwFlags, uint dx, uint dy, uint dwData, UIntPtr dwExtraInfo);
}
'@
}

$logDirectory = Join-Path $repoRoot "build\test_tmp\editor-owned-authoring-sources"
$stdoutPath = Join-Path $logDirectory "stdout.log"
$stderrPath = Join-Path $logDirectory "stderr.log"
$runtimeDirectory = Join-Path $logDirectory ("runtime-" + [Guid]::NewGuid().ToString("N"))
$runtimeExecutable = Join-Path $runtimeDirectory "henka_sandbox3d.exe"
$authoringDirectory = Join-Path $repoRoot "assets\authoring"
New-Item -ItemType Directory -Path $logDirectory -Force | Out-Null
New-Item -ItemType Directory -Path $runtimeDirectory -Force | Out-Null
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
    $pattern = "(?m)^Native authoring project controls: name=Showcase $Subject .*? save_x=(?<saveX>[-0-9.]+) save_y=(?<saveY>[-0-9.]+) reload_x=(?<reloadX>[-0-9.]+) reload_y=(?<reloadY>[-0-9.]+) width=140\.0 height=24\.0\."
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
    $pattern = "(?m)^Native authoring bevel control: name=Showcase $Subject .*? x=(?<x>[-0-9.]+) y=(?<y>[-0-9.]+) width=82\.0 height=24\.0\."
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
    $pattern = "(?m)^Native authoring face edit tools: name=Showcase $Subject .*? extrude_x=(?<extrudeX>[-0-9.]+) inset_x=(?<insetX>[-0-9.]+) y=(?<y>[-0-9.]+) width=82\.0 height=24\.0\."
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
    for ($attempt = 0; $attempt -lt 12; ++$attempt) {
        Scroll-FramebufferPoint `
            -Handle $Handle `
            -FramebufferWidth $FramebufferWidth `
            -FramebufferHeight $FramebufferHeight `
            -FramebufferX ([double]$details.Groups["x"].Value + [double]$details.Groups["width"].Value - 18.0) `
            -FramebufferY ([double]$details.Groups["y"].Value + 42.0) `
            -WheelDelta 120
    }
    Start-Sleep -Milliseconds 250
}

function Get-AuthoringDisclosure {
    param([Parameter(Mandatory = $true)][string]$Subject)
    $text = Get-Content -LiteralPath $stdoutPath -Raw -ErrorAction SilentlyContinue
    if ([string]::IsNullOrEmpty($text)) {
        return $null
    }
    $pattern = "(?m)^Native authoring disclosure: name=Showcase $Subject .*? x=(?<x>[-0-9.]+) y=(?<y>[-0-9.]+) width=(?<width>[-0-9.]+) height=28\.0\."
    $matches = [Regex]::Matches($text, $pattern)
    if ($matches.Count -eq 0) {
        return $null
    }
    return $matches[$matches.Count - 1]
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
try {
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
    [System.Windows.Forms.SendKeys]::SendWait('{F5}')
    if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Sandbox layout: Inspect" -TimeoutMilliseconds 5000)) {
        throw "The visible editor did not enter Inspect layout."
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
            Click-FramebufferPoint `
                -Handle $handle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ([double]$disclosure.Groups["x"].Value + 40.0) `
                -FramebufferY ([double]$disclosure.Groups["y"].Value + 14.0)
            $editable = Get-LastLogRegexMatch -Path $stdoutPath -Pattern "Native authoring Make Editable control: name=Showcase $subject .*? x=(?<x>[-0-9.]+) y=(?<y>[-0-9.]+) width=180\.0 height=24\.0\."
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
                            -Pattern "Native authoring dogfood: Make Editable converted .*Showcase $subject" `
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
        if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Native authoring dogfood: component move edited Showcase $subject .* source_state=HENKA_NATIVE_EDITED_FIXTURE design_authority=EDITOR_DERIVED_FIXTURE" -TimeoutMilliseconds 10000)) {
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
                -Pattern "Native authoring dogfood: selected topology scaled Showcase $subject" `
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
            -Pattern "Native authoring connected selection: name=Showcase $subject .*result=(?<result>success|limit)"
        if ($null -eq $connectedLog) {
            throw "The visible $subject Select Connected result was not reported."
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
        Click-FramebufferPoint `
            -Handle $handle `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -FramebufferX ([double]$faceControl.Groups["x"].Value + 44.0) `
            -FramebufferY ([double]$faceControl.Groups["y"].Value + 12.0)
        if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Native authoring topology mode:.*mode=Face" -TimeoutMilliseconds 5000)) {
            throw "The visible $subject Face selection mode did not become active."
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
        foreach ($xFraction in @(0.33, 0.50, 0.67)) {
            foreach ($yFraction in @(0.40, 0.55, 0.70)) {
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
        $bevel = Get-BevelControl -Subject $subject
        for ($bevelScrollAttempt = 0; $bevelScrollAttempt -lt 8 -and $null -eq $bevel; ++$bevelScrollAttempt) {
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
                -WheelDelta -120
            Start-Sleep -Milliseconds 150
            $bevel = Get-BevelControl -Subject $subject
        }
        if ($null -eq $bevel) {
            throw "The visible $subject Bevel control was not exposed after Face selection."
        }
        Click-FramebufferPoint `
            -Handle $handle `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -FramebufferX ([double]$bevel.Groups["x"].Value + 41.0) `
            -FramebufferY ([double]$bevel.Groups["y"].Value + 12.0)
        if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Native authoring dogfood: face bevel edited Showcase $subject" -TimeoutMilliseconds 10000)) {
            throw "The visible $subject Face Bevel operation did not complete."
        }

        $faceEditTools = Get-FaceEditTools -Subject $subject
        for ($faceEditScrollAttempt = 0; $faceEditScrollAttempt -lt 8 -and $null -eq $faceEditTools; ++$faceEditScrollAttempt) {
            $details = Get-DetailsGeometry
            if ($null -eq $details -or [double]$details.Groups["width"].Value -le 0.0) {
                throw "The visible $subject Object Details geometry was not reported while exposing Face tools."
            }
            Scroll-FramebufferPoint `
                -Handle $handle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ([double]$details.Groups["x"].Value + [double]$details.Groups["width"].Value - 18.0) `
                -FramebufferY ([double]$details.Groups["y"].Value + [double]$details.Groups["height"].Value * 0.55) `
                -WheelDelta -120
            Start-Sleep -Milliseconds 150
            $faceEditTools = Get-FaceEditTools -Subject $subject
        }
        if ($null -eq $faceEditTools) {
            throw "The visible $subject generic Face edit controls were not exposed."
        }
        Click-FramebufferPoint `
            -Handle $handle `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -FramebufferX ([double]$faceEditTools.Groups["extrudeX"].Value + 41.0) `
            -FramebufferY ([double]$faceEditTools.Groups["y"].Value + 12.0)
        if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Native authoring dogfood: face extrude edited Showcase $subject" -TimeoutMilliseconds 10000)) {
            throw "The visible $subject Face Extrude operation did not complete."
        }

        $controls = Get-ProjectControls -Subject $subject
        if ($null -eq $controls) {
            throw "The visible $subject Save Project control was not exposed."
        }
        Click-FramebufferPoint -Handle $handle -FramebufferWidth $framebufferWidth -FramebufferHeight $framebufferHeight -FramebufferX ([double]$controls.Groups["saveX"].Value + 70.0) -FramebufferY ([double]$controls.Groups["saveY"].Value + 12.0)
        if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Native authoring dogfood: project saved for Showcase $subject .* source_state=HENKA_NATIVE_EDITED_FIXTURE" -TimeoutMilliseconds 5000)) {
            throw "The visible $subject Save Project operation did not persist the expected editor-derived fixture state."
        }
        $controls = Get-ProjectControls -Subject $subject
        if ($null -eq $controls) {
            throw "The visible $subject Reload Project control was not exposed."
        }
        Click-FramebufferPoint -Handle $handle -FramebufferWidth $framebufferWidth -FramebufferHeight $framebufferHeight -FramebufferX ([double]$controls.Groups["reloadX"].Value + 70.0) -FramebufferY ([double]$controls.Groups["reloadY"].Value + 12.0)
        if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Native authoring dogfood: project reloaded for Showcase $subject .* source_state=HENKA_NATIVE_EDITED_FIXTURE" -TimeoutMilliseconds 5000)) {
            throw "The visible $subject Reload Project operation did not restore the expected editor-derived fixture state."
        }

        $source = Get-ChildItem -LiteralPath (Join-Path $runtimeDirectory "user\saves") -Filter '*.hams' -File -ErrorAction SilentlyContinue | Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
        if ($null -eq $source) {
            throw "The visible $subject Save Project operation did not produce an authoring source."
        }
        $destination = Join-Path $authoringDirectory ("showcase_{0}.hams" -f $subject.ToLowerInvariant())
        Copy-Item -LiteralPath $source.FullName -Destination $destination -Force
        $hash = (Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash
        Write-Output "[pass] $subject editor-owned source saved, reloaded, and copied from the visible workflow: $hash"
    }
}
finally {
    if ($null -ne $capturedProcess) {
        Close-HenkaCapturedProcess -CapturedProcess $capturedProcess
    }
}
