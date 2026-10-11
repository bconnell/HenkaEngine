# Product-owned Scene Objects telemetry parsing and interaction helpers used by
# the packaged Sandbox checker. Dot-source after the shared log and UI helpers.

function Wait-HenkaSceneObjectsRowDrawAfterOffset {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][long]$StartingOffset,
        [int]$TimeoutMilliseconds = 5000
    )

    $rowDrawPattern = '^HENKA_AUTOMATION_DIAGNOSTIC scene_objects_row_authority entity=\d+ .+\.$'
    return Wait-FileContainsAfterOffset `
        -Path $Path `
        -Pattern $rowDrawPattern `
        -StartingOffset $StartingOffset `
        -TimeoutMilliseconds $TimeoutMilliseconds
}

function Get-HenkaSceneObjectsPagerReport {
    param([Parameter(Mandatory = $true)][string]$Path)

    $number = '[-0-9.]+'
    $pattern = '^HENKA_AUTOMATION_DIAGNOSTIC scene_objects_pager ' +
        'page=(?<page>[0-9]+) page_count=(?<pageCount>[0-9]+) ' +
        'can_previous=(?<canPrevious>[01]) can_next=(?<canNext>[01]) ' +
        'previous_rect=(?<previousX>' + $number + '),(?<previousY>' + $number + '),(?<previousWidth>' + $number + '),(?<previousHeight>' + $number + ') ' +
        'next_rect=(?<nextX>' + $number + '),(?<nextY>' + $number + '),(?<nextWidth>' + $number + '),(?<nextHeight>' + $number + ')\.'
    $match = Get-LastLogRegexMatch -Path $Path -Pattern $pattern
    if ($null -eq $match) {
        return $null
    }

    try {
        $groups = $match.Groups
        $page = [int]::Parse($groups['page'].Value, [Globalization.CultureInfo]::InvariantCulture)
        $pageCount = [int]::Parse($groups['pageCount'].Value, [Globalization.CultureInfo]::InvariantCulture)
        $canPrevious = $groups['canPrevious'].Value -eq '1'
        $canNext = $groups['canNext'].Value -eq '1'
        $previousRect = [PSCustomObject]@{
            X = [double]::Parse($groups['previousX'].Value, [Globalization.CultureInfo]::InvariantCulture)
            Y = [double]::Parse($groups['previousY'].Value, [Globalization.CultureInfo]::InvariantCulture)
            Width = [double]::Parse($groups['previousWidth'].Value, [Globalization.CultureInfo]::InvariantCulture)
            Height = [double]::Parse($groups['previousHeight'].Value, [Globalization.CultureInfo]::InvariantCulture)
        }
        $nextRect = [PSCustomObject]@{
            X = [double]::Parse($groups['nextX'].Value, [Globalization.CultureInfo]::InvariantCulture)
            Y = [double]::Parse($groups['nextY'].Value, [Globalization.CultureInfo]::InvariantCulture)
            Width = [double]::Parse($groups['nextWidth'].Value, [Globalization.CultureInfo]::InvariantCulture)
            Height = [double]::Parse($groups['nextHeight'].Value, [Globalization.CultureInfo]::InvariantCulture)
        }
    }
    catch {
        return $null
    }

    if ($pageCount -lt 2 -or $page -lt 0 -or $page -ge $pageCount -or
        $canPrevious -ne ($page -gt 0) -or $canNext -ne ($page + 1 -lt $pageCount)) {
        return $null
    }
    foreach ($rect in @($previousRect, $nextRect)) {
        if ($rect.X -lt 0.0 -or $rect.Y -lt 0.0 -or
            $rect.Width -le 0.0 -or $rect.Height -le 0.0 -or
            [double]::IsNaN($rect.X) -or [double]::IsInfinity($rect.X) -or
            [double]::IsNaN($rect.Y) -or [double]::IsInfinity($rect.Y) -or
            [double]::IsNaN($rect.Width) -or [double]::IsInfinity($rect.Width) -or
            [double]::IsNaN($rect.Height) -or [double]::IsInfinity($rect.Height)) {
            return $null
        }
    }

    return [PSCustomObject]@{
        Page = $page
        PageCount = $pageCount
        CanPrevious = $canPrevious
        CanNext = $canNext
        PreviousRect = $previousRect
        NextRect = $nextRect
        LogIndex = $match.Index
    }
}

