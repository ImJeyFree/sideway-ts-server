param(
    [Parameter(Mandatory=$true)][string]$SdkDirectory,
    [Parameter(Mandatory=$true)][string]$ServerExe,
    [string]$Version='1.2.0',
    [string]$OutputDirectory
)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
if(-not $OutputDirectory){$OutputDirectory=Join-Path $root ('dist/'+$Version)}
$target=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path -LiteralPath $target){throw '기존 배포 폴더를 덮어쓰지 않습니다. 새 출력 경로를 지정하세요.'}
foreach($f in @($ServerExe,(Join-Path $SdkDirectory 'bin/SidewayTunerCore.dll'),(Join-Path $SdkDirectory 'lib/SidewayTunerCore.lib'))){if(-not(Test-Path -LiteralPath $f -PathType Leaf)){throw "필수 배포 파일 누락: $f"}}
$publicFiles=@(
 '.gitignore','CMakeLists.txt','CMakePresets.json','README.md','LICENSE','NOTICE','BINARY_LICENSE.md','THIRD_PARTY_NOTICES.md',
 'licenses/nlohmann-json-MIT.txt',
 'include/Common.h','include/TsBroadcaster.h','include/TunerClient.h','include/ScanClient.h','include/ChannelStore.h','include/HttpStreamer.h','include/UdpStreamer.h','include/TrayApp.h','include/WebDashboard.h','include/third_party/json.hpp',
 'src/main.cpp','src/TrayApp.cpp','src/HttpStreamer.cpp','src/UdpStreamer.cpp','src/TsBroadcaster.cpp','src/TunerClient.cpp','src/ChannelStore.cpp',
 'sdk/TunerCoreApi.h','tests/StreamTests.cpp','tests/DashboardTests.js','tests/MockTunerCore.cpp','tests/LoaderTests.cpp',
 'docs/DLL_API.md','docs/EPG_API.md','docs/공개_배포_범위.md','docs/변경_기록.md','tools/Prepare-PublicRelease.ps1'
)
function Copy-Relative([string]$base,[string]$relative,[string]$destination){
 $from=Join-Path $base $relative;$to=Join-Path $destination $relative
 if(-not(Test-Path -LiteralPath $from -PathType Leaf)){throw "허용 목록 파일 누락: $relative"}
 New-Item -ItemType Directory -Path (Split-Path $to -Parent) -Force | Out-Null
 Copy-Item -LiteralPath $from -Destination $to
}
$source=Join-Path $target 'source';$runtime=Join-Path $target 'runtime';$sdk=Join-Path $target 'sdk'
New-Item -ItemType Directory -Path $source,$runtime,$sdk | Out-Null
foreach($f in $publicFiles){Copy-Relative $root $f $source}
foreach($folder in @($runtime,$sdk)){
 foreach($f in @('LICENSE','NOTICE','BINARY_LICENSE.md','THIRD_PARTY_NOTICES.md','licenses/nlohmann-json-MIT.txt','README.md','docs/DLL_API.md','docs/EPG_API.md','docs/공개_배포_범위.md','docs/변경_기록.md')){Copy-Relative $root $f $folder}
}
Copy-Item -LiteralPath $ServerExe -Destination (Join-Path $runtime 'sideway-ts-server.exe')
Copy-Item -LiteralPath (Join-Path $SdkDirectory 'bin/SidewayTunerCore.dll') -Destination $runtime
Copy-Relative $SdkDirectory 'bin/SidewayTunerCore.dll' $sdk
Copy-Relative $SdkDirectory 'lib/SidewayTunerCore.lib' $sdk
Copy-Relative $root 'sdk/TunerCoreApi.h' $sdk
foreach($folder in @($source,$runtime,$sdk)){
 $manifest=Get-ChildItem -LiteralPath $folder -File -Recurse | Sort-Object FullName | ForEach-Object {
  [pscustomobject]@{path=[IO.Path]::GetRelativePath($folder,$_.FullName).Replace('\','/');sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()}
 }
 ConvertTo-Json -InputObject @($manifest) -Depth 3 | Set-Content -LiteralPath (Join-Path $folder 'SHA256.json') -Encoding utf8
 Compress-Archive -Path (Join-Path $folder '*') -DestinationPath (Join-Path $target ((Split-Path $folder -Leaf)+'-'+$Version+'.zip'))
}
$zipHashes=Get-ChildItem -LiteralPath $target -Filter '*.zip' | ForEach-Object {
 [pscustomobject]@{file=$_.Name;sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()}
}
ConvertTo-Json -InputObject @($zipHashes) | Set-Content -LiteralPath (Join-Path $target 'SHA256.json') -Encoding utf8
$zipHashes | Format-Table -AutoSize
Write-Output "로컬 배포 패키지 준비 완료: $target (commit/push/게시 없음)"
