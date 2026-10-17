/* Handle Windows file operations and CLI output; delegate checksum calculation to fq4r_checksum.c.
 * Windows 파일 처리와 CLI 출력을 담당합니다. 체크섬 계산은 fq4r_checksum.c에 위임합니다. */
#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <string.h>
#include <locale.h>
#include "fq4r_checksum.h"
#include "i18n.h"

/* Aggregate file outcomes and enumeration errors; the latter can increase independently of file count.
 * 파일별 처리 결과와 검색 오류를 집계합니다. 검색 오류는 파일 수와 별개로 누적될 수 있습니다. */
typedef struct { unsigned checked, valid, fixed, invalid, errors, mismatched; int fix; } Stats;

/**
 * @brief Request a localized Windows error message, falling back to generic text and the error code.
 * 선택 언어의 Windows 오류 설명을 요청하고, 리소스가 없으면 일반 설명과 오류 번호를 표시합니다.
 *
 * @param[in] action Localized description of the failed operation.
 * 실패한 작업에 대한 번역된 설명입니다.
 * @param[in] code Windows error code captured before other API calls.
 * 다른 API 호출 전에 확보한 Windows 오류 코드입니다.
 */
static void win_error(const wchar_t *action, DWORD code) {
    wchar_t *message = NULL;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS, NULL, code, MAKELANGID(language == LANG_EN ? LANG_ENGLISH : language == LANG_JA ? LANG_JAPANESE : LANG_KOREAN, SUBLANG_DEFAULT), (wchar_t *)&message, 0, NULL);
    wprintf(tr(L"  오류: %ls (Windows %lu): %ls\n", L"  ERROR: %ls (Windows %lu): %ls\n", L"  エラー: %ls (Windows %lu): %ls\n"), action, code,
        message ? message : tr(L"알 수 없는 오류", L"Unknown error", L"不明なエラー"));
    LocalFree(message);
}

/**
 * @brief Prefix absolute paths with the extended namespace to handle Unicode and long paths without OS policy changes.
 * 절대 경로에 확장 경로 접두사를 붙여 OS 정책 변경 없이 Unicode와 긴 경로를 처리합니다.
 *
 * @param[in] path NUL-terminated absolute or relative Unicode path.
 * NUL로 끝나는 절대 또는 상대 Unicode 경로입니다.
 * @return Allocated extended absolute path, or NULL on failure; the caller must free the result.
 * 할당한 확장 절대 경로이며, 실패하면 NULL입니다. 호출자가 반환 메모리를 free해야 합니다.
 */
static wchar_t *absolute_path(const wchar_t *path) {
    DWORD n = GetFullPathNameW(path, 0, NULL, NULL);
    wchar_t *plain, *result;
    size_t len;
    if (!n) return NULL;
    plain = calloc((size_t)n + 1, sizeof(wchar_t));
    if (!plain) return NULL;
    if (!GetFullPathNameW(path, n + 1, plain, NULL)) { free(plain); return NULL; }
    if (wcsncmp(plain, L"\\\\?\\", 4) == 0) return plain;
    /* Reject device paths that cannot be handled as ordinary file paths.
     * 일반 파일 경로로 처리할 수 없는 장치 경로는 거부합니다. */
    if (wcsncmp(plain, L"\\\\.\\", 4) == 0) { free(plain); SetLastError(ERROR_INVALID_NAME); return NULL; }
    len = wcslen(plain);
    result = calloc(len + 9, sizeof(wchar_t));
    if (result) {
        if (wcsncmp(plain, L"\\\\", 2) == 0) swprintf_s(result, len + 9, L"\\\\?\\UNC\\%ls", plain + 2);
        else swprintf_s(result, len + 9, L"\\\\?\\%ls", plain);
    }
    free(plain);
    return result;
}

