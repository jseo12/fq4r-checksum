# FQ4R Save Checksum CLI Tool

**English** | [한국어](README.ko.md) | [日本語](README.ja.md)

A tool for checking and repairing save-file checksums for the Windows remake of First Queen IV, referred to as **FQ4R** in this project. The official Japanese title is 「ファーストクイーンⅣ」. [Official website](http://kure.sakura.ne.jp/fq4/)

Repository name: `fq4r-checksum`. Executable: `fq4r-checksum.exe`. Written in C for Windows x64, using Windows system DLLs and a statically linked C runtime. No browser engine, Node.js, Python, or separate VC runtime installation is required to run it. Python is needed only for tests.

## Usage

Run these commands from the directory containing the downloaded executable. Source builds place it in `bin`.

```powershell
# One file: any filename is accepted
.\fq4r-checksum.exe "C:\Games\FQ4R\custom.dat"

# Directory: check save00.dat through save99.dat
.\fq4r-checksum.exe "C:\Games\FQ4R"

# Repair incorrect checksums after creating backups
.\fq4r-checksum.exe --fix "C:\Games\FQ4R"

# Language selection and help
.\fq4r-checksum.exe --lang en "C:\Games\FQ4R\save00.dat"
.\fq4r-checksum.exe --help
```

| Argument or option | Behavior |
|---|---|
| File or directory | Exactly one path is required |
| `-f`, `--fix` | Back up and repair only files with incorrect checksums |
| `-l`, `--lang ko\|en\|ja\|auto` | Select output language; `--lang=en` also works |
| `-h`, `--help` | Show usage and language-selection guidance |
| `--` | Stop interpreting subsequent arguments as options |

Directory scans select `saveNN.dat`, with exactly two decimal digits, case-insensitively and in filename order. Subdirectories are not scanned. Explicitly supplied files have no filename restriction.

Every file must be exactly **32768 bytes**, with signature **`win4` or `Win4`**. `WIN4` is rejected. Format and I/O errors are reported without stopping the remaining files; invalid formats are never repaired. Each file reports the range, stored checksum, expected checksum, and validity of all eight blocks.

## Languages and exit codes

The default, `auto`, selects Korean, English, or Japanese from the Windows user display language. Unsupported languages fall back to Korean. This uses the UI language, not regional formatting or the console code page. Explicit language options override automatic selection; the last language option wins. Codes are case-insensitive and preferences are not persisted.

No arguments prints an error and help including language selection. Language options after help also work, for example `--help --lang ja`. Output is UTF-8. If localized Windows error resources are unavailable, a generic translated message and the Windows error number are shown.

| Exit code | Meaning |
|---|---|
| `0` | All files valid or repaired, or help displayed |
| `1` | Checksum mismatches remain |
| `2` | Format, I/O, or argument error, or no matching files |

Code `2` takes precedence over `1`. The summary counts attempted files, originally valid files, repaired files, check-mode mismatches, invalid formats, and other errors. Enumeration errors may be counted independently of files.

## Repair and backups

The complete original is copied to `.bak`, or `.bak.1`, `.bak.2`, etc. without overwriting earlier backups. The backup is compared with the inspected input. Backup failure prevents repair. Valid files are neither modified nor backed up.

The replacement is written to a unique temporary file in the same directory, flushed, and installed with `ReplaceFileW`. Only **eight bytes at `0x7DA6–0x7DAD`** are changed. Created backups are retained on failure. Close the game before repairing: this tool does not fully coordinate concurrent saves or replacements by other programs.

A valid checksum does not establish semantic validity or guarantee that the game can load the save. Eight Korean-patched saves were checked during development; compatibility with every game language, update, or Windows version has not been verified.

## Build and test

Install Visual Studio C++ desktop build tools and the Windows SDK.

```powershell
.\build.cmd
.\bin\fq4r-checksum.exe --help
python .\test_cli.py
```

The script uses the active MSVC compiler or discovers an installation through `vswhere` and initializes its x64 environment. It enables size optimization, static CRT linking, and warnings as errors. Use an x64 Native Tools prompt when configuring MSVC manually.

Tests use synthetic saves and an independent Python XOR implementation, so no game files are required. Coverage includes corruption in all blocks and the checksum area, backup preservation, repeat repairs, invalid formats, filename filtering, Unicode and long paths, read-only files, and all three languages. Repair tests modify temporary copies only. The optional original-save check runs in the original analysis workspace and is skipped when those files are absent.

## Structure and API documentation

- `fq4r_checksum.c`, `fq4r_checksum.h`: shared C core without Windows dependencies; functions use `fq4r_`, constants use `FQ4R_`.
- `main.c`: Windows file operations and CLI.
- `i18n.h`: Korean, English, and Japanese output.
- `test_cli.py`: CLI integration tests.
- `Doxyfile`: API documentation configuration.

Treat all eight stored checksum bytes as zero, calculate an independent XOR for each of eight 4096-byte blocks, then write all results. The core can also be linked directly into a future GUI.

Doxygen comments place English first and Korean below it, documenting parameter directions, return values, preconditions, and buffer ownership. With Doxygen installed, run `doxygen Doxyfile` here and open `docs/generated/html/index.html`. Doxygen is not required to build or run the tool.

The `.gitignore` excludes build artifacts, generated documentation, caches, local IDE settings, saves, and backups. Game executables and real saves are not needed in the repository.

## License

This tool's source code and documentation are available under the [MIT License](LICENSE). Include the copyright and license notices when redistributing copies or substantial portions of the software. Include `LICENSE` with binary releases as well.

The license applies to this utility, not to First Queen IV, its game assets, or save data. This is an unofficial utility and is not affiliated with or endorsed by the game's rights holders.
