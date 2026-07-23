param(
    [ValidateSet("arm64-v8a", "x86_64", "all")]
    [string]$Abi = "all",

    [string]$AndroidNdkHome = "C:\Users\admin\AppData\Local\Android\Sdk\ndk\26.1.10909125",
    [string]$CMakeExe = "C:\Users\admin\AppData\Local\Android\Sdk\cmake\4.1.2\bin\cmake.exe",
    [string]$VcpkgRoot = ".codex_tmp\vcpkg",
    [string]$OutputDir = "output\phase7_android_real_native",

    [switch]$CopyToAndroidProject
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$vcpkgExe = Join-Path $repoRoot "$VcpkgRoot\vcpkg.exe"
$vcpkgToolchain = Join-Path $repoRoot "$VcpkgRoot\scripts\buildsystems\vcpkg.cmake"
$androidToolchain = Join-Path $AndroidNdkHome "build\cmake\android.toolchain.cmake"

if (!(Test-Path $vcpkgExe)) {
    throw "vcpkg.exe not found: $vcpkgExe"
}
if (!(Test-Path $CMakeExe)) {
    throw "cmake.exe not found: $CMakeExe"
}
if (!(Test-Path $androidToolchain)) {
    throw "Android NDK toolchain not found: $androidToolchain"
}

$env:ANDROID_NDK_HOME = $AndroidNdkHome
$env:ANDROID_NDK_ROOT = $AndroidNdkHome
$env:VCPKG_DISABLE_METRICS = "1"

$targets = @()
if ($Abi -eq "all" -or $Abi -eq "arm64-v8a") {
    $targets += @{ Abi = "arm64-v8a"; Triplet = "arm64-android"; Build = ".codex_tmp\phase7_android_real_arm64" }
}
if ($Abi -eq "all" -or $Abi -eq "x86_64") {
    $targets += @{ Abi = "x86_64"; Triplet = "x64-android"; Build = ".codex_tmp\phase7_android_real_x86_64" }
}

foreach ($target in $targets) {
    $targetAbi = $target.Abi
    $targetTriplet = $target.Triplet
    $targetBuild = $target.Build
    Push-Location $repoRoot
    try {
        & $vcpkgExe install --triplet $targetTriplet --host-triplet x64-mingw-dynamic
        if ($LASTEXITCODE -ne 0) {
            throw "vcpkg install failed for $targetTriplet"
        }

        & $CMakeExe -S . -B $targetBuild -G Ninja `
            "-DCMAKE_TOOLCHAIN_FILE=$vcpkgToolchain" `
            "-DVCPKG_CHAINLOAD_TOOLCHAIN_FILE=$androidToolchain" `
            "-DVCPKG_TARGET_TRIPLET=$targetTriplet" `
            "-DVCPKG_HOST_TRIPLET=x64-mingw-dynamic" `
            "-DANDROID_ABI=$targetAbi" `
            "-DANDROID_PLATFORM=android-28" `
            "-DHONTA_ENABLE_MOCK_DETECTION=OFF"
        if ($LASTEXITCODE -ne 0) {
            throw "CMake configure failed for $targetAbi"
        }

        & $CMakeExe --build $targetBuild --target honta_native
        if ($LASTEXITCODE -ne 0) {
            throw "CMake build failed for $targetAbi"
        }

        $builtSo = Join-Path $targetBuild "libhonta_native.so"
        $destDir = Join-Path $OutputDir $targetAbi
        New-Item -ItemType Directory -Force -Path $destDir | Out-Null
        Copy-Item -Force $builtSo (Join-Path $destDir "libhonta_native.so")

        if ($CopyToAndroidProject) {
            $jniDir = Join-Path "android\app\src\main\jniLibs" $targetAbi
            New-Item -ItemType Directory -Force -Path $jniDir | Out-Null
            Copy-Item -Force $builtSo (Join-Path $jniDir "libhonta_native.so")
        }
    }
    finally {
        Pop-Location
    }
}