/**
 * @brief Join a directory and filename; the caller must free the returned allocation.
 * 폴더와 파일명을 결합합니다. 반환 메모리는 호출자가 free해야 합니다.
 *
 * @param[in] dir NUL-terminated directory path.
 * NUL로 끝나는 폴더 경로입니다.
 * @param[in] name NUL-terminated filename or search pattern.
 * NUL로 끝나는 파일명 또는 검색 패턴입니다.
 * @return Allocated joined path, or NULL on allocation failure; the caller must free it.
 * 결합한 경로이며, 할당 실패 시 NULL입니다. 호출자가 반환 메모리를 free해야 합니다.
 */
static wchar_t *join(const wchar_t *dir, const wchar_t *name) {
    size_t n = wcslen(dir) + wcslen(name) + 2;
    wchar_t *s = calloc(n, sizeof(wchar_t));
    if (s) swprintf_s(s, n, L"%ls\\%ls", dir, name);
    return s;
}

/**
 * @brief Apply the two-digit naming rule only to directory scans, never to explicitly supplied files.
 * 폴더 검색에만 숫자 두 자리 규칙을 적용합니다. 직접 지정한 파일에는 적용하지 않습니다.
 *
 * @param[in] name Filename to match against saveNN.dat.
 * saveNN.dat 규칙과 비교할 파일명입니다.
 * @return 1 for exactly two decimal digits with case-insensitive prefix/suffix; otherwise 0.
 * 대소문자를 무시하고 숫자가 정확히 두 자리인 이름이면 1, 아니면 0입니다.
 */
static int save_name(const wchar_t *name) {
    size_t n = wcslen(name), i;
    if (n != 10 || _wcsnicmp(name, L"save", 4) || _wcsicmp(name + 6, L".dat")) return 0;
    for (i = 4; i < 6; ++i) if (name[i] < L'0' || name[i] > L'9') return 0;
    return 1;
}

/**
 * @brief Preserve a unique backup, then replace the original using a same-directory temporary file; return 1 on success.
 * 고유 백업을 보존한 뒤 같은 폴더의 임시 파일로 원본을 교체합니다. 성공은 1, 실패는 0입니다.
 *
 * @param[in] path Existing save path to replace.
 * 교체할 기존 세이브 경로입니다.
 * @param[in] original FQ4R_SAVE_SIZE bytes captured during inspection.
 * 검사 당시 확보한 FQ4R_SAVE_SIZE바이트 원본입니다.
 * @param[in] fixed Complete FQ4R_SAVE_SIZE-byte replacement content.
 * 교체할 전체 FQ4R_SAVE_SIZE바이트 내용입니다.
 * @return 1 on replacement success; 0 on failure after reporting an error.
 * 교체에 성공하면 1, 실패하면 오류를 출력한 뒤 0입니다.
 * @note Retains created backups on failure; does not fully coordinate concurrent file replacement.
 * 실패해도 생성한 백업을 보존합니다. 다른 프로그램의 동시 파일 교체를 완전히 조정하지는 않습니다.
 */
