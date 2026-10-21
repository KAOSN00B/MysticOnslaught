param([Parameter(Mandatory=$true)][string]$PackageDirectory)
$ErrorActionPreference = 'Stop'
$packageRoot = (Resolve-Path -LiteralPath $PackageDirectory).Path
$projectRoot = Split-Path $PSScriptRoot -Parent
$verificationRoot = Join-Path $projectRoot ('x64\ShippingSmoke\' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
New-Item -ItemType Directory -Path $verificationRoot | Out-Null
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class ShippingSmokeWindow {
    public delegate bool WindowVisitor(IntPtr window, IntPtr state);
    [DllImport("user32.dll")] public static extern bool EnumWindows(WindowVisitor visitor, IntPtr state);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr window, out uint process);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr window, uint message, IntPtr wParam, IntPtr lParam);
    public static void Close(uint process) {
        EnumWindows((window, state) => {
            uint owner; GetWindowThreadProcessId(window, out owner);
            if (owner == process) PostMessage(window, 0x0010, IntPtr.Zero, IntPtr.Zero);
            return true;
        }, IntPtr.Zero);
    }
}
'@
$previousLocalData = $env:LOCALAPPDATA
$gameProcess = $null
try {
    $env:LOCALAPPDATA = Join-Path $verificationRoot 'Profile'
    $gameProcess = Start-Process -FilePath (Join-Path $packageRoot 'MysticOnslaught.exe') -WorkingDirectory $env:TEMP -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $verificationRoot 'startup.log') -RedirectStandardError (Join-Path $verificationRoot 'stderr.log')
    $null = $gameProcess.Handle # Retain the native handle so PowerShell can read ExitCode after exit.
    Start-Sleep -Seconds 12
    $gameProcess.Refresh()
    if ($gameProcess.HasExited) { throw "Game exited during startup: $($gameProcess.ExitCode)" }
    [ShippingSmokeWindow]::Close([uint32]$gameProcess.Id)
    if (!$gameProcess.WaitForExit(15000)) { throw 'Game did not exit after WM_CLOSE.' }
    if ($gameProcess.ExitCode -ne 0) { throw "Game shutdown failed: $($gameProcess.ExitCode)" }
    $diagnostics = @(Get-Content (Join-Path $verificationRoot 'startup.log') | Select-String 'WARNING|ERROR')
    $stderr = Get-Content (Join-Path $verificationRoot 'stderr.log') -Raw
    # Every startup warning or error blocks this check. (The seven legacy arena
    # props that were never in the art pack stopped loading on 2026-10-19.)
    $unexpected = @($diagnostics)
    $diagnostics | Write-Output
    if ($unexpected.Count -gt 0 -or ![string]::IsNullOrWhiteSpace($stderr)) {
        throw "Unexpected startup diagnostics. Inspect $verificationRoot"
    }
    Write-Output "PASS: packaged startup and clean shutdown, no warnings. Logs: $verificationRoot"
} finally {
    $env:LOCALAPPDATA = $previousLocalData
    if ($gameProcess -and !$gameProcess.HasExited) { Stop-Process -Id $gameProcess.Id }
}
