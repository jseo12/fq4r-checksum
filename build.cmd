@echo off
rem Keep build environment changes local; default to x64 for existing users.
rem 빌드 환경 변경을 로컬로 한정하고 기존 사용자를 위해 기본값은 x64로 유지합니다.
setlocal
cd /d "%~dp0"
set "fq4rArch=%~1"
if not defined fq4rArch set "fq4rArch=x64"
if not "%~2"=="" goto usage
if /i "%fq4rArch%"=="all" goto build_all
if /i "%fq4rArch%"=="x64" (
  set "fq4rArch=x64"
  set "fq4rOut=bin"
) else if /i "%fq4rArch%"=="x86" (
  set "fq4rArch=x86"
  set "fq4rOut=bin\x86"
) else goto usage

rem Always select the requested MSVC target, even if another cl is on PATH.
rem PATH에 다른 cl이 있어도 요청한 MSVC 대상 아키텍처를 명시적으로 선택합니다.
set "fq4rVs=%VSINSTALLDIR%"
if not defined fq4rVs (
  for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "fq4rVs=%%i"
)
if not defined fq4rVs (
  echo ERROR: Visual Studio C++ desktop build tools were not found.
  exit /b 1
)
call "%fq4rVs%\VC\Auxiliary\Build\vcvarsall.bat" %fq4rArch%
if errorlevel 1 exit /b 1
where cl >nul 2>nul
if errorlevel 1 exit /b 1

rem Isolate x86 outputs so that the existing x64 binary is not overwritten.
rem 기존 x64 실행 파일을 덮어쓰지 않도록 x86 산출물을 분리합니다.
if not exist "%fq4rOut%" mkdir "%fq4rOut%"
if errorlevel 1 exit /b 1
rem Optimize for size and statically link the release CRT for either target.
rem 두 대상 모두 크기 최적화와 배포용 CRT 정적 링크를 적용합니다.
cl /nologo /TC /W4 /WX /O1 /MT /utf-8 /D_CRT_SECURE_NO_WARNINGS main.c fq4r_checksum.c /Fe:"%fq4rOut%\fq4r-checksum.exe" /Fo:%fq4rOut%\ /link /MACHINE:%fq4rArch% /INCREMENTAL:NO /OPT:REF /OPT:ICF
if errorlevel 1 exit /b 1
rem Package only successful builds; propagate packaging failures to automation.
rem 성공한 빌드만 패키징하고 패키징 실패도 자동화 호출자에게 전달합니다.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0package.ps1" -Architecture %fq4rArch%
exit /b %errorlevel%

:build_all
rem Build and package both architectures, stopping on the first failure.
rem 두 아키텍처를 빌드하고 패키징하며 첫 실패에서 중단합니다.
call "%~f0" x64
if errorlevel 1 exit /b 1
call "%~f0" x86
exit /b %errorlevel%

:usage
echo Usage: build.cmd [x64^|x86^|all]
exit /b 2