static int replace_save(const wchar_t *path, const uint8_t *original, const uint8_t *fixed) {
    size_t n = wcslen(path) + 80;
    wchar_t *backup = calloc(n, sizeof(wchar_t)), *temp = calloc(n, sizeof(wchar_t));
    HANDLE h = INVALID_HANDLE_VALUE;
    DWORD wrote = 0, error = 0, count;
    unsigned i;
    uint8_t current[FQ4R_SAVE_SIZE];
    LARGE_INTEGER size;
    int ok = 0, created = 0;
    if (!backup || !temp) { wprintf(tr(L"  오류: 메모리가 부족합니다.\n", L"  ERROR: out of memory\n", L"  エラー: メモリが不足しています。\n")); goto done; }
    /* Copy with fail-if-exists enabled so earlier backups are never overwritten.
     * 대상이 존재하면 실패하도록 복사하여 기존 백업을 덮어쓰지 않습니다. */
    for (i = 0; i < 10000; ++i) {
        if (!i) swprintf_s(backup, n, L"%ls.bak", path);
        else swprintf_s(backup, n, L"%ls.bak.%u", path, i);
        if (CopyFileW(path, backup, TRUE)) break;
        error = GetLastError();
        if (error != ERROR_FILE_EXISTS && error != ERROR_ALREADY_EXISTS) goto failed;
    }
    if (i == 10000) { error = ERROR_FILE_EXISTS; goto failed; }
    /* Abort if the backup differs from the inspected content; concurrent replacement is not fully locked out.
     * 백업이 검사 당시 내용과 다르면 수정을 중단합니다. 동시 교체를 완전히 잠그지는 않습니다. */
    h = CreateFileW(backup, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) { error = GetLastError(); goto failed; }
    if (!GetFileSizeEx(h, &size) || size.QuadPart != FQ4R_SAVE_SIZE ||
        !ReadFile(h, current, sizeof(current), &count, NULL) || count != sizeof(current) ||
        memcmp(current, original, sizeof(current))) { error = ERROR_FILE_INVALID; goto failed; }
    CloseHandle(h); h = INVALID_HANDLE_VALUE;
    for (i = 0; i < 10000; ++i) {
        swprintf_s(temp, n, L"%ls.fq4r-checksum.%lu.%u.tmp", path, GetCurrentProcessId(), i);
        h = CreateFileW(temp, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
        if (h != INVALID_HANDLE_VALUE) break;
        error = GetLastError();
        if (error != ERROR_FILE_EXISTS && error != ERROR_ALREADY_EXISTS) goto failed;
    }
    if (h == INVALID_HANDLE_VALUE) { error = ERROR_FILE_EXISTS; goto failed; }
    /* Track successful creation so cleanup removes only a temporary file owned by this invocation.
     * 이 실행이 만든 임시 파일만 정리하도록 생성 성공 여부를 기록합니다. */
    created = 1;
    if (!WriteFile(h, fixed, FQ4R_SAVE_SIZE, &wrote, NULL) || wrote != FQ4R_SAVE_SIZE || !FlushFileBuffers(h)) {
        error = GetLastError(); if (!error) error = ERROR_WRITE_FAULT; goto failed;
    }
    CloseHandle(h); h = INVALID_HANDLE_VALUE;
    /* Replace only after writing and flushing the complete result; the backup is already preserved separately.
     * 전체 결과를 기록하고 flush한 후 교체합니다. 백업은 이미 별도로 보존되어 있습니다. */
    if (!ReplaceFileW(path, temp, NULL, 0, NULL, NULL)) { error = GetLastError(); goto failed; }
    created = 0;
    wprintf(tr(L"  수정 완료: 체크섬을 수정했습니다. 백업: %ls\n", L"  FIXED: checksum bytes updated; backup: %ls\n", L"  修正完了: チェックサムを更新しました。バックアップ: %ls\n"), backup);
    ok = 1;
    goto done;
failed:
    win_error(tr(L"수정 실패 (생성된 백업은 보존됩니다)", L"repair failed (any created backup is retained)", L"修正失敗（作成済みのバックアップは保持されます）"), error);
done:
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    if (created) DeleteFileW(temp);
    free(backup); free(temp);
    return ok;
}

/**
 * @brief Validate format and report eight block results; record file errors and return control to the caller.
 * 형식 검증 후 8영역의 결과를 출력합니다. 파일 오류는 통계에 남기고 호출자에게 복귀합니다.
 *
 * @param[in] path Save path; its filename is not restricted.
 * 세이브 경로이며 파일명은 제한하지 않습니다.
 * @param[in,out] s Aggregate counters and the repair-mode flag.
 * 누적 통계와 수정 모드 설정입니다.
 * @note Reports failures and returns so directory processing can continue.
 * 실패를 보고하고 복귀하여 폴더의 다음 파일을 계속 처리할 수 있게 합니다.
 */
static void check_file(const wchar_t *path, Stats *s) {
    HANDLE h;
    LARGE_INTEGER size;
    uint8_t data[FQ4R_SAVE_SIZE], fixed[FQ4R_SAVE_SIZE], expected[FQ4R_BLOCK_COUNT];
    DWORD count, error;
    unsigned i, bad = 0;
    ++s->checked;
    wprintf(tr(L"\n파일: %ls\n", L"\nFILE: %ls\n", L"\nファイル: %ls\n"), path);
    h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) { win_error(tr(L"파일 열기", L"open", L"ファイルを開く"), GetLastError()); ++s->errors; return; }
    if (!GetFileSizeEx(h, &size)) { error = GetLastError(); CloseHandle(h); win_error(tr(L"파일 크기 확인", L"file size", L"ファイルサイズの確認"), error); ++s->errors; return; }
    if (size.QuadPart != FQ4R_SAVE_SIZE) {
        wprintf(tr(L"  형식 오류: 세이브 파일 크기는 32768바이트여야 합니다. 실제: %lld바이트\n", L"  INVALID FORMAT: expected 32768 bytes, got %lld\n", L"  形式エラー: セーブファイルは32768バイトである必要があります。実際: %lldバイト\n"), size.QuadPart);
        CloseHandle(h); ++s->invalid; return;
    }
    if (!ReadFile(h, data, sizeof(data), &count, NULL) || count != sizeof(data)) {
        error = GetLastError(); CloseHandle(h); win_error(tr(L"파일 읽기", L"read", L"ファイルの読み込み"), error ? error : ERROR_HANDLE_EOF); ++s->errors; return;
    }
    CloseHandle(h);
    if (!fq4r_validate(data, sizeof(data))) {
        wprintf(tr(L"  형식 오류: 세이브 파일 signature는 win4 또는 Win4여야 합니다.\n", L"  INVALID FORMAT: signature must be win4 or Win4\n", L"  形式エラー: セーブファイルの signature は win4 または Win4 である必要があります。\n")); ++s->invalid; return;
    }
    /* Check mode leaves the original unchanged; fix mode also computes changes on a copy first.
     * 검사 모드는 원본을 변경하지 않으며, 수정 모드도 복사본에서 먼저 계산합니다. */
    fq4r_checksums(data, expected);
    wprintf(tr(L"  영역   범위        저장값 계산값   결과\n", L"  Block  Range       Stored Expected Result\n", L"  領域   範囲        保存値 計算値   結果\n"));
    for (i = 0; i < FQ4R_BLOCK_COUNT; ++i) {
        int match = data[FQ4R_CHECKSUM_OFFSET + i] == expected[i];
        wprintf(L"  %u      %04X-%04X   %02X     %02X       %ls\n", i,
            i * FQ4R_BLOCK_SIZE, (i + 1) * FQ4R_BLOCK_SIZE - 1,
            data[FQ4R_CHECKSUM_OFFSET + i], expected[i], match ? tr(L"정상", L"OK", L"正常") : tr(L"불일치", L"MISMATCH", L"不一致"));
        if (!match) ++bad;
    }
    if (!bad) { ++s->valid; wprintf(tr(L"  정상: 8/8 영역의 체크섬이 일치합니다.\n", L"  VALID: 8/8 blocks\n", L"  正常: 8/8 領域のチェックサムが一致しています。\n")); }
    else if (s->fix) {
        memcpy(fixed, data, sizeof(data)); fq4r_repair(fixed, sizeof(fixed));
        if (replace_save(path, data, fixed)) ++s->fixed;
        else ++s->errors;
    } else ++s->mismatched;
}

