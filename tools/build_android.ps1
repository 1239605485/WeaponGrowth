param([string]$Ndk='C:/orbuild/toolchain/android-ndk-r26c',[string]$Cmake='C:/Users/liuxin/AppData/Local/Android/Sdk/cmake/4.1.2/bin/cmake.exe')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
& $Cmake -S $root -B "$root/build-android" -G Ninja "-DCMAKE_MAKE_PROGRAM=$(Split-Path $Cmake)/ninja.exe" "-DCMAKE_TOOLCHAIN_FILE=$Ndk/build/cmake/android.toolchain.cmake" -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-24 -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
if($LASTEXITCODE -ne 0){throw 'Configure failed'}
& $Cmake --build "$root/build-android" --parallel 4
if($LASTEXITCODE -ne 0){throw 'Build failed'}
