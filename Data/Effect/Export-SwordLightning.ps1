param([string]$EffekseerExe = "$PSScriptRoot/../../../Effekseer1.80.7Win/Tool/bin/Effekseer.exe")
$ErrorActionPreference = 'Stop'
$source = Join-Path $PSScriptRoot 'Sword_Lightning.efkproj'
[xml]$project = Get-Content -LiteralPath $source -Raw
$dependencies = $project.SelectNodes('//*[not(*)]') | Where-Object { $_.InnerText -match '\.(png|jpg|efkmodel|wav)$' } | ForEach-Object { $_.InnerText } | Sort-Object -Unique
foreach ($dependency in $dependencies) {
    if (!(Test-Path -LiteralPath (Join-Path $PSScriptRoot $dependency))) { throw "Missing effect dependency: $dependency. Export aborted to protect texture UV coordinates." }
}
if (!(Test-Path -LiteralPath $EffekseerExe)) { throw 'Provide the installed Effekseer.exe path with -EffekseerExe.' }
$output = Join-Path $PSScriptRoot 'Sword_Lightning.efkefc'
$process = Start-Process -FilePath $EffekseerExe -ArgumentList "-cui -in `"$source`" -o `"$output`"" -WorkingDirectory $PSScriptRoot -WindowStyle Hidden -Wait -PassThru
if ($process.ExitCode -ne 0 -or !(Test-Path -LiteralPath $output)) { throw 'Effekseer export failed.' }
Write-Output "Exported with all $($dependencies.Count) dependencies: $output"
