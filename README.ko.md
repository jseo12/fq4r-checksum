# FQ4R 세이브 체크섬 CLI 도구

[English](README.md) | **한국어** | [日本語](README.ja.md)

퍼스트 퀸 4 리메이크 Windows판의 세이브 파일 체크섬을 검사하고 수정하는 도구입니다. 이 프로젝트에서는 해당 게임을 **FQ4R**로 부릅니다. 공식 일본어 제품명은 「ファーストクイーンⅣ」입니다. [공식 사이트](http://kure.sakura.ne.jp/fq4/)

저장소 이름은 `fq4r-checksum`, 실행 파일은 `fq4r-checksum.exe`입니다. Windows x86·x64용 C 프로그램으로, Windows 기본 DLL과 정적으로 링크한 C 런타임을 사용합니다. 브라우저 엔진, Node.js, Python 또는 별도 VC 런타임 설치 없이 실행합니다. Python은 테스트할 때만 필요합니다.

## 사용법

다운로드한 EXE가 있는 폴더에서 실행합니다. 소스로 빌드했다면 EXE는 `bin` 폴더에 생성됩니다.

```powershell
# 파일 하나: 파일명 제한 없음
.\fq4r-checksum.exe "C:\Games\FQ4R\custom.dat"

# 폴더: save00.dat~save99.dat 검사
.\fq4r-checksum.exe "C:\Games\FQ4R"

# 백업 후 체크섬 수정
.\fq4r-checksum.exe --fix "C:\Games\FQ4R"

# 언어 선택과 도움말
.\fq4r-checksum.exe --lang ko "C:\Games\FQ4R\save00.dat"
.\fq4r-checksum.exe --help
```

| 인자·옵션 | 동작 |
|---|---|
| 파일 또는 폴더 | 경로 하나 필수 |
| `-f`, `--fix` | 체크섬이 잘못된 파일만 백업 후 제자리 수정 |
| `-l`, `--lang ko\|en\|ja\|auto` | 출력 언어 선택. `--lang=ko`도 지원 |
| `-h`, `--help` | 사용법과 언어 선택 안내 |
| `--` | 이후 인자를 옵션으로 해석하지 않음 |

폴더에서는 대소문자를 구분하지 않고 숫자 정확히 두 자리인 `saveNN.dat`를 이름순으로 검사합니다. 하위 폴더는 검색하지 않습니다. 파일 경로를 직접 지정하면 이름은 제한하지 않습니다.

모든 파일에서 크기 **32768바이트**와 signature **`win4` 또는 `Win4`**를 확인합니다. `WIN4`는 허용하지 않습니다. 형식·입출력 오류는 표시하고 다음 파일로 넘어갑니다. 잘못된 형식은 수정하지 않습니다. 각 파일의 8개 블록에 대해 범위, 저장값, 계산값과 일치 여부를 출력합니다.

## 언어와 종료 코드

기본값 `auto`는 Windows 사용자 표시 언어를 기준으로 한국어·영어·일본어를 선택합니다. 미지원 언어는 한국어를 사용합니다. 지역 형식이나 콘솔 코드페이지와는 별개입니다. 명시적 언어 옵션이 자동 선택보다 우선하고, 여러 번 지정하면 마지막 값이 적용됩니다. 언어 코드는 대소문자를 구분하지 않으며 설정을 저장하지 않습니다.

인자가 없으면 언어 선택 안내를 포함한 도움말과 오류를 표시합니다. `--help --lang ja`처럼 도움말 뒤에 언어를 지정해도 적용됩니다. 출력은 UTF-8이며 Windows 오류 설명의 해당 언어 리소스가 없으면 일반 오류 설명과 오류 번호를 표시합니다.

| 종료 코드 | 의미 |
|---|---|
| `0` | 모두 정상 또는 수정 완료, 혹은 도움말 표시 |
| `1` | 체크섬 불일치가 남음 |
| `2` | 형식·입출력·인자 오류 또는 대상 없음 |

`2`가 `1`보다 우선합니다. 요약에는 검사 수, 원래 정상인 수, 수정 완료 수, 검사 모드의 불일치 수, 형식 오류 수, 그 밖의 오류 수를 표시합니다. 검색 오류는 파일 수와 별도로 집계될 수 있습니다.

## 수정과 백업

원본 전체를 `.bak`에 복사하고, 이미 있으면 `.bak.1`, `.bak.2` 순서로 기존 백업을 보존합니다. 백업 내용이 검사 당시 입력과 같은지 확인합니다. 백업 실패 시 원본을 수정하지 않습니다. 정상 파일은 변경하거나 백업하지 않습니다.

수정 결과를 같은 폴더의 고유 임시 파일에 기록하고 flush한 뒤 `ReplaceFileW`로 교체합니다. 수정 범위는 **`0x7DA6~0x7DAD`의 8바이트**입니다. 실패 시 생성된 백업은 보존합니다. 다른 프로그램의 동시 저장·교체를 완전히 조정하지 않으므로 게임을 종료하고 수정하십시오.

체크섬 정상은 게임 데이터의 의미적 유효성이나 게임 내 로드 성공을 보장하지 않습니다. 개발 당시 한글 패치판 세이브 8개를 검사했습니다. 모든 언어판·업데이트·Windows 버전에서의 동작을 검증한 것은 아닙니다.

## 빌드와 테스트

Visual Studio의 C++ 데스크톱 빌드 도구와 Windows SDK가 필요합니다.

```powershell
.\build.cmd
.\bin\fq4r-checksum.exe --help
python .\test_cli.py
```

`build.cmd`는 현재 Visual Studio 설치 또는 `vswhere`로 찾은 설치를 사용하여 요청한 대상 환경을 설정합니다. 크기 최적화, 정적 CRT와 경고 오류 처리를 적용합니다. 일반 MSVC 환경을 직접 설정한다면 x64 Native Tools 명령 프롬프트를 사용하십시오.

테스트는 합성 세이브와 독립적인 Python XOR 구현을 사용하므로 게임 파일 없이 실행할 수 있습니다. 모든 블록 변조, 체크섬 영역 변조, 백업 보존, 반복 수정, 잘못된 형식, 이름 필터, Unicode·긴 경로, 읽기 전용 파일과 세 언어를 검증합니다. 수정 테스트는 임시 복사본만 사용합니다. 기존 분석 작업 공간의 원본 세이브 검사는 해당 파일이 없으면 건너뜁니다.

## 아키텍처별 빌드

```powershell
.\build.cmd x64
.\build.cmd x86

# x86 실행 파일로 통합 테스트
$env:FQ4R_CHECK_EXE = "$PWD\bin\x86\fq4r-checksum.exe"
python .\test_cli.py
Remove-Item Env:FQ4R_CHECK_EXE
```

- x64: `bin/fq4r-checksum.exe`. 인자 없이 빌드할 때도 기본값입니다.
- x86: `bin/x86/fq4r-checksum.exe`. 32비트 Windows 사용자용입니다.

요청한 아키텍처를 명시적으로 선택하고 산출물을 분리합니다. 동일한 C 소스와 배포용 설정을 사용합니다. x86 실행 파일은 현재 64비트 Windows 환경에서 검증했으며, 실제 32비트 Windows 설치 환경에서는 아직 검증하지 않았습니다. 구형 Windows 버전의 호환성을 보장한다는 뜻은 아닙니다.

## 릴리스 패키지

빌드 성공 시 `dist/`에 배포용 ZIP을 자동 생성합니다.

```powershell
.\build.cmd       # x64 빌드와 ZIP 생성
.\build.cmd x86   # x86 빌드와 ZIP 생성
.\build.cmd all   # 두 아키텍처 빌드와 ZIP 생성
```

버전은 `VERSION` 파일에서 읽습니다(현재 `0.1.0`). 결과는 `fq4r-checksum-v0.1.0-windows-x64.zip`, `fq4r-checksum-v0.1.0-windows-x86.zip`입니다. 각 ZIP 최상위에는 `fq4r-checksum.exe`, `LICENSE`, `README.md`, `README.ko.md`, `README.ja.md`의 5개 파일만 포함합니다. 샘플과 중간 산출물은 제외합니다. 다시 빌드하면 새 ZIP이 완성된 후 같은 버전·아키텍처의 이전 패키지를 교체합니다.

`package.ps1`은 Windows PowerShell과 .NET을 사용하므로 별도 압축 프로그램이 필요 없습니다. EXE의 PE 아키텍처와 필수 파일을 확인하며, 패키징 실패도 빌드 명령 실패로 처리합니다. 기존 EXE를 다시 패키징하려면 `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\package.ps1 -Architecture x64`를 실행합니다(`x86`도 가능). ZIP은 Git에서 제외되며 GitHub Release 첨부 파일로 업로드할 수 있습니다. 태그 생성·커밋·푸시·릴리스 게시를 자동 수행하지는 않습니다.

## GitHub Actions 빌드

`.github/workflows/build.yml`은 Windows 실행 환경에서 x86·x64를 각각 빌드·테스트하고 배포 ZIP을 workflow artifact로 업로드합니다. `main` 푸시, `v*` 태그 푸시, `main` 대상 풀 리퀘스트와 수동 실행을 지원합니다. 버전 태그는 `VERSION`과 일치해야 합니다.

workflow를 커밋·푸시한 뒤 **Actions → Build Windows binaries → Run workflow**로 수동 빌드를 시작할 수 있습니다. 완료된 실행 화면의 **Artifacts**에서 결과를 다운로드합니다. `upload-artifact@v7`의 `archive: false`를 사용하므로 추가 ZIP 포장 없이 배포용 ZIP 자체가 artifact가 됩니다. 아키텍처별 artifact를 개별 다운로드하면 해당 ZIP을 GitHub Release에 바로 첨부할 수 있습니다. 보관 기간은 저장소 정책 범위 내에서 14일입니다.

이 workflow는 빌드 결과만 업로드하며 GitHub Release를 만들거나 게시하지 않습니다. 선택한 커밋에 workflow가 있어야 실행되므로, 최초 커밋을 가리키는 기존 `v0.1.0` 태그에 새 workflow가 소급 적용되지는 않습니다.

## 구조와 API 문서

- `fq4r_checksum.c`, `fq4r_checksum.h`: Windows API와 독립적인 공통 C 모듈. 함수 접두사는 `fq4r_`, 상수는 `FQ4R_`입니다.
- `main.c`: Windows 파일 처리와 CLI.
- `i18n.h`: 한국어·영어·일본어 출력.
- `test_cli.py`: CLI 통합 테스트.
- `Doxyfile`: API 문서 생성 설정.

저장 체크섬 8바이트 전체를 0으로 취급한 뒤, 4096바이트씩 8개 블록의 XOR을 계산하고 모든 결과를 마지막에 기록합니다. 공통 모듈은 향후 GUI에서도 직접 재사용할 수 있습니다.

Doxygen 주석은 영어 다음 줄에 한국어를 작성했습니다. 매개변수 방향, 반환값, 전제조건과 버퍼 소유권을 설명합니다. Doxygen 설치 후 이 폴더에서 `doxygen Doxyfile`을 실행하면 `docs/generated/html/index.html`이 생성됩니다. Doxygen은 빌드·실행에 필요하지 않습니다.

`.gitignore`는 빌드 산출물, 생성 문서, 캐시, 로컬 IDE 설정, 세이브와 백업을 제외합니다. 게임 실행 파일이나 실제 세이브를 저장소에 포함할 필요가 없습니다.

## 라이선스

이 도구의 소스 코드와 문서는 [MIT 라이선스](LICENSE)로 공개합니다. 소프트웨어의 복사본이나 주요 부분을 재배포할 때 저작권 및 라이선스 고지를 포함해야 합니다. EXE 배포 시에도 `LICENSE`를 함께 포함하십시오.

라이선스는 이 유틸리티에 적용되며, 원본 게임 First Queen IV와 게임 리소스 또는 세이브 데이터에는 적용되지 않습니다. 이 도구는 비공식 유틸리티이며 게임 권리자와 제휴하거나 승인을 받은 제품이 아닙니다.
