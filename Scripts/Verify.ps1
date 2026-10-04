$ErrorActionPreference = 'Stop'
$factoryRoot = Split-Path -Parent $PSScriptRoot
Push-Location -LiteralPath $factoryRoot
try {
    cmake --preset windows
    if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed' }
    foreach ($factoryConfiguration in @('debug', 'release')) {
        cmake --build --preset $factoryConfiguration
        if ($LASTEXITCODE -ne 0) { throw "$factoryConfiguration build failed" }
        ctest --preset $factoryConfiguration
        if ($LASTEXITCODE -ne 0) { throw "$factoryConfiguration tests failed" }
    }
    cmake --install Build/windows --config Release --prefix Install
    if ($LASTEXITCODE -ne 0) { throw 'Installation failed' }
    & .\Install\bin\FactoryCoreRuntime.exe --demo
    if ($LASTEXITCODE -ne 0) { throw 'Installed runtime demonstration failed' }
    & .\Install\bin\FactoryCoreAuthor.exe inspect .\Install\share\FactoryCore\Examples\CylinderCell.factory
    if ($LASTEXITCODE -ne 0) { throw 'Installed authoring tool failed' }
    cpack --config Build/windows/CPackConfig.cmake -C Release -B Build/Packages
    if ($LASTEXITCODE -ne 0) { throw 'Packaging failed' }
}
finally {
    Pop-Location
}
