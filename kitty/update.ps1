# Keeps the kitty-graphics Windows Terminal ("Terminal Dev") on the newest
# Windows Terminal Preview release.
#
# Run by the scheduled task "WT Kitty Update" (weekly) under the portable
# PowerShell 7 in C:\src\tools\pwsh, never from WSL: WSL hands Windows
# processes both PATH and Path, and MSBuild fails on the duplicate (MSB6001).
#
#   1. Find the newest Preview release of microsoft/terminal.
#   2. If kitty-graphics already contains it, stop.
#   3. Rebase kitty-graphics onto it, build the package, and push the branch.
#   4. Install the build, or, while Terminal Dev is running, leave it pending:
#      registering a loose package cannot be deferred, and forcing it would
#      close every Terminal Dev window. The task "WT Kitty Install" runs
#      -InstallPending every 30 minutes and installs once Terminal Dev is
#      closed.
#   5. Toast the outcome. On a conflict or a failed build the branch goes back
#      to where it was and the installed build is left alone.
#
# -Force rebuilds and reinstalls even when there is no new release.
# Log: C:\src\wt-update.log (C:\src\wt-install.log for -InstallPending);
# MSBuild detail: C:\src\wt-msbuild.log.
param([switch]$Force, [switch]$InstallPending)

$ErrorActionPreference = 'Continue'
$repo = 'C:\src\wt'
$branch = 'kitty-graphics'
$pendingFile = 'C:\src\wt-dev\PENDING'
Start-Transcript -Path ($InstallPending ? 'C:\src\wt-install.log' : 'C:\src\wt-update.log') -Force | Out-Null

