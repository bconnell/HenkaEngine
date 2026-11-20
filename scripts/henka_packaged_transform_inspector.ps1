function Invoke-PackagedTransformInspectorWorkflow {
    param(
        [Parameter(Mandatory = $true)][System.IntPtr]$Handle,
        [Parameter(Mandatory = $true)][int]$FramebufferWidth,
        [Parameter(Mandatory = $true)][int]$FramebufferHeight,
        [Parameter(Mandatory = $true)][double]$DetailsX,
        [Parameter(Mandatory = $true)][double]$DetailsY,
        [Parameter(Mandatory = $true)][double]$DetailsWidth,
        [Parameter(Mandatory = $true)][double]$DetailsHeight,
        [Parameter(Mandatory = $true)][uint32]$EntityId,
        [Parameter(Mandatory = $true)][string]$EventPath,
        [Parameter(Mandatory = $true)][string]$StdoutPath,
        [Parameter(Mandatory = $true)][string]$ScreenshotPath
    )

    $entity = [Regex]::Escape([string]$EntityId)
    $number = '[-0-9.]+'
    $disclosurePattern = '^HENKA_AUTOMATION_DIAGNOSTIC object_details_disclosure id=transform entity=' +
        $entity + ' x=(?<x>' + $number + ') y=(?<y>' + $number + ') width=(?<width>' +
        $number + ') height=(?<height>' + $number + ') expanded=(?<expanded>[01])\.'
    $expandedDisclosurePattern = '^HENKA_AUTOMATION_DIAGNOSTIC object_details_disclosure id=transform entity=' +
        $entity + ' x=' + $number + ' y=' + $number + ' width=' + $number +
        ' height=' + $number + ' expanded=1\.'
    $rowPattern = '^HENKA_AUTOMATION_DIAGNOSTIC transform_row entity=' + $entity +
        ' row=position editable=1 rect=(?<rowX>' + $number + '),(?<rowY>' + $number +
        '),(?<rowWidth>' + $number + '),(?<rowHeight>' + $number + ') label=' +
        $number + ',' + $number + ',' + $number + ',' + $number +
        ' field0=(?<fieldX>' + $number + '),(?<fieldY>' + $number + '),(?<fieldWidth>' +
        $number + '),(?<fieldHeight>' + $number + ') field1=(?<field1X>' + $number +
        '),(?<field1Y>' + $number + '),(?<field1Width>' + $number + '),(?<field1Height>' +
        $number + ') field2=(?<field2X>' + $number + '),(?<field2Y>' + $number +
        '),(?<field2Width>' + $number + '),(?<field2Height>' + $number + ')'
    $actionsPrefix = '^HENKA_AUTOMATION_DIAGNOSTIC transform_actions entity=' + $entity +
        ' editable=1 dirty='
    $actionsSuffix = ' rect=' + $number + ',' + $number + ',' +
        $number + ',' + $number + ' label=' + $number + ',' + $number + ',' +
        $number + ',' + $number + ' apply=(?<applyX>' + $number + '),(?<applyY>' +
        $number + '),(?<applyWidth>' + $number + '),(?<applyHeight>' + $number +
        ') revert=(?<revertX>' + $number + '),(?<revertY>' + $number +
        '),(?<revertWidth>' + $number + '),(?<revertHeight>' + $number + ')'
    $dirtyActionsPattern = $actionsPrefix + '1' + $actionsSuffix
    $cleanActionsPattern = $actionsPrefix + '0' + $actionsSuffix
    $detailsScrollX = $DetailsX + [Math]::Max(12.0, $DetailsWidth - 18.0)
    $detailsScrollY = $DetailsY + [Math]::Max(30.0, $DetailsHeight * 0.55)
    $disclosure = Get-LastLogRegexMatch -Path $StdoutPath -Pattern $disclosurePattern

    for ($attempt = 0; $null -eq $disclosure -and $attempt -le 8; ++$attempt) {
        if ($attempt -eq 8) {
            break
        }
        $direction = if ($attempt -lt 4) { 1 } else { -1 }
        Scroll-FramebufferPointAndWaitForConsumption `
            -Handle $Handle `
            -FramebufferWidth $FramebufferWidth `
            -FramebufferHeight $FramebufferHeight `
            -FramebufferX $detailsScrollX `
            -FramebufferY $detailsScrollY `
            -WheelDelta $direction `
            -TimeoutMilliseconds 3000
        $disclosure = Get-LastLogRegexMatch -Path $StdoutPath -Pattern $disclosurePattern
    }
    if ($null -eq $disclosure) {
        throw "The product did not expose the selected Cube's Transform disclosure through the visible Object Details panel."
    }

    $disclosureX = [double]$disclosure.Groups['x'].Value
    $disclosureY = [double]$disclosure.Groups['y'].Value
    $disclosureWidth = [double]$disclosure.Groups['width'].Value
    $disclosureHeight = [double]$disclosure.Groups['height'].Value
    if ($disclosureX -lt $DetailsX -or
        $disclosureY -lt $DetailsY -or
        ($disclosureX + $disclosureWidth) -gt ($DetailsX + $DetailsWidth) -or
        ($disclosureY + $disclosureHeight) -gt ($DetailsY + $DetailsHeight)) {
        throw 'The product-reported Transform disclosure is outside the visible Object Details panel.'
    }

    if ($disclosure.Groups['expanded'].Value -ne '1') {
        $expandOffset = Get-FileLengthSafe -Path $StdoutPath
        Click-FramebufferPoint `
            -Handle $Handle `
            -FramebufferWidth $FramebufferWidth `
            -FramebufferHeight $FramebufferHeight `
            -FramebufferX ([double]$disclosure.Groups['x'].Value + [double]$disclosure.Groups['width'].Value * 0.5) `
            -FramebufferY ([double]$disclosure.Groups['y'].Value + [double]$disclosure.Groups['height'].Value * 0.5)
        if (-not (Wait-FileContainsAfterOffset `
                -Path $StdoutPath `
                -Pattern $expandedDisclosurePattern `
                -StartingOffset $expandOffset `
                -TimeoutMilliseconds 3000)) {
            throw "A real click on the product-reported Transform disclosure did not expand Object Details."
        }
    }
    Write-Output '[pass] Product-native Transform disclosure opened for the selected Cube'

    $row = $null
    $rowVisible = $false
    for ($attempt = 0; -not $rowVisible -and $attempt -le 8; ++$attempt) {
        $row = Get-LastLogRegexMatch -Path $StdoutPath -Pattern $rowPattern
        if ($null -ne $row) {
            $rowX = [double]$row.Groups['rowX'].Value
            $rowY = [double]$row.Groups['rowY'].Value
            $rowWidth = [double]$row.Groups['rowWidth'].Value
            $rowHeight = [double]$row.Groups['rowHeight'].Value
            $rowVisible = $rowX -ge $DetailsX -and
                $rowY -ge $DetailsY -and
                ($rowX + $rowWidth) -le ($DetailsX + $DetailsWidth) -and
                ($rowY + $rowHeight) -le ($DetailsY + $DetailsHeight)
        }
        if ($rowVisible) {
            break
        }
        if ($attempt -eq 8) {
            break
        }
        Scroll-FramebufferPointAndWaitForConsumption `
            -Handle $Handle `
            -FramebufferWidth $FramebufferWidth `
            -FramebufferHeight $FramebufferHeight `
            -FramebufferX $detailsScrollX `
            -FramebufferY $detailsScrollY `
            -WheelDelta -1 `
            -TimeoutMilliseconds 3000
    }
    if (-not $rowVisible -or $null -eq $row) {
        throw 'The expanded Transform section did not expose a visible editable Position row in the packaged Object Details panel.'
    }

    foreach ($rect in @(
        @{ X = [double]$row.Groups['rowX'].Value; Y = [double]$row.Groups['rowY'].Value; Width = [double]$row.Groups['rowWidth'].Value; Height = [double]$row.Groups['rowHeight'].Value; Name = 'Transform Position row' },
        @{ X = [double]$row.Groups['fieldX'].Value; Y = [double]$row.Groups['fieldY'].Value; Width = [double]$row.Groups['fieldWidth'].Value; Height = [double]$row.Groups['fieldHeight'].Value; Name = 'Transform Position X field' },
        @{ X = [double]$row.Groups['field1X'].Value; Y = [double]$row.Groups['field1Y'].Value; Width = [double]$row.Groups['field1Width'].Value; Height = [double]$row.Groups['field1Height'].Value; Name = 'Transform Position Y field' },
        @{ X = [double]$row.Groups['field2X'].Value; Y = [double]$row.Groups['field2Y'].Value; Width = [double]$row.Groups['field2Width'].Value; Height = [double]$row.Groups['field2Height'].Value; Name = 'Transform Position Z field' }
    )) {
        Assert-FramebufferRect `
            -Name $rect.Name `
            -FramebufferWidth $FramebufferWidth `
            -FramebufferHeight $FramebufferHeight `
            -X $rect.X `
            -Y $rect.Y `
            -Width $rect.Width `
            -Height $rect.Height
        if ($rect.X -lt $DetailsX -or
            $rect.Y -lt $DetailsY -or
            ($rect.X + $rect.Width) -gt ($DetailsX + $DetailsWidth) -or
            ($rect.Y + $rect.Height) -gt ($DetailsY + $DetailsHeight)) {
            throw "$($rect.Name) is outside the visible Object Details panel."
        }
    }

    $dirtyOffset = Get-FileLengthSafe -Path $StdoutPath
    Click-FramebufferPoint `
        -Handle $Handle `
        -FramebufferWidth $FramebufferWidth `
        -FramebufferHeight $FramebufferHeight `
        -FramebufferX ([double]$row.Groups['fieldX'].Value + [double]$row.Groups['fieldWidth'].Value * 0.5) `
        -FramebufferY ([double]$row.Groups['fieldY'].Value + [double]$row.Groups['fieldHeight'].Value * 0.5)
    for ($key = 0; $key -lt 16; ++$key) {
        Send-HenkaAutomationKey -EventPath $EventPath -KeyName 'Backspace'
    }
    Send-HenkaAutomationText -EventPath $EventPath -Text '1.25'
    if (-not (Wait-FileContainsAfterOffset `
            -Path $StdoutPath `
            -Pattern $dirtyActionsPattern `
            -StartingOffset $dirtyOffset `
            -TimeoutMilliseconds 3000)) {
        throw "The Position X value was not accepted through the real packaged text field."
    }
    $actions = Get-LastLogRegexMatch -Path $StdoutPath -Pattern $dirtyActionsPattern
    if ($null -eq $actions) {
        throw 'The product did not report the pending transform edit and Apply hit region.'
    }
    Assert-FramebufferRect `
        -Name 'Transform Apply control' `
        -FramebufferWidth $FramebufferWidth `
        -FramebufferHeight $FramebufferHeight `
        -X ([double]$actions.Groups['applyX'].Value) `
        -Y ([double]$actions.Groups['applyY'].Value) `
        -Width ([double]$actions.Groups['applyWidth'].Value) `
        -Height ([double]$actions.Groups['applyHeight'].Value)
    if ([double]$actions.Groups['applyX'].Value -lt $DetailsX -or
        [double]$actions.Groups['applyY'].Value -lt $DetailsY -or
        ([double]$actions.Groups['applyX'].Value + [double]$actions.Groups['applyWidth'].Value) -gt ($DetailsX + $DetailsWidth) -or
        ([double]$actions.Groups['applyY'].Value + [double]$actions.Groups['applyHeight'].Value) -gt ($DetailsY + $DetailsHeight)) {
        throw 'The product-reported Apply control is outside the visible Object Details panel.'
    }

    $commitOffset = Get-FileLengthSafe -Path $StdoutPath
    Click-FramebufferPoint `
        -Handle $Handle `
        -FramebufferWidth $FramebufferWidth `
        -FramebufferHeight $FramebufferHeight `
        -FramebufferX ([double]$actions.Groups['applyX'].Value + [double]$actions.Groups['applyWidth'].Value * 0.5) `
        -FramebufferY ([double]$actions.Groups['applyY'].Value + [double]$actions.Groups['applyHeight'].Value * 0.5)
    $commitPattern = '^HENKA_AUTOMATION_DIAGNOSTIC transform_commit entity=' + $entity + ' position=1\.25,'
    if (-not (Wait-FileContainsAfterOffset `
            -Path $StdoutPath `
            -Pattern $commitPattern `
            -StartingOffset $commitOffset `
            -TimeoutMilliseconds 5000)) {
        throw "Apply did not commit Position X=1.25 to the canonical live scene transform."
    }
    if (-not (Wait-FileContainsAfterOffset `
            -Path $StdoutPath `
            -Pattern $cleanActionsPattern `
            -StartingOffset $commitOffset `
            -TimeoutMilliseconds 3000)) {
        throw 'The committed Transform Inspector did not return to its clean state.'
    }
    Write-Output '[pass] Real packaged transform edit changed the canonical live object transform'

    $commitCountBeforeRevert = [Regex]::Matches(
        [IO.File]::ReadAllText($StdoutPath),
        '^HENKA_AUTOMATION_DIAGNOSTIC transform_commit entity=' + $entity,
        [Text.RegularExpressions.RegexOptions]::Multiline).Count
    $row = Get-LastLogRegexMatch -Path $StdoutPath -Pattern $rowPattern
    if ($null -eq $row) {
        throw 'Transform Position geometry disappeared before the Revert interaction.'
    }
    $revertDirtyOffset = Get-FileLengthSafe -Path $StdoutPath
    Click-FramebufferPoint `
        -Handle $Handle `
        -FramebufferWidth $FramebufferWidth `
        -FramebufferHeight $FramebufferHeight `
        -FramebufferX ([double]$row.Groups['fieldX'].Value + [double]$row.Groups['fieldWidth'].Value * 0.5) `
        -FramebufferY ([double]$row.Groups['fieldY'].Value + [double]$row.Groups['fieldHeight'].Value * 0.5)
    for ($key = 0; $key -lt 16; ++$key) {
        Send-HenkaAutomationKey -EventPath $EventPath -KeyName 'Backspace'
    }
    Send-HenkaAutomationText -EventPath $EventPath -Text '9'
    if (-not (Wait-FileContainsAfterOffset `
            -Path $StdoutPath `
            -Pattern $dirtyActionsPattern `
            -StartingOffset $revertDirtyOffset `
            -TimeoutMilliseconds 3000)) {
        throw 'A second real field edit did not become pending before Revert.'
    }
    $actions = Get-LastLogRegexMatch -Path $StdoutPath -Pattern $dirtyActionsPattern
    Assert-FramebufferRect `
        -Name 'Transform Revert control' `
        -FramebufferWidth $FramebufferWidth `
        -FramebufferHeight $FramebufferHeight `
        -X ([double]$actions.Groups['revertX'].Value) `
        -Y ([double]$actions.Groups['revertY'].Value) `
        -Width ([double]$actions.Groups['revertWidth'].Value) `
        -Height ([double]$actions.Groups['revertHeight'].Value)
    if ([double]$actions.Groups['revertX'].Value -lt $DetailsX -or
        [double]$actions.Groups['revertY'].Value -lt $DetailsY -or
        ([double]$actions.Groups['revertX'].Value + [double]$actions.Groups['revertWidth'].Value) -gt ($DetailsX + $DetailsWidth) -or
        ([double]$actions.Groups['revertY'].Value + [double]$actions.Groups['revertHeight'].Value) -gt ($DetailsY + $DetailsHeight)) {
        throw 'The product-reported Revert control is outside the visible Object Details panel.'
    }

    $revertOffset = Get-FileLengthSafe -Path $StdoutPath
    Click-FramebufferPoint `
        -Handle $Handle `
        -FramebufferWidth $FramebufferWidth `
        -FramebufferHeight $FramebufferHeight `
        -FramebufferX ([double]$actions.Groups['revertX'].Value + [double]$actions.Groups['revertWidth'].Value * 0.5) `
        -FramebufferY ([double]$actions.Groups['revertY'].Value + [double]$actions.Groups['revertHeight'].Value * 0.5)
    if (-not (Wait-FileContainsAfterOffset `
            -Path $StdoutPath `
            -Pattern $cleanActionsPattern `
            -StartingOffset $revertOffset `
            -TimeoutMilliseconds 3000)) {
        throw 'A real Revert click did not discard the pending edit and restore the committed value.'
    }
    $commitCountAfterRevert = [Regex]::Matches(
        [IO.File]::ReadAllText($StdoutPath),
        '^HENKA_AUTOMATION_DIAGNOSTIC transform_commit entity=' + $entity,
        [Text.RegularExpressions.RegexOptions]::Multiline).Count
    if ($commitCountAfterRevert -ne $commitCountBeforeRevert) {
        throw 'Revert unexpectedly published a second canonical transform commit.'
    }
    Assert-HenkaAutomationBackground -Handle $Handle
    Save-WindowScreenshot `
        -Handle $Handle `
        -Path $ScreenshotPath `
        -Description 'Packaged Transform Inspector after Apply and Revert'
    Write-Output '[pass] Real packaged Revert restored the committed field value without another scene mutation'
}
