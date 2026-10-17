# Package only the executable, license, and three READMEs after a successful build.
# 빌드 성공 후 실행 파일, 라이선스, 세 언어 README만 패키징합니다.
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('x86', 'x64')]
    [string]$Architecture
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$Architecture = $Architecture.ToLowerInvariant()

# Keep versioning in one file; reject characters that could escape the output directory.
# 버전은 파일 하나로 관리하며 출력 폴더를 벗어날 수 있는 문자는 거부합니다.
$releaseVersion = (Get-Content -LiteralPath (Join-Path $PSScriptRoot 'VERSION') -Raw).Trim()
if ($releaseVersion -notmatch '^\d+\.\d+\.\d+(?:-[0-9A-Za-z.-]+)?$') {
    throw 'VERSION must contain a version such as 0.1.0 or 0.2.0-beta.1.'
}

$binaryRelativePath = if ($Architecture -eq 'x86') { 'bin\x86\fq4r-checksum.exe' } else { 'bin\fq4r-checksum.exe' }
$binaryPath = Join-Path $PSScriptRoot $binaryRelativePath
$packageEntries = @(
    @{ Source = $binaryPath; Name = 'fq4r-checksum.exe' }
    @{ Source = (Join-Path $PSScriptRoot 'LICENSE'); Name = 'LICENSE' }
    @{ Source = (Join-Path $PSScriptRoot 'README.md'); Name = 'README.md' }
    @{ Source = (Join-Path $PSScriptRoot 'README.ko.md'); Name = 'README.ko.md' }
    @{ Source = (Join-Path $PSScriptRoot 'README.ja.md'); Name = 'README.ja.md' }
)
foreach ($entry in $packageEntries) {
    if (-not (Test-Path -LiteralPath $entry.Source -PathType Leaf)) {
        throw "Required package file is missing: $($entry.Source)"
    }
}

# Check the PE machine field to prevent distributing a binary under the wrong architecture name.
# 잘못된 아키텍처 이름으로 배포하지 않도록 PE machine 필드를 확인합니다.
$binaryBytes = [System.IO.File]::ReadAllBytes($binaryPath)
if ($binaryBytes.Length -lt 64 -or $binaryBytes[0] -ne 0x4D -or $binaryBytes[1] -ne 0x5A) {
    throw 'The executable does not have a valid DOS header.'
}
$peOffset = [BitConverter]::ToInt32($binaryBytes, 0x3C)
if ($peOffset -lt 0 -or $peOffset -gt $binaryBytes.Length - 6 -or
    [BitConverter]::ToUInt32($binaryBytes, $peOffset) -ne 0x00004550) {
    throw 'The executable does not have a valid PE header.'
}
$expectedMachine = if ($Architecture -eq 'x86') { 0x014C } else { 0x8664 }
if ([BitConverter]::ToUInt16($binaryBytes, $peOffset + 4) -ne $expectedMachine) {
    throw "The executable architecture does not match $Architecture."
}

$outputDirectory = Join-Path $PSScriptRoot 'dist'
[System.IO.Directory]::CreateDirectory($outputDirectory) | Out-Null
$archivePath = Join-Path $outputDirectory "fq4r-checksum-v$releaseVersion-windows-$Architecture.zip"
$temporaryArchive = Join-Path $outputDirectory ('.package-' + [Guid]::NewGuid().ToString('N') + '.tmp')
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

# Build a fresh archive with an explicit allowlist; never include samples or intermediate files.
# 명시적 허용 목록으로 새 압축 파일을 만들며 샘플과 중간 산출물은 포함하지 않습니다.
$archive = $null
try {
    $archive = [System.IO.Compression.ZipFile]::Open($temporaryArchive, [System.IO.Compression.ZipArchiveMode]::Create)
    foreach ($entry in $packageEntries) {
        [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile(
            $archive, $entry.Source, $entry.Name, [System.IO.Compression.CompressionLevel]::Optimal) | Out-Null
    }
    $archive.Dispose()
    $archive = $null

    # Replace an earlier package only after the new ZIP has been completed successfully.
    # 새 ZIP을 성공적으로 완성한 뒤에만 이전 패키지를 교체합니다.
    Move-Item -LiteralPath $temporaryArchive -Destination $archivePath -Force
    Write-Output "Package: $archivePath"
} finally {
    if ($null -ne $archive) { $archive.Dispose() }
    if (Test-Path -LiteralPath $temporaryArchive) {
        Remove-Item -LiteralPath $temporaryArchive
    }
}