function Move-HenkaSceneObjectsPagerThroughUi {
    param(
        [Parameter(Mandatory = $true)][PSCustomObject]$Pager,
        [Parameter(Mandatory = $true)][int]$Direction,
        [Parameter(Mandatory = $true)][System.IntPtr]$Handle,
        [Parameter(Mandatory = $true)][int]$FramebufferWidth,
        [Parameter(Mandatory = $true)][int]$FramebufferHeight,
        [Parameter(Mandatory = $true)][PSCustomObject]$SceneObjects,
        [Parameter(Mandatory = $true)][string]$StdoutPath
    )

    if ($Direction -notin @(-1, 1)) {
        throw "Scene Objects pager direction must be -1 or 1."
    }
    $canMove = if ($Direction -gt 0) { $Pager.CanNext } else { $Pager.CanPrevious }
    if (-not $canMove) {
        throw "The product-owned Scene Objects pager reports no page in the requested direction."
    }

    $button = if ($Direction -gt 0) { $Pager.NextRect } else { $Pager.PreviousRect }
    $directionName = if ($Direction -gt 0) { "Next" } else { "Previous" }
    $null = Assert-FramebufferRect `
        -Name "Product-owned Scene Objects $directionName button" `
        -FramebufferWidth $FramebufferWidth -FramebufferHeight $FramebufferHeight `
        -X $button.X -Y $button.Y -Width $button.Width -Height $button.Height
    if ($button.X -lt $SceneObjects.X -or $button.Y -lt $SceneObjects.Y -or
        $button.X + $button.Width -gt $SceneObjects.X + $SceneObjects.Width -or
        $button.Y + $button.Height -gt $SceneObjects.Y + $SceneObjects.Height) {
        throw "The product-owned Scene Objects $directionName button lies outside its active panel."
    }

    $expectedPage = $Pager.Page + $Direction
    $clickOffset = Get-FileLengthSafe -Path $StdoutPath
    Send-HenkaBackgroundFramebufferClickAndWait `
        -Handle $Handle `
        -FramebufferWidth $FramebufferWidth `
        -FramebufferHeight $FramebufferHeight `
        -FramebufferX ($button.X + $button.Width * 0.5) `
        -FramebufferY ($button.Y + $button.Height * 0.5) `
        -StdoutPath $StdoutPath `
        -Description "Navigating to Scene Objects page $($expectedPage + 1) through the real $directionName control"
    $pagePattern = '^HENKA_AUTOMATION_DIAGNOSTIC scene_objects_pager page=' +
        $expectedPage + ' page_count=' + $Pager.PageCount + ' .+\.$'
    if (-not (Wait-FileContainsAfterOffset -Path $StdoutPath -Pattern $pagePattern `
            -StartingOffset $clickOffset -TimeoutMilliseconds 5000)) {
        throw "The real Scene Objects $directionName click was consumed, but the product page state did not advance."
    }
    $nextPager = Get-HenkaSceneObjectsPagerReport -Path $StdoutPath
    if ($null -eq $nextPager -or $nextPager.Page -ne $expectedPage -or
        $nextPager.PageCount -ne $Pager.PageCount -or $nextPager.LogIndex -le $Pager.LogIndex) {
        throw "The product pager report did not confirm the requested page transition."
    }
    if (-not (Wait-HenkaSceneObjectsRowDrawAfterOffset -Path $StdoutPath `
            -StartingOffset $clickOffset -TimeoutMilliseconds 5000)) {
        throw "Scene Objects paging advanced, but its production row draw did not follow."
    }

    Write-Host ("[pass] Real Scene Objects {0} interaction changed the product page to {1}/{2}" -f `
        $directionName, ($nextPager.Page + 1), $nextPager.PageCount)
    return $nextPager
}

function Test-HenkaSceneObjectsRowAuthorityCurrent {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][System.Text.RegularExpressions.Match]$Row,
        [Parameter(Mandatory = $true)][PSCustomObject]$Pager
    )

    $layoutPattern = '^HENKA_AUTOMATION_DIAGNOSTIC scene_objects_layout result=ready ' +
        '.+ page=(?<page>\d+) page_count=(?<pageCount>\d+) .+\.$'
    $layout = Get-LastLogRegexMatch -Path $Path -Pattern $layoutPattern
    if ($null -eq $layout) {
        return $false
    }

    return (
        $layout.Index -lt $Row.Index -and
        $Row.Index -lt $Pager.LogIndex -and
        [int]$layout.Groups['page'].Value -eq $Pager.Page -and
        [int]$layout.Groups['pageCount'].Value -eq $Pager.PageCount -and
        [int]$Row.Groups['page'].Value -eq $Pager.Page -and
        [int]$Row.Groups['pageCount'].Value -eq $Pager.PageCount)
}

function Get-HenkaSceneObjectsOneLinePanelHeightFromRow {
    param(
        [Parameter(Mandatory = $true)][System.Text.RegularExpressions.Match]$Row,
        [Parameter(Mandatory = $true)][double]$PanelY,
        [Parameter(Mandatory = $true)][double]$CurrentPanelHeight,
        [Parameter(Mandatory = $true)][double]$MinimumRowHeight,
        [Parameter(Mandatory = $true)][double]$DesiredRowCapacity,
        [Parameter(Mandatory = $true)][double]$FooterHeight,
        [Parameter(Mandatory = $true)][double]$MinimumPanelHeight
    )

    if (-not $Row.Success -or $Row.Groups['hidden'].Value -ne '1' -or
        $Row.Groups['lines'].Value -ne '2' -or
        $Row.Groups['visibleCapacity'].Value -notmatch '^\d+$' -or
        [int]$Row.Groups['visibleCapacity'].Value -le 1) {
        return $null
    }

    $culture = [Globalization.CultureInfo]::InvariantCulture
    $rowY = 0.0
    $availableRowHeight = 0.0
    $rowHeight = 0.0
    if (-not [double]::TryParse($Row.Groups['y'].Value, [Globalization.NumberStyles]::Float, $culture, [ref]$rowY) -or
        -not [double]::TryParse($Row.Groups['availableRowHeight'].Value, [Globalization.NumberStyles]::Float, $culture, [ref]$availableRowHeight) -or
        -not [double]::TryParse($Row.Groups['height'].Value, [Globalization.NumberStyles]::Float, $culture, [ref]$rowHeight)) {
        return $null
    }

    foreach ($value in @($PanelY, $CurrentPanelHeight, $MinimumRowHeight, $DesiredRowCapacity,
            $FooterHeight, $MinimumPanelHeight, $rowY, $availableRowHeight, $rowHeight)) {
        if ([double]::IsNaN($value) -or [double]::IsInfinity($value)) {
            return $null
        }
    }

    $targetPanelHeight = ($rowY - $PanelY) + $FooterHeight + $DesiredRowCapacity
    if ($MinimumRowHeight -le 0.0 -or $DesiredRowCapacity -lt $MinimumRowHeight -or
        $rowHeight -le $MinimumRowHeight -or $availableRowHeight -le $DesiredRowCapacity -or
        $MinimumPanelHeight -le 0.0 -or $targetPanelHeight -lt $MinimumPanelHeight -or
        $targetPanelHeight -ge $CurrentPanelHeight) {
        return $null
    }

    return $targetPanelHeight
}

function Find-HenkaSceneObjectsRowAuthorityThroughPager {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string]$Entity,
        [Parameter(Mandatory = $true)][System.IntPtr]$Handle,
        [Parameter(Mandatory = $true)][int]$FramebufferWidth,
        [Parameter(Mandatory = $true)][int]$FramebufferHeight,
        [Parameter(Mandatory = $true)][PSCustomObject]$SceneObjects,
        [Parameter(Mandatory = $true)][string]$StdoutPath
    )

    $escapedName = [Regex]::Escape($Name)
    $escapedEntity = [Regex]::Escape($Entity)
    $number = '[-0-9.]+'
    $rowPattern = '^HENKA_AUTOMATION_DIAGNOSTIC scene_objects_row_authority ' +
        'entity=(?<entity>' + $escapedEntity + ') name=' + $escapedName + ' display=(?<display>[^\r\n]*) ' +
        'label_lines=(?<lines>\d+) hidden=(?<hidden>[01]) selected=(?<selected>[01]) ' +
        'page=(?<page>\d+) page_count=(?<pageCount>\d+) row_index=(?<rowIndex>\d+) ' +
        'first_visible=\d+ visible_count=\d+ available_row_height=(?<availableRowHeight>' + $number + ')' +
        ' line_height=' + $number + ' natural_height=' + $number +
        ' row_x=(?<x>' + $number + ') row_y=(?<y>' + $number +
        ') row_width=(?<width>' + $number + ') row_height=(?<height>' + $number +
        ') visible_line_capacity=(?<visibleCapacity>\d+) formatter=[a-z-]+ text_x=' + $number +
        ' text_y=' + $number + ' hitbox_uses_same_row_rect=(?<sameRect>1) clicked=[01]\.'

    $pager = Get-HenkaSceneObjectsPagerReport -Path $StdoutPath
    if ($null -eq $pager) {
        return $null
    }

    $targetRow = Get-LastLogRegexMatch -Path $StdoutPath -Pattern $rowPattern
    if ($null -ne $targetRow -and
        (Test-HenkaSceneObjectsRowAuthorityCurrent -Path $StdoutPath -Row $targetRow -Pager $pager)) {
        return $targetRow
    }

    # Walk the product's bounded page range using only reported controls. Every
    # transition must produce fresh page and row-draw telemetry.
    while ($pager.Page -gt 0) {
        $pager = Move-HenkaSceneObjectsPagerThroughUi `
            -Pager $pager -Direction -1 -Handle $Handle `
            -FramebufferWidth $FramebufferWidth -FramebufferHeight $FramebufferHeight `
            -SceneObjects $SceneObjects -StdoutPath $StdoutPath
        $targetRow = Get-LastLogRegexMatch -Path $StdoutPath -Pattern $rowPattern
        if ($null -ne $targetRow -and
            (Test-HenkaSceneObjectsRowAuthorityCurrent -Path $StdoutPath -Row $targetRow -Pager $pager)) {
            return $targetRow
        }
    }
    while ($pager.CanNext) {
        $pager = Move-HenkaSceneObjectsPagerThroughUi `
            -Pager $pager -Direction 1 -Handle $Handle `
            -FramebufferWidth $FramebufferWidth -FramebufferHeight $FramebufferHeight `
            -SceneObjects $SceneObjects -StdoutPath $StdoutPath
        $targetRow = Get-LastLogRegexMatch -Path $StdoutPath -Pattern $rowPattern
        if ($null -ne $targetRow -and
            (Test-HenkaSceneObjectsRowAuthorityCurrent -Path $StdoutPath -Row $targetRow -Pager $pager)) {
            return $targetRow
        }
    }

    return $null
}
