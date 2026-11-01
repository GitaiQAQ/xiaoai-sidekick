param([string]$ProjectDir='C:\Users\gitai\Desktop\lx06-agent')
$ErrorActionPreference='Stop'
$factory=Join-Path $ProjectDir 'temp\rootfs-factory-system0'
$patched=Join-Path $ProjectDir 'temp\rootfs-patched'
$index=@{}
foreach($side in @(@('Factory',$factory),@('Patched',$patched))){
 Get-ChildItem -LiteralPath $side[1] -Recurse -File | ForEach-Object {
  $relative=$_.FullName.Substring($side[1].Length+1).Replace('\','/')
  if(-not $index.ContainsKey($relative)){$index[$relative]=@{}}
  $index[$relative][$side[0]]=@{Bytes=$_.Length;Hash=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}
 }
}
$rows=foreach($relative in ($index.Keys | Sort-Object)){
 $f=$index[$relative]['Factory']; $p=$index[$relative]['Patched']
 $state=if(-not $f){'Added'}elseif(-not $p){'Removed'}elseif($f.Hash -ne $p.Hash){'Changed'}else{'Same'}
 [pscustomobject]@{Path=$relative;State=$state;FactoryBytes=$f.Bytes;PatchedBytes=$p.Bytes;FactorySHA256=$f.Hash;PatchedSHA256=$p.Hash}
}
$rows | Export-Csv -LiteralPath (Join-Path $ProjectDir 'temp\manifests\rootfs-file-manifest.csv') -NoTypeInformation -Encoding UTF8
$rows | Where-Object State -ne Same | Export-Csv -LiteralPath (Join-Path $ProjectDir 'temp\manifests\rootfs-differences.csv') -NoTypeInformation -Encoding UTF8
$rows | Group-Object State | Select-Object Name,Count
$rows | Where-Object State -ne Same | Select-Object Path,State,FactoryBytes,PatchedBytes
