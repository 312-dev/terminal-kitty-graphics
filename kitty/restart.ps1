# Restarts Terminal Dev onto a pending build: closes every Terminal Dev
# window, installs the build update.ps1 left in C:\src\wt-dev\PENDING, and
# reopens the given dtach sessions as tabs in one window. Closing a window
# only detaches its sessions; they keep running in dtach.
#
# Start it detached (Start-Process), since it closes the window it may have
# been started from:
#   restart.ps1 -Sessions gray-23,scrolly-2
# Log: C:\src\wt-restart.log
param([string[]]$Sessions = @())
# Started with pwsh -File, "-Sessions a,b" arrives as the single string "a,b".
$Sessions = $Sessions -split ',' | ForEach-Object { $_.Trim() } | Where-Object { $_ }

Start-Transcript -Path 'C:\src\wt-restart.log' -Force | Out-Null
try {
    Start-Sleep -Seconds 2
    $location = (Get-AppxPackage WindowsTerminalDev).InstallLocation
    "closing Terminal Dev from $location"
    Get-Process WindowsTerminal -ErrorAction SilentlyContinue |
        Where-Object { $_.Path -and $_.Path.StartsWith($location, [StringComparison]::OrdinalIgnoreCase) } |
        Stop-Process -Force
    Start-Sleep -Seconds 2

    & 'C:\src\tools\pwsh\pwsh.exe' -NoProfile -ExecutionPolicy Bypass -File 'C:\src\wt\kitty\update.ps1' -InstallPending
    Get-Content 'C:\src\wt-install.log' | Select-String 'INSTALL-RESULT|still runs'
    "registered: $((Get-AppxPackage WindowsTerminalDev).Version) @ $((Get-AppxPackage WindowsTerminalDev).InstallLocation)"

    # One window, a tab per session. wt splits on every ';', so the attach
    # command has none.
    $tabs = foreach ($s in $Sessions) {
        "new-tab -p ""Ubuntu 24.04"" wsl.exe -d Ubuntu-24.04 -- bash -lc ""dtach -a /run/user/1000/claude-sessions/$s -E -z -r winch || true && exec bash -l"""
    }
    if (-not $tabs) { $tabs = @('new-tab -p "Ubuntu 24.04"') }
    $arguments = '-w new ' + ($tabs -join ' ; ')
    "wtd $arguments"
    Start-Process "$env:LOCALAPPDATA\Microsoft\WindowsApps\wtd.exe" -ArgumentList $arguments
    'RESTART-RESULT done'
}
catch {
    "RESTART-RESULT exception: $_"
}
finally {
    Stop-Transcript | Out-Null
}
