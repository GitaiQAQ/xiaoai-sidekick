param([string]$ProjectDir='C:\Users\gitai\Desktop\lx06-agent',
      [string]$CredentialsFile='', [switch]$TransportTest)
# Backwards-compatible text-only entry point.
& ([scriptblock]::Create((Get-Content -Raw -LiteralPath (Join-Path $ProjectDir 'tools\Invoke-DoubaoSmoke.ps1')))) @PSBoundParameters
