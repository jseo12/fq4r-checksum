@echo off
rem Keep environment changes local to this build invocation.
rem 환경 변수 변경은 이 빌드 실행 안에서만 유지합니다.
setlocal
cd /d "%~dp0"
rem If no compiler is active, find MSVC through vswhere and initialize its x64 environment.
rem 현재 컴파일러가 없으면 vswhere로 MSVC를 찾아 x64 빌드 환경을 초기화합니다.
where cl >nul 2>nul
if errorlevel 1 (
  for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%i\VC\Auxiliary\Build\vcvars64.bat"
)
where cl >nul 2>nul
if errorlevel 1 exit /b 1
rem Keep the executable and intermediate artifacts in the bin directory.
rem 실행 파일과 중간 산출물을 bin 폴더에 모읍니다.
if not exist bin mkdir bin
rem Compile as C, treat warnings as errors, optimize for size, and link the CRT statically.
rem C로 컴파일하고 경고를 오류로 처리합니다. 작은 코드와 정적 CRT를 사용합니다.
cl /nologo /TC /W4 /WX /O1 /MT /utf-8 /D_CRT_SECURE_NO_WARNINGS main.c fq4r_checksum.c /Fe:bin\fq4r-checksum.exe /Fo:bin\ /link /INCREMENTAL:NO /OPT:REF /OPT:ICF
rem Propagate the compiler exit code to the caller.
rem 컴파일러 종료 코드를 호출자에게 전달합니다.
exit /b %errorlevel%
