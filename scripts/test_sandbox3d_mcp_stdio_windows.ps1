[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$SandboxPath,
    [string]$CandidateIdentity = "mcp-local-smoke",
    [ValidateRange(5, 300)]
    [int]$HardTimeoutSeconds = 45
)

$ErrorActionPreference = "Stop"

$sandbox = (Resolve-Path -LiteralPath $SandboxPath).Path
if (-not (Test-Path -LiteralPath $sandbox -PathType Leaf))
{
    throw "Sandbox executable does not exist: $sandbox"
}
if ([string]::IsNullOrWhiteSpace($CandidateIdentity) -or $CandidateIdentity.Length -ge 256)
{
    throw "CandidateIdentity must be non-empty and shorter than 256 characters."
}

$process_info = [System.Diagnostics.ProcessStartInfo]::new()
$process_info.FileName = $sandbox
$process_info.Arguments = "--mcp-stdio"
$process_info.WorkingDirectory = Split-Path -Parent $sandbox
$process_info.UseShellExecute = $false
$process_info.CreateNoWindow = $true
$process_info.RedirectStandardInput = $true
$process_info.RedirectStandardOutput = $true
$process_info.RedirectStandardError = $true
$process_info.EnvironmentVariables["HENKA_CANDIDATE_ID"] = $CandidateIdentity

$process = [System.Diagnostics.Process]::new()
$process.StartInfo = $process_info

$clock = [System.Diagnostics.Stopwatch]::StartNew()
$failure = $null
$stderr_task = $null
$stderr_text = ""

function Get-RemainingMilliseconds
{
    $remaining = ($HardTimeoutSeconds * 1000) - $clock.ElapsedMilliseconds
    if ($remaining -le 0)
    {
        throw "MCP smoke exceeded its hard ${HardTimeoutSeconds}s deadline."
    }
    return [Math]::Max(1, [int]$remaining)
}

function Read-McpResponse
{
    $read_task = $process.StandardOutput.ReadLineAsync()
    if (-not $read_task.Wait((Get-RemainingMilliseconds)))
    {
        throw "MCP response did not arrive before the hard deadline."
    }
    $line = $read_task.Result
    if ([string]::IsNullOrWhiteSpace($line))
    {
        throw "MCP stdio closed before returning a JSON-RPC response."
    }
    try
    {
        $value = $line | ConvertFrom-Json
    }
    catch
    {
        throw "MCP stdout contained non-JSON protocol output: $line"
    }
    return [pscustomobject]@{ Raw = $line; Value = $value }
}

function Send-McpRequest([string]$json)
{
    if ($process.HasExited)
    {
        throw "Sandbox exited before MCP request: $json"
    }
    $process.StandardInput.WriteLine($json)
    $process.StandardInput.Flush()
    return Read-McpResponse
}

function Assert-McpSuccess($response, [int]$expected_id, [string]$label)
{
    if ($response.Value.jsonrpc -ne "2.0" -or [int]$response.Value.id -ne $expected_id)
    {
        throw "$label returned an invalid JSON-RPC envelope: $($response.Raw)"
    }
    if ($null -ne $response.Value.error)
    {
        throw "$label returned a JSON-RPC error: $($response.Raw)"
    }
    if ($null -eq $response.Value.result -or $response.Value.result.isError)
    {
        throw "$label returned a semantic MCP error: $($response.Raw)"
    }
}

