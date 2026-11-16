Set-StrictMode -Version Latest

function Get-HenkaTypographyCaptureScalePairViolation {
    param([Parameter(Mandatory = $true)][object[]]$Captures)

    if ($Captures.Count -eq 0) {
        return 'No typography captures were supplied.'
    }

    $groups = @{}
    foreach ($capture in $Captures) {
        if ($null -eq $capture -or
            $null -eq $capture.PSObject.Properties['font'] -or
            $null -eq $capture.PSObject.Properties['ui_scale_percent'] -or
            $null -eq $capture.PSObject.Properties['client_width'] -or
            $null -eq $capture.PSObject.Properties['client_height'] -or
            $null -eq $capture.PSObject.Properties['os_display_scale_numerator'] -or
            $null -eq $capture.PSObject.Properties['os_display_scale_denominator']) {
            return 'A typography capture is missing its product-reported OS display scale or comparison identity.'
        }

        try {
            $font = [string]$capture.font
            $scale = [int]$capture.ui_scale_percent
            $width = [int]$capture.client_width
            $height = [int]$capture.client_height
            $numerator = [uint32]$capture.os_display_scale_numerator
            $denominator = [uint32]$capture.os_display_scale_denominator
        }
        catch {
            return 'A typography capture contains a malformed scale or comparison identity.'
        }

        if ($font -notin @('atkinson', 'noto') -or
            $scale -notin @(100, 125, 150, 200) -or
            $width -le 0 -or $height -le 0 -or
            $numerator -eq 0 -or $denominator -eq 0) {
            return 'A typography capture contains an unsupported font, app scale, resolution, or OS display scale.'
        }

        $key = '{0}|{1}|{2}|{3}' -f $scale, $width, $height, $font
        if ($groups.ContainsKey($key)) {
            return "A duplicate typography capture exists for $key."
        }
        $groups[$key] = $capture
    }

    foreach ($scale in @(100, 125, 150, 200)) {
        $resolutions = @($Captures |
            Where-Object { $_.ui_scale_percent -eq $scale } |
            ForEach-Object { '{0}x{1}' -f $_.client_width, $_.client_height } |
            Sort-Object -Unique)
        foreach ($resolution in $resolutions) {
            $dimensions = $resolution -split 'x'
            $width = [int]$dimensions[0]
            $height = [int]$dimensions[1]
            $atkinsonKey = '{0}|{1}|{2}|atkinson' -f $scale, $width, $height
            $notoKey = '{0}|{1}|{2}|noto' -f $scale, $width, $height
            if (-not $groups.ContainsKey($atkinsonKey) -or -not $groups.ContainsKey($notoKey)) {
                return "Both fonts must have a matched capture at $scale percent, ${width}x${height}."
            }

            $atkinson = $groups[$atkinsonKey]
            $noto = $groups[$notoKey]
            if ([decimal]$atkinson.os_display_scale_numerator * [decimal]$noto.os_display_scale_denominator -ne
                [decimal]$noto.os_display_scale_numerator * [decimal]$atkinson.os_display_scale_denominator) {
                return "Atkinson and Noto captures used different product-reported OS display scales at $scale percent, ${width}x${height}."
            }
        }
    }

    return $null
}