/**
 * @brief Process filenames in case-insensitive order regardless of filesystem enumeration order.
 * 파일시스템의 열거 순서와 관계없이 대소문자 무시 이름순으로 처리합니다.
 *
 * @param[in] a Pointer to the first Unicode path pointer, as supplied by qsort.
 * qsort가 전달한 첫 번째 Unicode 경로 포인터의 주소입니다.
 * @param[in] b Pointer to the second Unicode path pointer, as supplied by qsort.
 * qsort가 전달한 두 번째 Unicode 경로 포인터의 주소입니다.
 * @return Negative, zero, or positive according to case-insensitive ordering.
 * 대소문자 무시 정렬 순서에 따라 음수, 0 또는 양수입니다.
 */
static int compare_names(const void *a, const void *b) {
    return _wcsicmp(*(const wchar_t *const *)a, *(const wchar_t *const *)b);
}

/**
 * @brief Collect and sort matching paths in this directory, then inspect them without recursing.
 * 현재 폴더의 대상 경로만 수집·정렬한 뒤 검사합니다. 하위 폴더는 탐색하지 않습니다.
 *
 * @param[in] path Directory whose immediate files are scanned.
 * 직접 포함된 파일을 검색할 폴더입니다.
 * @param[in,out] s Aggregate counters and the repair-mode flag.
 * 누적 통계와 수정 모드 설정입니다.
 * @note Only saveNN.dat files are selected; an empty match set is an error.
 * saveNN.dat 파일만 선택하며, 대상이 없으면 오류로 집계합니다.
 */
