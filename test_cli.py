"""CLI integration tests modify temporary copies only; original saves are read-only.
임시 복사본만 수정하는 CLI 통합 테스트입니다. 원본 세이브는 읽기 전용입니다.
"""
import subprocess
import tempfile
import unittest
from pathlib import Path

# Compare C results against a small, independent Python XOR implementation.
# 독립적인 Python XOR 구현과 C 결과를 비교합니다.
ROOT = Path(__file__).resolve().parents[2]
EXE = Path(__file__).parent / 'bin' / 'fq4r-checksum.exe'


class Reference:
    @staticmethod
    def repair_checksums(data):
        # Exclude the complete checksum area before calculating any block.
        # 블록 계산 전에 체크섬 저장 영역 전체를 제외합니다.
        result = bytearray(data)
        result[0x7DA6:0x7DAE] = bytes(8)
        expected = []
        for offset in range(0, 0x8000, 0x1000):
            value = 0
            for byte in result[offset:offset + 0x1000]:
                value ^= byte
            expected.append(value)
        result[0x7DA6:0x7DAE] = bytes(expected)
        return bytes(result)


ref = Reference()
# Synthetic fixtures allow testing without distributing game saves or binaries.
# 합성 데이터로 게임 세이브나 실행 파일을 배포하지 않고 테스트할 수 있습니다.
fixture = bytearray((i * 37 + i // 97) & 255 for i in range(0x8000))
fixture[:4] = b'win4'
BASE_SAVE = ref.repair_checksums(fixture)



class CliTests(unittest.TestCase):
    # Use English for general tests regardless of OS language and decode UTF-8 strictly.
    # 일반 테스트는 OS 언어와 무관하게 영어 출력을 사용하고 UTF-8을 엄격히 검사합니다.
    def run_cli(self, *args):
        return subprocess.run([str(EXE), '--lang', 'en', *map(str, args)], capture_output=True,
                              encoding='utf-8', errors='strict')

    # Check actual saves successfully and verify that original bytes remain unchanged.
    # 실제 세이브의 검사 성공과 원본 바이트 보존을 확인합니다.
    def test_originals_read_only(self):
        paths = sorted(ROOT.glob('save[0-9][0-9].dat'))
        if not paths:
            self.skipTest('Optional original saves are not available; synthetic tests still run.')
        for path in paths:
            with self.subTest(path=path.name):
                original = path.read_bytes()
                result = self.run_cli(path)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertIn('VALID: 8/8', result.stdout)
                self.assertEqual(original, path.read_bytes())

    # Cover corruption in every block and the checksum area, repeat repair, and backup preservation.
    # 모든 블록 변조, 체크섬 영역 변조, 반복 수정, 백업 중복 보존을 함께 검증합니다.
    def test_all_blocks_fix_backup_and_idempotence(self):
        with tempfile.TemporaryDirectory(dir=Path(__file__).parent) as directory:
            path = Path(directory) / '한글 일본어 日本語 任意.bin'
            original = bytearray(BASE_SAVE)
            for i in range(8):
                original[i * 0x1000 + 0x100] ^= 0x5A
            path.write_bytes(original)
            result = self.run_cli(path)
            self.assertEqual(result.returncode, 1, result.stdout)
            self.assertEqual(result.stdout.count('MISMATCH'), 8)
            result = self.run_cli('--fix', path)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertEqual(path.read_bytes(), ref.repair_checksums(original))
            self.assertEqual(Path(str(path) + '.bak').read_bytes(), original)
            result = self.run_cli(path, '-f')
            self.assertEqual(result.returncode, 0, result.stdout)
            self.assertFalse(Path(str(path) + '.bak.1').exists())
            damaged = bytearray(path.read_bytes())
            damaged[0x7DA6:0x7DAE] = bytes(8)
            path.write_bytes(damaged)
            result = self.run_cli('-f', path)
            self.assertEqual(result.returncode, 0, result.stdout)
            self.assertEqual(Path(str(path) + '.bak.1').read_bytes(), damaged)
            self.assertEqual(Path(str(path) + '.bak').read_bytes(), original)
            self.assertEqual(path.read_bytes(), ref.repair_checksums(damaged))

    # Verify continuation after invalid files, filename filtering, and non-recursive enumeration.
    # 잘못된 파일을 건너뛰고 계속 검사하며 이름 필터와 비재귀 검색을 지키는지 확인합니다.
    def test_directory_filter_errors_continue(self):
        with tempfile.TemporaryDirectory(dir=Path(__file__).parent) as directory:
            folder = Path(directory)
            data = BASE_SAVE
            (folder / 'save00.dat').write_bytes(b'bad size')
            invalid = bytearray(data); invalid[:4] = b'WIN4'
            (folder / 'save01.dat').write_bytes(invalid)
            (folder / 'SAVE02.DAT').write_bytes(data)
            for name in ['other.dat', 'saveAB.dat', 'save000.dat', 'save03.dat.bak']:
                (folder / name).write_bytes(b'ignored')
            nested = folder / 'nested'; nested.mkdir()
            (nested / 'save04.dat').write_bytes(b'ignored')
            result = self.run_cli('-f', folder)
            self.assertEqual(result.returncode, 2, result.stdout)
            self.assertIn('checked=3 valid=1 fixed=0 mismatched=0 invalid=2 errors=0', result.stdout)
            self.assertEqual((folder / 'save00.dat').read_bytes(), b'bad size')
            self.assertEqual([p.name for p in folder.glob('*.bak')], ['save03.dat.bak'])

    # Verify repair failure for read-only files and restore temporary file attributes during cleanup.
    # 읽기 전용 파일의 수정 실패를 검증하고 종료 시 임시 파일 속성을 복원합니다.
    def test_read_only_fix_failure_preserves_original(self):
        import ctypes
        with tempfile.TemporaryDirectory(dir=Path(__file__).parent) as directory:
            path = Path(directory) / 'save00.dat'
            data = bytearray(BASE_SAVE); data[500] ^= 1
            path.write_bytes(data)
            set_attributes = ctypes.windll.kernel32.SetFileAttributesW
            self.assertTrue(set_attributes(str(path.resolve()), 1))
            try:
                result = self.run_cli('--fix', path)
                self.assertEqual(result.returncode, 2, result.stdout)
                self.assertEqual(path.read_bytes(), data)
            finally:
                for item in Path(directory).iterdir():
                    set_attributes(str(item.resolve()), 0x80)

    # Test accepted signatures, arbitrary filenames, absent targets, invalid options, and help exit codes.
    # 허용 signature, 임의 파일명, 대상 없음, 잘못된 옵션 및 도움말 종료 코드를 검증합니다.
    def test_signature_and_usage(self):
        with tempfile.TemporaryDirectory(dir=Path(__file__).parent) as directory:
            folder = Path(directory)
            self.assertEqual(self.run_cli(folder).returncode, 2)
            self.assertEqual(self.run_cli(folder / 'absent').returncode, 2)
            path = folder / 'arbitrary'
            data = bytearray(BASE_SAVE); data[:4] = b'Win4'
            path.write_bytes(ref.repair_checksums(data))
            self.assertEqual(self.run_cli(path).returncode, 0)
        self.assertEqual(self.run_cli().returncode, 2)
        self.assertEqual(self.run_cli('--unknown').returncode, 2)
        self.assertEqual(self.run_cli('--help').returncode, 0)

    # Verify checking, backup, and repair on paths exceeding the traditional MAX_PATH limit.
    # 기존 MAX_PATH를 넘는 경로에서 검사와 백업·수정이 동작하는지 확인합니다.
    def test_long_path(self):
        with tempfile.TemporaryDirectory(dir=Path(__file__).parent) as directory:
            folder = Path(directory)
            for _ in range(5):
                folder /= 'long_path_' + 'x' * 45
            folder.mkdir(parents=True)
            path = folder / 'save00.dat'
            data = bytearray(BASE_SAVE); data[123] ^= 1
            path.write_bytes(data)
            result = self.run_cli('--fix', path)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertEqual(path.read_bytes(), ref.repair_checksums(data))

    # Test help, check, error, and fix output in all three languages and option-order independence.
    # 세 언어의 도움말·검사·오류·수정 출력과 옵션 순서 독립성을 검증합니다.
    def test_languages_and_help(self):
        for lang, help_word, valid_word, error_word, fixed_word in [
            ('ko', '사용법:', '정상: 8/8', '형식 오류:', '수정 완료:'),
            ('en', 'Usage:', 'VALID: 8/8', 'INVALID FORMAT:', 'FIXED:'),
            ('ja', '使い方:', '正常: 8/8', '形式エラー:', '修正完了:'),
        ]:
            with self.subTest(lang=lang):
                for args in [('--help', '--lang', lang), ('-l', lang, '-h'),
                             ('--lang=' + lang, '--help'), ('--lang', lang)]:
                    result = self.run_cli(*args)
                    self.assertEqual(result.returncode, 0 if '-h' in args or '--help' in args else 2)
                    self.assertIn(help_word, result.stdout)
                    for code in ('ko', 'en', 'ja', 'auto'):
                        self.assertIn(code, result.stdout)
                with tempfile.TemporaryDirectory(dir=Path(__file__).parent) as directory:
                    path = Path(directory) / 'save00.dat'
                    path.write_bytes(BASE_SAVE)
                    self.assertIn(valid_word, self.run_cli('--lang', lang, path).stdout)
                    path.write_bytes(b'bad')
                    result = self.run_cli('--lang', lang, path)
                    self.assertEqual(result.returncode, 2)
                    self.assertIn(error_word, result.stdout)
                    data = bytearray(BASE_SAVE); data[123] ^= 1
                    path.write_bytes(data)
                    result = self.run_cli('--lang', lang, '-f', path)
                    self.assertEqual(result.returncode, 0, result.stdout)
                    self.assertIn(fixed_word, result.stdout)
                    self.assertEqual(path.read_bytes(), ref.repair_checksums(data))
        for args in [('--lang',), ('--lang=xx',), ('--lang', 'xx')]:
            self.assertEqual(self.run_cli(*args).returncode, 2)

    # Bypass the language-forcing helper to verify automatic selection from the actual Windows UI language.
    # 명시적 언어를 주입하는 도우미를 우회하여 실제 Windows 표시 언어 자동 선택을 검증합니다.
    def test_auto_language_and_no_arguments(self):
        import ctypes
        primary = ctypes.windll.kernel32.GetUserDefaultUILanguage() & 0x3ff
        word = {9: 'Usage:', 17: '使い方:'}.get(primary, '사용법:')
        for args in [[], ['--help'], ['--lang', 'auto', '--help']]:
            result = subprocess.run([str(EXE), *args], capture_output=True, encoding='utf-8')
            self.assertEqual(result.returncode, 0 if '--help' in args else 2)
            self.assertIn(word, result.stdout)
            self.assertIn('--lang', result.stdout)


if __name__ == '__main__':
    unittest.main(verbosity=2)
