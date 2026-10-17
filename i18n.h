#ifndef FQ4R_I18N_H
#define FQ4R_I18N_H
/* CLI-only translation helpers included by main.c, separate from the shared checksum module.
 * main.c에서만 포함하는 CLI 전용 번역 도우미입니다. 공통 체크섬 모듈과 분리합니다. */
typedef enum { LANG_KO, LANG_EN, LANG_JA } Language;
static Language language = LANG_KO;
/**
 * @brief Return the selected language string without allocating or freeing memory.
 * 선택한 언어의 문자열을 반환하며 메모리를 할당하거나 해제하지 않습니다.
 *
 * @param[in] ko Korean text.
 * 한국어 문자열입니다.
 * @param[in] en English text.
 * 영어 문자열입니다.
 * @param[in] ja Japanese text.
 * 일본어 문자열입니다.
 * @return Borrowed pointer to the selected input string; do not free it.
 * 선택한 입력 문자열을 가리키는 포인터이며 해제하면 안 됩니다.
 */
static const wchar_t *tr(const wchar_t *ko, const wchar_t *en, const wchar_t *ja) {
    return language == LANG_EN ? en : language == LANG_JA ? ja : ko;
}
/**
 * @brief Group regional variants by primary language and fall back to Korean for unsupported languages.
 * 지역별 하위 언어를 주 언어로 묶고, 미지원 언어는 한국어로 처리합니다.
 *
 * @param[in] id Windows UI language identifier.
 * Windows 표시 언어 식별자입니다.
 * @return Supported language, falling back to LANG_KO.
 * 지원하는 언어이며, 미지원 언어는 LANG_KO입니다.
 */
static Language language_from_id(LANGID id) {
    switch (PRIMARYLANGID(id)) {
    case LANG_ENGLISH: return LANG_EN;
    case LANG_JAPANESE: return LANG_JA;
    default: return LANG_KO;
    }
}
/**
 * @brief Codes are case-insensitive; invalid codes leave the current language unchanged and return 0.
 * 언어 코드의 대소문자는 무시합니다. 잘못된 코드는 현재 언어를 유지하며 0을 반환합니다.
 *
 * @param[in] value NUL-terminated code: ko, en, ja, or auto.
 * NUL로 끝나는 ko, en, ja 또는 auto 코드입니다.
 * @return 1 if selected; 0 if invalid, preserving the previous language.
 * 선택 성공 시 1, 잘못된 값은 이전 언어를 유지하고 0입니다.
 * @note Updates the CLI translation state; auto uses the Windows user UI language.
 * CLI 번역 상태를 변경하며, auto는 Windows 사용자 표시 언어를 사용합니다.
 */
static int select_language(const wchar_t *value) {
    if (!_wcsicmp(value, L"ko")) language = LANG_KO;
    else if (!_wcsicmp(value, L"en")) language = LANG_EN;
    else if (!_wcsicmp(value, L"ja")) language = LANG_JA;
    else if (!_wcsicmp(value, L"auto")) language = language_from_id(GetUserDefaultUILanguage());
    else return 0;
    return 1;
}
/**
 * @brief Provide the same language-selection guidance for missing paths and explicit help requests.
 * 경로가 없을 때와 도움말 요청 시 같은 언어 선택 안내를 제공합니다.
 *
 * @note Writes help in the current language without changing settings or files.
 * 설정이나 파일을 변경하지 않고 현재 언어로 도움말을 출력합니다.
 */
static void print_help(void) {
    wprintf(L"%ls\n", tr(
        L"FQ4R 세이브 체크섬 CLI 도구\n"
        L"사용법: fq4r-checksum.exe [-f|--fix] [--lang ko|en|ja|auto] <파일 또는 폴더>\n"
        L"폴더: saveNN.dat(숫자 두 자리)를 검사합니다. 대소문자 구분 및 하위 폴더 검색 없음.\n"
        L"파일: 이름 제한 없음. 크기와 signature는 항상 검사합니다.\n"
        L"--fix, -f: 고유한 .bak 백업 후 수정합니다. 정상 파일은 변경하지 않습니다.\n"
        L"--lang, -l: ko 한국어 / en English / ja 日本語 / auto 자동 선택.\n"
        L"기본: Windows 표시 언어로 자동 선택하며, 지원하지 않는 언어는 한국어를 사용합니다.\n"
        L"종료 코드: 0 정상/수정 완료, 1 체크섬 불일치, 2 형식/입출력/인자 오류 또는 대상 없음.",
        L"FQ4R Save Checksum CLI Tool\n"
        L"Usage: fq4r-checksum.exe [-f|--fix] [--lang ko|en|ja|auto] <file-or-directory>\n"
        L"Directory: check saveNN.dat (two digits), case-insensitive, non-recursive.\n"
        L"Explicit file: any name; size/signature are always validated.\n"
        L"--fix, -f: repair with a unique .bak backup; valid files unchanged.\n"
        L"--lang, -l: ko 한국어 / en English / ja 日本語 / auto automatic selection.\n"
        L"Default: Windows display language; unsupported languages fall back to Korean.\n"
        L"Exit: 0 all valid/fixed, 1 checksum mismatch, 2 format/I/O/usage/no-match error.",
        L"FQ4R セーブデータチェックサムCLIツール\n"
        L"使い方: fq4r-checksum.exe [-f|--fix] [--lang ko|en|ja|auto] <ファイルまたはフォルダー>\n"
        L"フォルダー: saveNN.dat（数字2桁）を検査します。大文字小文字の区別なし、再帰検索なし。\n"
        L"ファイル: 名前の制限なし。サイズと signature は常に検査します。\n"
        L"--fix, -f: 一意の .bak バックアップを作成して修正します。正常なファイルは変更しません。\n"
        L"--lang, -l: ko 한국어 / en English / ja 日本語 / auto 自動選択。\n"
        L"既定: Windows の表示言語で自動選択します。未対応の言語は韓国語になります。\n"
        L"終了コード: 0 正常/修正完了、1 チェックサム不一致、2 形式/入出力/引数エラーまたは対象なし。"));
}
#endif
