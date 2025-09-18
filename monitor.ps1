<#
.SYNOPSIS
  Monitor for a specific serial port (default COM7) and auto-connect at 9600 baud.
.DESCRIPTION
  Continuously polls available serial ports; when the target port appears it opens
  an interactive terminal session (raw) at 9600 8N1. Keystrokes you type are sent;
  incoming data is printed to the console. Ctrl+C (or typing /exit and Enter) exits
  the active session and returns to monitoring; Ctrl+Break or closing the window ends the script.
.PARAMETER Port
  Serial port name (e.g. COM7). Default: COM7
.PARAMETER Baud
  Baud rate (default 9600)
.PARAMETER PollMs
  Poll interval while waiting (default 1000 ms)
.NOTES
  Requires Windows PowerShell 5+ (built-in) using System.IO.Ports.SerialPort.
#>
param(
  [string]$Port = 'COM7',
  [int]$Baud = 9600,
  [int]$PollMs = 1000,
  [switch]$Hex,
  [switch]$Timestamp,
  [switch]$NoDTR,
  [switch]$NoRTS,
  [int]$OpenDelayMs = 300,
  [int]$StatusEverySec = 10
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Write-Status($msg) {
  $ts = (Get-Date).ToString('HH:mm:ss')
  Write-Host "[$ts] $msg"
}

function Open-SerialPort($name, $baud) {
  $sp = New-Object System.IO.Ports.SerialPort $name, $baud, 'None', 8, 'One'
  $sp.ReadTimeout = 200
  $sp.WriteTimeout = 200
  if (-not $NoDTR) { $sp.DtrEnable = $true }
  if (-not $NoRTS) { $sp.RtsEnable = $true }
  $sp.Open()
  if ($OpenDelayMs -gt 0) { Start-Sleep -Milliseconds $OpenDelayMs }
  return $sp
}

function Monitor-Port {
  param([string]$Target, [int]$Baud, [int]$PollMs)
  while ($true) {
    if ([System.IO.Ports.SerialPort]::GetPortNames() -contains $Target) {
      Write-Status "Port $Target detected; opening..."
      try { return (Open-SerialPort -name $Target -baud $Baud) } catch {
        Write-Status "Open failed: $($_.Exception.Message). Retrying..."
      }
    }
    Start-Sleep -Milliseconds $PollMs
  }
}

function Run-Terminal {
  param([System.IO.Ports.SerialPort]$Serial)
  Write-Status "Connected to $($Serial.PortName) @ $($Serial.BaudRate) (Hex=$($Hex.IsPresent); Timestamp=$($Timestamp.IsPresent)). Press Ctrl+C to disconnect."
  Write-Host "------------------------------------------------------------"
  $totalBytes = 0
  $totalLines = 0
  $lineBuffer = ''
  $swStatus = [Diagnostics.Stopwatch]::StartNew()
  $enc = [System.Text.Encoding]::ASCII
  try {
    while ($Serial.IsOpen) {
      # READ PATH
      $available = $Serial.BytesToRead
      if ($available -gt 0) {
        if ($Hex) {
          $data = New-Object byte[] ($available)
          $read = $Serial.Read($data,0,$data.Length)
          if ($read -gt 0) {
            $totalBytes += $read
            $hexStr = ($data[0..($read-1)] | ForEach-Object { $_.ToString('X2') }) -join ' '
            if ($Timestamp) { $ts = (Get-Date).ToString('HH:mm:ss.fff'); Write-Host "[$ts] $hexStr" }
            else { Write-Host $hexStr }
          }
        } else {
          $chunk = $Serial.ReadExisting()
          if ($chunk.Length -gt 0) {
            $totalBytes += $chunk.Length
            $lineBuffer += $chunk
            while ($lineBuffer -match "`r?`n") {
              $idx = $lineBuffer.IndexOf("`n")
              if ($idx -lt 0) { break }
              $line = $lineBuffer.Substring(0,$idx).TrimEnd("`r")
              $lineBuffer = $lineBuffer.Substring($idx+1)
              $totalLines++
              if ($Timestamp) { $ts = (Get-Date).ToString('HH:mm:ss.fff'); Write-Host "[$ts] $line" }
              else { Write-Host $line }
            }
            # If not whole line yet and no timestamp, output partial without newline (live effect)
            if (-not $Timestamp -and ($lineBuffer.Length -gt 0) -and ($lineBuffer.Length -lt 200)) {
              Write-Host -NoNewline $lineBuffer
            }
          }
        }
      }

      # KEYBOARD PATH
      if ([Console]::KeyAvailable) {
        $key = [Console]::ReadKey($true)
        if ($key.Key -eq 'C' -and $key.Modifiers -band [ConsoleModifiers]::Control) {
          Write-Status 'Ctrl+C received; disconnecting...'
          break
        } elseif ($key.Key -eq 'Enter') {
          $Serial.Write("`r`n")
        } elseif ($key.Key -eq 'Backspace') {
          $Serial.Write("`b")
        } elseif ($key.KeyChar -ne 0) {
          $Serial.Write($key.KeyChar)
        }
      }

      # PERIODIC STATUS
      if ($StatusEverySec -gt 0 -and $swStatus.Elapsed.TotalSeconds -ge $StatusEverySec) {
        #Write-Status "Stats: bytes=$totalBytes lines=$totalLines (buffer=$($lineBuffer.Length))"
        $swStatus.Restart()
      }

      Start-Sleep -Milliseconds 5
    }
  } finally {
    if ($Serial.IsOpen) { $Serial.Close() }
    Write-Status "Disconnected from $($Serial.PortName). Total bytes=$totalBytes lines=$totalLines"
  }
}

while ($true) {
  $sp = Monitor-Port -Target $Port -Baud $Baud -PollMs $PollMs
  Run-Terminal -Serial $sp
  Write-Status "Reverting to monitoring (Ctrl+C again quickly to exit)."
  Start-Sleep -Milliseconds 500
}