static void check_directory(const wchar_t *path, Stats *s) {
    wchar_t *pattern = join(path, L"*"), **names = NULL;
    size_t used = 0, capacity = 0, i;
    WIN32_FIND_DATAW entry;
    HANDLE search;
    DWORD error;
    if (!pattern) { ++s->errors; return; }
    search = FindFirstFileW(pattern, &entry); free(pattern);
    if (search == INVALID_HANDLE_VALUE) {
        error = GetLastError();
        if (error != ERROR_FILE_NOT_FOUND) { win_error(tr(L"폴더 목록 읽기", L"list directory", L"フォルダー一覧の取得"), error); ++s->errors; }
        else { wprintf(tr(L"검사할 saveNN.dat 파일이 없습니다.\n", L"No matching saveNN.dat files.\n", L"検査対象の saveNN.dat ファイルがありません。\n")); ++s->errors; }
        return;
    }
    do {
        if (!(entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && save_name(entry.cFileName)) {
            wchar_t *name;
            if (used == capacity) {
                size_t next = capacity ? capacity * 2 : 16;
                wchar_t **grown = realloc(names, next * sizeof(*names));
                if (!grown) { ++s->errors; break; }
                names = grown; capacity = next;
            }
            name = join(path, entry.cFileName);
            if (!name) { ++s->errors; break; }
            names[used++] = name;
        }
    } while (FindNextFileW(search, &entry));
    error = GetLastError(); FindClose(search);
    if (error != ERROR_NO_MORE_FILES) { win_error(tr(L"폴더 검색", L"directory enumeration", L"フォルダーの検索"), error); ++s->errors; }
    if (!used) { wprintf(tr(L"검사할 saveNN.dat 파일이 없습니다.\n", L"No matching saveNN.dat files.\n", L"検査対象の saveNN.dat ファイルがありません。\n")); ++s->errors; }
    if (used > 1) qsort(names, used, sizeof(*names), compare_names);
    for (i = 0; i < used; ++i) { check_file(names[i], s); free(names[i]); }
    free(names);
}

/**
 * @brief Accept Unicode arguments, process language/options/path, and return automation-friendly exit codes.
 * Unicode 인자를 받아 언어, 옵션, 경로를 처리하고 자동화용 종료 코드를 반환합니다.
 *
 * @param[in] argc Argument count including the executable name.
 * 실행 파일명을 포함한 인자 개수입니다.
 * @param[in] argv Unicode argument array provided by the C runtime.
 * C 런타임이 제공한 Unicode 인자 배열입니다.
 * @return 0 for success/help; 1 for checksum mismatches; 2 for format, I/O, usage, or no-match errors.
 * 성공 또는 도움말은 0, 체크섬 불일치는 1, 형식·입출력·인자·대상 없음 오류는 2입니다.
 */
int wmain(int argc, wchar_t **argv) {
    const wchar_t *input = NULL;
    wchar_t *path;
    Stats stats = {0};
    int i, options = 1;
    DWORD attributes;
    /* Emit UTF-8 even when redirected; paths use Unicode internally.
     * 리다이렉션 시에도 UTF-8로 출력합니다. 경로는 내부적으로 Unicode를 사용합니다. */
    setvbuf(stdout, NULL, _IONBF, 0);
    SetConsoleOutputCP(CP_UTF8);
    _wsetlocale(LC_ALL, L".UTF8");
    language = language_from_id(GetUserDefaultUILanguage());
    /* Resolve language in the first pass, including language options placed after help.
     * 1차 순회에서 언어를 먼저 결정하여 도움말 뒤에 있는 언어 옵션도 반영합니다. */
    for (i = 1; i < argc; ++i) {
        const wchar_t *value = NULL;
        if (!wcscmp(argv[i], L"--")) break;
        if (!wcscmp(argv[i], L"--lang") || !wcscmp(argv[i], L"-l")) {
            if (i + 1 < argc) value = argv[++i];
        } else if (!wcsncmp(argv[i], L"--lang=", 7)) value = argv[i] + 7;
        else continue;
        if (!value || !select_language(value)) {
            wprintf(L"%ls\n", tr(L"오류: --lang 값은 ko, en, ja, auto 중 하나여야 합니다.",
                L"ERROR: --lang requires ko, en, ja, or auto.",
                L"エラー: --lang には ko、en、ja、auto のいずれかを指定してください。"));
            print_help(); return 2;
        }
    }
    /* Parse remaining options and the path in a second pass; arguments after -- are treated as paths.
     * 2차 순회에서 나머지 옵션과 경로를 처리합니다. -- 뒤의 인자는 경로로 해석합니다. */
    for (i = 1; i < argc; ++i) {
        if (options && !wcscmp(argv[i], L"--")) { options = 0; continue; }
        if (options && (!wcscmp(argv[i], L"--lang") || !wcscmp(argv[i], L"-l"))) { ++i; continue; }
        if (options && !wcsncmp(argv[i], L"--lang=", 7)) continue;
        if (options && (!wcscmp(argv[i], L"-h") || !wcscmp(argv[i], L"--help"))) {
            print_help(); return 0;
        }
        if (options && (!wcscmp(argv[i], L"-f") || !wcscmp(argv[i], L"--fix"))) stats.fix = 1;
        else if ((options && argv[i][0] == L'-') || input) {
            wprintf(tr(L"오류: 알 수 없는 옵션이거나 경로가 여러 개입니다. --help를 참고하세요.\n", L"ERROR: unknown option or multiple paths. Use --help.\n", L"エラー: 不明なオプション、または複数のパスが指定されています。--help を参照してください。\n")); return 2;
        } else input = argv[i];
    }
    if (!input) {
        wprintf(tr(L"오류: 파일 또는 폴더를 지정하세요.\n", L"ERROR: specify a file or directory. Use --help.\n", L"エラー: ファイルまたはフォルダーを指定してください。\n"));
        print_help(); return 2;
    }
    path = absolute_path(input);
    if (!path) { win_error(tr(L"경로 확인", L"resolve path", L"パスの解決"), GetLastError()); return 2; }
    attributes = GetFileAttributesW(path);
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        win_error(tr(L"경로 접근", L"path", L"パスへのアクセス"), GetLastError()); free(path); return 2;
    }
    if (attributes & FILE_ATTRIBUTE_DIRECTORY) check_directory(path, &stats);
    else check_file(path, &stats);
    free(path);
    wprintf(tr(L"\n요약: 검사=%u 정상=%u 수정=%u 불일치=%u 형식오류=%u 오류=%u\n", L"\nSummary: checked=%u valid=%u fixed=%u mismatched=%u invalid=%u errors=%u\n", L"\n集計: 検査=%u 正常=%u 修正=%u 不一致=%u 形式エラー=%u エラー=%u\n"),
        stats.checked, stats.valid, stats.fixed,
        stats.mismatched,
        stats.invalid, stats.errors);
    /* Format/I/O errors (2) take precedence over checksum mismatches (1); all valid/fixed returns 0.
     * 형식·입출력 오류(2)가 체크섬 불일치(1)보다 우선합니다. 모두 정상/수정 완료이면 0입니다. */
    if (stats.errors || stats.invalid) return 2;
    return stats.checked == stats.valid + stats.fixed ? 0 : 1;
}
