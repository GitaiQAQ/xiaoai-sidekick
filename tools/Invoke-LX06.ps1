param([Parameter(Mandatory=$true)][string]$Command,[string]$OutputFile,[switch]$Binary)
$ErrorActionPreference='Stop'
$projectDir=Split-Path -Parent $PSScriptRoot
$env:SSH_ASKPASS=Join-Path $projectDir 'tools\local\ssh-askpass.cmd'
$env:SSH_ASKPASS_REQUIRE='force'
$env:DISPLAY='codex'
$info=New-Object Diagnostics.ProcessStartInfo
$info.FileName='C:\Windows\System32\OpenSSH\ssh.exe'
$info.UseShellExecute=$false
$info.CreateNoWindow=$true
$info.RedirectStandardOutput=$true
$info.RedirectStandardError=$true
# Remote command is supplied through stdin to sh, avoiding shell quoting in command arguments.
$info.RedirectStandardInput=$true
$info.Arguments='-T -o HostKeyAlgorithms=+ssh-rsa -o UserKnownHostsFile=C:/Users/gitai/Desktop/lx06-agent/tools/local/known_hosts -o ConnectTimeout=8 -o NumberOfPasswordPrompts=1 root@192.168.100.210 sh'
$process=New-Object Diagnostics.Process
$process.StartInfo=$info
[void]$process.Start()
$errors=$process.StandardError.ReadToEndAsync()
$process.StandardInput.Write($Command.Replace("`r",'') + "`n")
$process.StandardInput.Close()
if($Binary){
 if(-not $OutputFile){throw 'Binary requires OutputFile'}
 $stream=[IO.File]::Create($OutputFile)
 try{$process.StandardOutput.BaseStream.CopyTo($stream)}finally{$stream.Dispose()}
}else{
 $output=$process.StandardOutput.ReadToEnd()
 if($OutputFile){[IO.File]::WriteAllText($OutputFile,$output,(New-Object Text.UTF8Encoding($false)))}else{Write-Output $output}
}
$process.WaitForExit()
$stderr=$errors.Result
if($process.ExitCode -ne 0){throw "SSH exited $($process.ExitCode): $stderr"}