function Send-Toast([string]$title, [string]$body) {
    # WinRT toasts need Windows PowerShell; pwsh 7 cannot load the types.
    $t = $title.Replace("'", "''")
    $b = $body.Replace("'", "''")
    powershell.exe -NoProfile -Command @"
[Windows.UI.Notifications.ToastNotificationManager, Windows.UI.Notifications, ContentType = WindowsRuntime] | Out-Null
`$xml = [Windows.UI.Notifications.ToastNotificationManager]::GetTemplateContent([Windows.UI.Notifications.ToastTemplateType]::ToastText02)
`$text = `$xml.GetElementsByTagName('text')
[void]`$text.Item(0).AppendChild(`$xml.CreateTextNode('$t'))
[void]`$text.Item(1).AppendChild(`$xml.CreateTextNode('$b'))
`$appId = '{1AC14E77-02E7-4E5D-B744-2EB1AE5198B7}\WindowsPowerShell\v1.0\powershell.exe'
[Windows.UI.Notifications.ToastNotificationManager]::CreateToastNotifier(`$appId).Show([Windows.UI.Notifications.ToastNotification]::new(`$xml))
"@
}

function Get-TerminalDevLocation {
    (Get-AppxPackage WindowsTerminalDev -ErrorAction SilentlyContinue).InstallLocation
}

function Test-TerminalDevRunning {
    $location = Get-TerminalDevLocation
    [bool]($location -and (Get-Process WindowsTerminal -ErrorAction SilentlyContinue |
        Where-Object { $_.Path -and $_.Path.StartsWith($location, [StringComparison]::OrdinalIgnoreCase) }))
}

# Registers the build named in the PENDING file, unless Terminal Dev is open.
function Install-PendingBuild {
    if (-not (Test-Path $pendingFile)) { return 'none' }
    if (Test-TerminalDevRunning) { return 'waiting' }
    $folder = (Get-Content $pendingFile -Raw).Trim()
    Add-AppxPackage -Path "$folder\AppxManifest.xml" -Register -ForceUpdateFromAnyVersion -ErrorAction Stop
    # Check rather than trust: a registration that changes nothing succeeds.
    if ((Get-TerminalDevLocation) -ne $folder) {
        throw "registered $folder, but Terminal Dev still runs from $(Get-TerminalDevLocation)"
    }
    Remove-Item $pendingFile
    Send-Toast 'Terminal Dev updated' "Now running the build in $(Split-Path $folder -Leaf)."
    return 'installed'
}

# Returns only git's exit code; its output goes to the transcript through the
# host, so it does not end up in the return value.
function Invoke-Git {
    & git -C $repo @args 2>&1 | ForEach-Object { Write-Host "git: $_" }
    return $LASTEXITCODE
}

try {
    # A fresh-logon PATH, with %USERPROFILE%-style entries expanded, plus the
    # portable pwsh that the build's own steps call, and git.
    $env:PATH = [Environment]::ExpandEnvironmentVariables((@(
        'C:\src\tools\pwsh',
        [Environment]::GetEnvironmentVariable('PATH', 'Machine'),
        [Environment]::GetEnvironmentVariable('PATH', 'User'),
        'C:\Program Files\Git\cmd'
    ) -join ';'))

    if ($InstallPending) {
        "INSTALL-RESULT $(Install-PendingBuild)"
        return
    }

    # 1. The newest Preview release (GitHub marks Preview releases as prereleases).
    $releases = Invoke-RestMethod 'https://api.github.com/repos/microsoft/terminal/releases?per_page=30'
    $tag = ($releases | Where-Object { $_.prerelease -and $_.name -like '*Preview*' } | Select-Object -First 1).tag_name
    if (-not $tag) { throw 'no Preview release found' }
    "newest Preview: $tag"

    [void](Invoke-Git fetch upstream "refs/tags/${tag}:refs/tags/$tag" --no-tags)
    $current = (& git -C $repo describe --tags --abbrev=0 --match 'v1.*' $branch 2>$null)
    "kitty-graphics is based on: $current"

    # 2. Already on it?
    & git -C $repo merge-base --is-ancestor $tag $branch
    $upToDate = $LASTEXITCODE -eq 0
    if ($upToDate -and -not $Force) {
        'UPDATE-RESULT up-to-date'
        return
    }

    # 3a. Rebase, keeping the old tip so a failure can go back to it.
    $before = (& git -C $repo rev-parse $branch).Trim()
    [void](Invoke-Git checkout $branch)
    if (-not $upToDate) {
        if ((Invoke-Git rebase --onto $tag $current $branch) -ne 0) {
            [void](Invoke-Git rebase --abort)
            [void](Invoke-Git reset --hard $before)
            Send-Toast 'Terminal Dev update needs attention' "Rebasing kitty graphics onto $tag hit a conflict. The current build is untouched."
            'UPDATE-RESULT rebase-conflict'
            return
        }
    }

    # 3b. Build.
    Set-Location $repo
    Import-Module .\tools\OpenConsole.psm1
    Set-MsBuildDevEnvironment
    & .\dep\nuget\nuget.exe restore .\OpenConsole.slnx
    & .\dep\nuget\nuget.exe restore .\dep\nuget\packages.config
    msbuild.exe .\OpenConsole.slnx /t:Terminal\CascadiaPackage /p:Configuration=Release /p:Platform=x64 `
        /p:AppxSymbolPackageEnabled=false /m /v:minimal /nologo `
        "/flp:Verbosity=normal;LogFile=C:\src\wt-msbuild.log"
    if ($LASTEXITCODE -ne 0) {
        [void](Invoke-Git reset --hard $before)
        Send-Toast 'Terminal Dev update needs attention' "The build on $tag failed; see C:\src\wt-msbuild.log. The current build is untouched."
        'UPDATE-RESULT build-failed'
        return
    }

    # 3c. Unpack into a folder per build, so the running build's files are
    #     never overwritten.
    $msix = Get-ChildItem "$repo\src\cascadia\CascadiaPackage\AppPackages\*\CascadiaPackage_*_x64.msix" |
        Sort-Object LastWriteTime -Descending | Select-Object -First 1
    $loose = "C:\src\wt-dev\$tag-$(Get-Date -Format yyyyMMddHHmm)"
    $makeappx = Get-ChildItem 'C:\Program Files (x86)\Windows Kits\10\bin\10.*\x64\makeappx.exe' |
        Sort-Object FullName -Descending | Select-Object -First 1
    & $makeappx.FullName unpack /o /p $msix.FullName /d $loose | Select-Object -Last 1
    # Every local build is version 0.0.1.0, and registering the same version
    # from another folder silently does nothing. Give each build its own: the
    # Preview's version, with the hours since 2026 as the last part.
    $version = "$(($tag.TrimStart('v') -split '\.')[0..2] -join '.').$([int]((Get-Date) - [datetime]'2026-01-01').TotalHours % 65535)"
    $manifest = "$loose\AppxManifest.xml"
    (Get-Content $manifest -Raw) -replace '(<Identity\b[^>]*\bVersion=")[^"]+', "`${1}$version" | Set-Content $manifest -NoNewline
    "package version: $version"
    [void](Invoke-Git push --force-with-lease origin $branch)

    # 4. Install now if Terminal Dev is closed, otherwise leave it pending.
    Set-Content -Path $pendingFile -Value $loose
    $installed = Install-PendingBuild
    if ($installed -eq 'waiting') {
        Send-Toast 'Terminal Dev update ready' "Built on Windows Terminal Preview $tag. It installs after you close every Terminal Dev window."
    }

    # Keep the three newest build folders, and never the one installed or pending.
    $keep = @((Get-TerminalDevLocation), $loose)
    Get-ChildItem C:\src\wt-dev -Directory | Sort-Object CreationTime -Descending | Select-Object -Skip 3 |
        Where-Object { $keep -notcontains $_.FullName } | Remove-Item -Recurse -Force -ErrorAction SilentlyContinue

    "UPDATE-RESULT updated $tag ($installed)"
}
catch {
    Send-Toast 'Terminal Dev update needs attention' "The update script failed: $_"
    "UPDATE-RESULT exception: $_"
}
finally {
    Stop-Transcript | Out-Null
}
