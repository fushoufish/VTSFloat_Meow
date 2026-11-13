param(
    [Parameter(Mandatory = $true)][string] $TaskPath,
    [Parameter(Mandatory = $true)][string] $Token,
    [int] $ResumePid = 0,
    [string] $ResumeCreated = ''
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$utf8 = [System.Text.UTF8Encoding]::new($false)
$taskFile = [IO.Path]::GetFullPath($TaskPath)
$taskDirectory = [IO.Path]::GetDirectoryName($taskFile)
$cacheDirectory = [IO.Path]::GetDirectoryName($taskDirectory)

function Read-Json([string] $Path) {
    $fileInfo = [IO.FileInfo]::new($Path)
    if ($fileInfo.Length -gt 65536) { throw 'Update record exceeds its size limit.' }
    return ConvertFrom-Json -InputObject ([IO.File]::ReadAllText($Path, $utf8))
}

function Write-Json([string] $Path, $Value) {
    $temporary = "$Path.$PID.tmp"
    $data = $utf8.GetBytes((ConvertTo-Json -InputObject $Value -Depth 4 -Compress))
    $stream = [IO.FileStream]::new($temporary, [IO.FileMode]::Create,
        [IO.FileAccess]::Write, [IO.FileShare]::None, 4096, [IO.FileOptions]::WriteThrough)
    try {
        $stream.Write($data, 0, $data.Length)
        $stream.Flush($true)
    } finally {
        $stream.Dispose()
    }
    if ([IO.File]::Exists($Path)) {
        [IO.File]::Replace($temporary, $Path, [NullString]::Value)
    } else {
        [IO.File]::Move($temporary, $Path)
    }
}

function Get-Created([Diagnostics.Process] $Process) {
    return $Process.StartTime.ToUniversalTime().ToFileTimeUtc().ToString(
        [Globalization.CultureInfo]::InvariantCulture)
}

function Get-ApplicationProcess([int] $Id, [string] $Created) {
    $found = [Diagnostics.Process]::GetProcessById($Id)
    if ($found.HasExited -or (Get-Created $found) -cne $Created -or
        $found.MainModule.FileName -ine $target) {
        throw 'Running application identity does not match the update task.'
    }
    # Keep the process handle before the other process can exit.
    $null = $found.Handle
    return $found
}

function Get-Sha256([string] $Path) {
    $stream = [IO.File]::OpenRead($Path)
    $hash = [Security.Cryptography.SHA256]::Create()
    try {
        return [BitConverter]::ToString($hash.ComputeHash($stream)).Replace('-', '').ToLowerInvariant()
    } finally {
        $hash.Dispose()
        $stream.Dispose()
    }
}

function Confirm-File([string] $Path, [string] $Digest, [long] $Size = -1) {
    if (-not [IO.File]::Exists($Path) -or
        ($Size -ge 0 -and [IO.FileInfo]::new($Path).Length -ne $Size) -or
        (Get-Sha256 $Path) -cne $Digest) {
        throw "File integrity check failed: $Path"
    }
}

function Write-Signal([string] $Name, [Diagnostics.Process] $Application = $null) {
    $value = @{
        token = $Token
        pid = $PID
        created = $updaterCreated
        tag = $task.tag
    }
    if ($null -ne $Application) {
        $value.app_pid = $Application.Id
        $value.app_created = Get-Created $Application
    }
    Write-Json (Join-Path $taskDirectory $Name) $value
}

function Test-Signal([string] $Name, [int] $Id, [string] $Created) {
    $file = Join-Path $taskDirectory $Name
    if (-not [IO.File]::Exists($file)) { return $false }
    try {
        $signal = Read-Json $file
        return $signal.token -ceq $Token -and $signal.tag -ceq $task.tag -and
            $signal.pid -eq $Id -and $signal.created -ceq $Created
    } catch {
        return $false
    }
}

function Wait-Signal([string] $Name, [Diagnostics.Process] $Application,
                     [string] $Created, [int] $Seconds) {
    $elapsed = [Diagnostics.Stopwatch]::StartNew()
    while ($elapsed.Elapsed.TotalSeconds -lt $Seconds) {
        if (Test-Signal $Name $Application.Id $Created) { return }
        if (Test-Signal 'abort.json' $Application.Id $Created) {
            throw 'Update was cancelled before replacement.'
        }
        if ($Application.HasExited) { throw "Application exited before $Name." }
        Start-Sleep -Milliseconds 100
    }
    throw "Timed out waiting for $Name."
}

function Set-Phase([string] $Phase) {
    $task.phase = $Phase
    Write-Json $taskFile $task
}

function Remove-TaskFiles {
    # All paths come from the validated task ID, executable and fixed filenames.
    foreach ($file in @($backup, $staged, $failedNew)) {
        [IO.File]::Delete($file)
    }
    foreach ($name in @('ready.json', 'commit.json', 'abort.json', 'app-ready.json',
                        'show-success.json', 'shown.json', 'payload.part',
                        'failure.txt', 'updater.ps1', 'task.json')) {
        [IO.File]::Delete((Join-Path $taskDirectory $name))
    }
    [IO.Directory]::Delete($taskDirectory, $false)
}

function Write-Result([bool] $Restored, [string] $Detail) {
    Write-Json (Join-Path $cacheDirectory 'last-result.json') @{
        target = $target
        restored = $Restored
        detail = $Detail
    }
}

$application = $null
$rollbackAllowed = $false
$committed = $false
$validated = $false

try {
    if ($Token -cnotmatch '\A[0-9a-f]{64}\z' -or
        [IO.Path]::GetFileName($taskDirectory) -cne $Token.Substring(0, 32) -or
        [IO.Path]::GetFileName($cacheDirectory) -cne 'VTSFloat_Meow-updates' -or
        [IO.Path]::GetFileName($taskFile) -cne 'task.json') {
        throw 'Invalid update task location.'
    }
    $task = Read-Json $taskFile
    if ($task.schema -ne 1 -or $task.token -cne $Token -or
        $task.tag -cnotmatch '\A[vV]?[0-9]{1,5}\.[0-9]{1,5}(\.[0-9]{1,5})?\z' -or
        $task.sha256 -cnotmatch '\A[0-9a-f]{64}\z' -or
        $task.original_sha256 -cnotmatch '\A[0-9a-f]{64}\z' -or
        $task.size -lt 1024 -or $task.size -gt 134217728) {
        throw 'Invalid update task fields.'
    }
    $target = [IO.Path]::GetFullPath([string] $task.target)
    if ($target -ine $task.target -or [IO.Path]::GetExtension($target) -ine '.exe') {
        throw 'Invalid application path.'
    }
    $sidePrefix = "$target.update-" + $Token.Substring(0, 32)
    $staged = "$sidePrefix.new"
    $backup = "$sidePrefix.bak"
    $failedNew = "$sidePrefix.failed"
    $workingDirectory = [IO.Path]::GetDirectoryName($target)
    $updaterCreated = Get-Created ([Diagnostics.Process]::GetCurrentProcess())
    $task.updater_pid = $PID
    $task.updater_created = $updaterCreated
    $validated = $true

    if ($ResumePid -gt 0) {
        $application = Get-ApplicationProcess $ResumePid $ResumeCreated
        Confirm-File $target $task.sha256 $task.size
        Confirm-File $backup $task.original_sha256
        Set-Phase 'starting'
        Write-Signal 'ready.json'
    } else {
        if ($task.phase -cne 'prepared' -or [IO.File]::Exists($backup)) {
            throw 'The update was not prepared for replacement.'
        }
        $original = Get-ApplicationProcess $task.parent_pid $task.parent_created
        Confirm-File $target $task.original_sha256
        Confirm-File $staged $task.sha256 $task.size
        Set-Phase 'prepared'
        Write-Signal 'ready.json'
        Wait-Signal 'commit.json' $original $task.parent_created 25
        if (-not $original.WaitForExit(45000)) {
            throw 'The original application did not exit.'
        }
        $rollbackAllowed = $true
        Confirm-File $target $task.original_sha256
        Confirm-File $staged $task.sha256 $task.size
        Set-Phase 'replacing'
        [IO.File]::Replace($staged, $target, $backup)
        Set-Phase 'starting'
        $launch = @{
            FilePath = $target
            ArgumentList = ('--update-task "{0}" --update-token {1}' -f $taskFile, $Token)
            WorkingDirectory = $workingDirectory
            PassThru = $true
        }
        $application = Start-Process @launch
    }

    $appCreated = Get-Created $application
    Wait-Signal 'app-ready.json' $application $appCreated 45
    $rollbackAllowed = $true
    if ($application.WaitForExit(2500)) { throw 'The updated application exited during startup.' }
    Set-Phase 'confirming'
    Write-Signal 'show-success.json' $application
    Wait-Signal 'shown.json' $application $appCreated 15
    # Keep the backup for startup recovery if writing the final record fails.
    $committed = $true
    Set-Phase 'committed'
    Remove-TaskFiles
    exit 0
} catch {
    $failure = $_.Exception.Message
    if ($committed) { exit 0 }
    if ($validated) {
        try { [IO.File]::WriteAllText((Join-Path $taskDirectory 'failure.txt'), $failure, $utf8) } catch {}
    }
    if (-not $rollbackAllowed) { exit 1 }
    try {
        try { Set-Phase 'restoring' } catch {}
        if ($null -ne $application -and -not $application.HasExited) {
            $null = $application.CloseMainWindow()
            if (-not $application.WaitForExit(2000)) {
                $application.Kill()
                if (-not $application.WaitForExit(5000)) { throw 'The updated process could not be stopped.' }
            }
        }
        if ([IO.File]::Exists($backup)) {
            Confirm-File $backup $task.original_sha256
            if ([IO.File]::Exists($target)) {
                [IO.File]::Replace($backup, $target, $failedNew)
            } else {
                [IO.File]::Move($backup, $target)
            }
        }
        Confirm-File $target $task.original_sha256
        try { Write-Result $true $failure } catch {}
        Start-Process -FilePath $target -WorkingDirectory $workingDirectory
        try { Remove-TaskFiles } catch {}
    } catch {
        $restoreFailure = "$failure / Restore: " + $_.Exception.Message
        try { Write-Result $false $restoreFailure } catch {}
        try {
            Add-Type -AssemblyName System.Windows.Forms
            $null = [System.Windows.Forms.MessageBox]::Show(
                "VTSFloat_Meow could not finish restoring the previous version. Backup: $backup",
                'VTSFloat_Meow')
        } catch {}
    }
    exit 1
}
