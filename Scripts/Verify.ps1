param(
    [switch]$CoreOnly,
    [switch]$GpuTests
)
$ErrorActionPreference = 'Stop'
$factoryRoot = Split-Path -Parent $PSScriptRoot
Push-Location -LiteralPath $factoryRoot
try {
    $factoryPreset = if ($CoreOnly) { 'windows' } else { 'editor' }
    $factoryDirectory = if ($CoreOnly) { 'Build/windows' } else { 'Build/editor' }
    $factoryGpuFlag = if ($GpuTests) { 'ON' } else { 'OFF' }
    if ($CoreOnly) { cmake --preset $factoryPreset } else { cmake --preset $factoryPreset "-DFACTORYCORE_GPU_TESTS=$factoryGpuFlag" }
    if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed' }
    foreach ($factoryConfiguration in @('debug', 'release')) {
        $factoryBuildPreset = if ($CoreOnly) { $factoryConfiguration } else { "editor-$factoryConfiguration" }
        cmake --build --preset $factoryBuildPreset --parallel 6
        if ($LASTEXITCODE -ne 0) { throw "$factoryConfiguration build failed" }
        ctest --preset $factoryBuildPreset
        if ($LASTEXITCODE -ne 0) { throw "$factoryConfiguration tests failed" }
    }
    cmake --install $factoryDirectory --config Release --prefix Install
    if ($LASTEXITCODE -ne 0) { throw 'Installation failed' }
    & .\Install\bin\FactoryCoreRuntime.exe --demo
    if ($LASTEXITCODE -ne 0) { throw 'Installed runtime demonstration failed' }
    & .\Install\bin\FactoryCoreAuthor.exe inspect .\Install\share\FactoryCore\Examples\CylinderCell.factory
    if ($LASTEXITCODE -ne 0) { throw 'Installed authoring tool failed' }
    if (!$CoreOnly) {
        & .\Install\bin\FactoryCoreEditor.exe --help
        if ($LASTEXITCODE -ne 0) { throw 'Installed editor failed' }
        if ($GpuTests) {
            & .\Install\bin\FactoryCoreEditor.exe --smoke --capture Build/Testing/Installed-Editor.png
            if ($LASTEXITCODE -ne 0) { throw 'Installed renderer failed' }
            & .\Install\bin\FactoryCoreEditor.exe --cell-test --capture Build/Testing/Installed-Cell/Editor-Complete.png
            if ($LASTEXITCODE -ne 0) { throw 'Installed product cell test failed' }
        }
    }
    $factoryPackageDirectory = if ($CoreOnly) { "Build/Packages/Core" } else { "Build/Packages" }
    cpack --config "$factoryDirectory/CPackConfig.cmake" -C Release -B $factoryPackageDirectory
    if ($LASTEXITCODE -ne 0) { throw 'Packaging failed' }
}
finally {
    Pop-Location
}