try
{
    $started = $process.Start()
    if (-not $started)
    {
        throw "Unable to start Sandbox executable: $sandbox"
    }
    # Drain diagnostics continuously without a PowerShell event callback. An
    # event script block may run on a thread-pool thread with no runspace and
    # can crash the harness before the protocol result is observed.
    $stderr_task = $process.StandardError.ReadToEndAsync()

    $meta = '"_meta":{"io.modelcontextprotocol/clientInfo":{"name":"henka-mcp-smoke","version":"1.0"},"protocolVersion":"2026-07-28"}'
    $discover = Send-McpRequest "{`"jsonrpc`":`"2.0`",`"id`":1,`"method`":`"server/discover`",`"params`":{$meta}}"
    Assert-McpSuccess $discover 1 "server/discover"
    if ($discover.Value.result.structuredContent.state.spec_version -ne "2026-07-28")
    {
        throw "server/discover did not report MCP 2026-07-28."
    }

    $tools = Send-McpRequest "{`"jsonrpc`":`"2.0`",`"id`":2,`"method`":`"tools/list`",`"params`":{$meta}}"
    Assert-McpSuccess $tools 2 "tools/list"
    $tool_names = @($tools.Value.result.structuredContent.state.tools | ForEach-Object { $_.name })
    foreach ($required_tool in @("henka.observe", "henka.select_object", "henka.exit"))
    {
        if ($tool_names -notcontains $required_tool)
        {
            throw "tools/list omitted required tool: $required_tool"
        }
    }

    $observe = Send-McpRequest "{`"jsonrpc`":`"2.0`",`"id`":3,`"method`":`"tools/call`",`"params`":{`"name`":`"henka.observe`",`"arguments`":{},$meta}}"
    Assert-McpSuccess $observe 3 "henka.observe"
    $observe_state = $observe.Value.result.structuredContent.state
    if ([int]$observe_state.object_count -lt 1)
    {
        throw "henka.observe did not expose a real default scene object."
    }
    $first_object = @($observe_state.objects)[0]
    $document_id = [UInt64]$first_object.document_id
    if ($document_id -eq 0)
    {
        throw "henka.observe returned an invalid persistent document identity."
    }

    $select = Send-McpRequest "{`"jsonrpc`":`"2.0`",`"id`":4,`"method`":`"tools/call`",`"params`":{`"name`":`"henka.select_object`",`"arguments`":{`"document_id`":$document_id},$meta}}"
    Assert-McpSuccess $select 4 "henka.select_object"
    if ([UInt64]$select.Value.result.structuredContent.state.document_id -ne $document_id)
    {
        throw "henka.select_object did not return the selected persistent identity."
    }

    $selected_observe = Send-McpRequest "{`"jsonrpc`":`"2.0`",`"id`":5,`"method`":`"tools/call`",`"params`":{`"name`":`"henka.observe`",`"arguments`":{},$meta}}"
    Assert-McpSuccess $selected_observe 5 "henka.observe after selection"
    $selected_object = @($selected_observe.Value.result.structuredContent.state.objects) |
        Where-Object { [UInt64]$_.document_id -eq $document_id } |
        Select-Object -First 1
    if ($null -eq $selected_object -or -not $selected_object.selected)
    {
        throw "henka.observe did not expose the canonical selection result."
    }

    $invalid_select = Send-McpRequest "{`"jsonrpc`":`"2.0`",`"id`":6,`"method`":`"tools/call`",`"params`":{`"name`":`"henka.select_object`",`"arguments`":{`"document_id`":999999},$meta}}"
    if (-not $invalid_select.Value.result.isError)
    {
        throw "Invalid persistent identity was accepted by the live MCP path."
    }

    $exit = Send-McpRequest "{`"jsonrpc`":`"2.0`",`"id`":7,`"method`":`"tools/call`",`"params`":{`"name`":`"henka.exit`",`"arguments`":{},$meta}}"
    Assert-McpSuccess $exit 7 "henka.exit"
    if (-not $process.WaitForExit((Get-RemainingMilliseconds)))
    {
        throw "Sandbox did not exit after henka.exit before the hard deadline."
    }
    if ($stderr_task.Wait(2000))
    {
        $stderr_text = $stderr_task.Result
    }
    if ($process.ExitCode -ne 0)
    {
        throw "Sandbox exited with code $($process.ExitCode) after MCP smoke."
    }
    Write-Output "PASS MCP stdio production smoke candidate=$CandidateIdentity object_document_id=$document_id"
}
catch
{
    $failure = $_.Exception
}
finally
{
    if ($null -ne $process)
    {
        if (-not $process.HasExited)
        {
            try { $process.StandardInput.Close() } catch {}
            if (-not $process.WaitForExit(2000))
            {
                try { $process.Kill() } catch {}
                try { $process.WaitForExit(2000) } catch {}
            }
        }
        if ($null -ne $stderr_task -and $stderr_task.Wait(2000))
        {
            $stderr_text = $stderr_task.Result
        }
        $process.Dispose()
    }
}

if ($null -ne $failure)
{
    $diagnostics = $stderr_text.Trim()
    Write-Error ("MCP smoke failed: " + $failure.ToString())
    if ($diagnostics.Length -gt 0)
    {
        Write-Error ("MCP stderr:`n" + $diagnostics)
    }
    throw $failure
}
