$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$zig=Join-Path $root '.tools/ziglang/zig.exe'
Push-Location $root
try {
 New-Item -ItemType Directory -Force build-host | Out-Null
 foreach($name in @('rules','runtime','gacha','command','host','integration','chat_transport')) {
  if(-not (Test-Path "tests/${name}_test.cpp")){continue}
  $sources=@(Get-ChildItem src/growth,src/catalog,src/storage,src/gacha -Filter '*.cpp' -ErrorAction SilentlyContinue | ForEach-Object FullName)
  if($name -in @('host','integration','chat_transport')){$sources+=@('src/engine/patch_api.cpp')}
  if($name -eq 'integration'){$sources+=@('src/runtime/game.cpp','src/commands/chat.cpp')}
  if($name -eq 'chat_transport'){$sources+=@('src/commands/chat.cpp')}
  & $zig c++ -std=c++17 -O2 -Wall -Wextra -Werror -Isrc -Ivendor "tests/${name}_test.cpp" @sources -o "build-host/${name}_test.exe"
  if($LASTEXITCODE -ne 0){throw "$name compile failed"}
  & "./build-host/${name}_test.exe"
  if($LASTEXITCODE -ne 0){throw "$name test failed"}
 }
} finally {Pop-Location}
