param([string]$Version = "1.0.0")

$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
Set-Location $root

$msbuild = "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
if (-not (Test-Path $msbuild)) {
    $msbuild = "MSBuild"
}

& $msbuild APC.vcxproj /t:Rebuild /p:Configuration=Release /p:Platform=x64 /m /v:m
if ($LASTEXITCODE -ne 0) {
    throw "Build failed with exit code $LASTEXITCODE."
}

$name = "APC-v$Version"
$dist = Join-Path $root "dist"
$stage = Join-Path $dist $name
$zip = Join-Path $dist "$name.zip"
$checksums = Join-Path $dist "SHA256SUMS.txt"

New-Item -ItemType Directory -Path $dist -Force | Out-Null
if (Test-Path $stage) {
    Remove-Item -LiteralPath $stage -Recurse -Force
}
New-Item -ItemType Directory -Path $stage | Out-Null

$release = Join-Path $root "bin\x64\Release"
Copy-Item (Join-Path $release "APC.exe") $stage
Copy-Item (Join-Path $release "settings.ini") $stage
Copy-Item (Join-Path $root "LICENSE") $stage
Copy-Item (Join-Path $root "CHANGELOG.md") $stage
Copy-Item (Join-Path $root "README.md") $stage
Copy-Item (Join-Path $root "README") (Join-Path $stage "README") -Recurse
Copy-Item (Join-Path $root "SPECIFICATION") (Join-Path $stage "SPECIFICATION") -Recurse
Copy-Item (Join-Path $root "docs") (Join-Path $stage "docs") -Recurse

if (Test-Path $zip) {
    Remove-Item -LiteralPath $zip -Force
}
Compress-Archive -Path $stage -DestinationPath $zip
$hash = (Get-FileHash $zip -Algorithm SHA256).Hash.ToLowerInvariant()
"$hash  $name.zip" | Set-Content $checksums -Encoding ASCII

Write-Host "Created $zip"
Write-Host "SHA256: $hash"
