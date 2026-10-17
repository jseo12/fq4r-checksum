# FQ4R 세이브 체크섬: HEX 편집 참고

FQ4R Windows판 세이브를 HEX 에디터로 수정할 때 참고하는 문서입니다. 현재 분석한 한글 패치판을 기준으로 하며, 다른 버전의 동일 형식을 보장하지는 않습니다.

## 파일 형식

- 크기: **0x8000바이트 = 32768바이트**.
- 파일 시작 4바이트: `77 69 6E 34` (`win4`) 또는 `57 69 6E 34` (`Win4`).
- 아래 주소는 모두 **파일 시작을 0으로 한 16진수 오프셋**이며 범위의 양 끝을 포함합니다.
- HEX 에디터는 **덮어쓰기 모드**를 사용합니다. 바이트를 삽입·삭제하여 파일 크기를 바꾸면 안 됩니다.

## 체크섬 위치와 검사 범위

파일을 **0x1000바이트(4096바이트)씩 8개 블록**으로 나누며, 각 블록의 체크섬은 **1바이트**입니다.

| 블록 | 검사 범위 | 체크섬 저장 주소 |
|---|---|---|
| 0 | `0000~0FFF` | `7DA6` |
| 1 | `1000~1FFF` | `7DA7` |
| 2 | `2000~2FFF` | `7DA8` |
| 3 | `3000~3FFF` | `7DA9` |
| 4 | `4000~4FFF` | `7DAA` |
| 5 | `5000~5FFF` | `7DAB` |
| 6 | `6000~6FFF` | `7DAC` |
| 7 | `7000~7FFF` | `7DAD` |

돈·아이템·마법 등 데이터 종류별로 정해지는 것이 아니라 **수정 주소가 속한 블록**으로 정해집니다. 예를 들어 `0692`는 블록 0, 아레스 능력치가 있는 `25xx`는 블록 2입니다.

## 전체 재계산 방법

계산은 덧셈이 아닌 **바이트 단위 XOR(배타적 논리합)**입니다.

1. 원본을 백업합니다.
2. 데이터를 수정합니다.
3. 체크섬 저장 영역 **`7DA6~7DAD`의 8바이트 전체를 0으로 취급**합니다. HEX 에디터에서 계산한다면 먼저 이 영역을 `00` 8개로 덮어씁니다.
4. 각 블록의 모든 바이트를 XOR하여 8개의 결과를 따로 기록합니다.
5. 계산이 모두 끝난 다음, 결과를 순서대로 `7DA6~7DAD`에 기록합니다.

```text
checksum[block] = 00 XOR byte[start] XOR ... XOR byte[end]
```

여기서 헷갈리기 쉬운 점은 **체크섬을 적는 8칸도 마지막 블록 안에 있다**는 것입니다.

```text
마지막 블록: 7000 ...................................... 7FFF
                           7DA6 ........ 7DAD
                           체크섬을 적는 8칸
```

마지막 블록을 계산할 때 이 8칸의 숫자까지 함께 XOR하면, 데이터뿐 아니라 체크섬 값까지 계산에 섞이게 됩니다. 따라서 **계산할 때는 이 8칸을 전부 `00`으로 비워둡니다.**

예를 들어 첫 번째 블록을 계산한 결과가 `A0`이라고 해도, `7DA6`에 바로 쓰지 말고 메모장 등에 적어두세요. 바로 쓰면 마지막 블록을 계산할 때 그 `A0`까지 포함되기 때문입니다.

**“8칸을 먼저 0으로 비운다 → 8개 결과를 따로 구한다 → 마지막에 8칸을 채운다”** 순서로 작업하면 됩니다. HEX 에디터가 특정 주소를 계산에서 제외하는 기능을 제공한다면, 이 8칸을 제외해도 같은 결과입니다.

## C 예제

아래는 파일을 이미 메모리로 읽었다고 가정한 간단한 예제입니다. 성공하면 버퍼의 체크섬 8바이트만 수정합니다. 실제 파일 열기·백업·저장 코드는 생략했습니다. C에서 `^`는 XOR 연산자입니다.

```c
#include <stddef.h>
#include <string.h>

/**
 * @brief Recalculate the eight checksum bytes in a loaded save buffer.
 * 메모리로 읽은 세이브 버퍼의 체크섬 8바이트를 다시 계산합니다.
 * @param[in,out] save Writable save buffer; NULL is rejected.
 * 쓰기 가능한 세이브 버퍼이며 NULL은 허용하지 않습니다.
 * @param[in] size Accessible buffer size in bytes.
 * 버퍼에서 접근 가능한 바이트 수입니다.
 * @return 1 on success; 0 on invalid size or signature, leaving input unchanged.
 * 성공 시 1, 크기나 signature 오류 시 입력을 변경하지 않고 0을 반환합니다.
 */
int fix_save_checksums(unsigned char *save, size_t size)
{
    unsigned char results[8] = {0};
    size_t block, offset;

    if (save == NULL || size != 0x8000)
        return 0;
    if (memcmp(save, "win4", 4) != 0 && memcmp(save, "Win4", 4) != 0)
        return 0;

    /* Clear all eight checksum slots before calculating any block.
     * 블록을 계산하기 전에 체크섬 8칸 전체를 0으로 비웁니다. */
    memset(save + 0x7DA6, 0, 8);

    for (block = 0; block < 8; ++block) {
        size_t start = block * 0x1000;
        size_t end = start + 0x1000;
        for (offset = start; offset < end; ++offset)
            results[block] ^= save[offset];
    }

    /* Write the results only after all eight calculations are finished.
     * 8개 계산이 모두 끝난 다음 결과를 기록합니다. */
    memcpy(save + 0x7DA6, results, 8);
    return 1;
}
```

## Python 예제

동일한 계산을 Python으로 표현한 예제입니다. 원본 입력은 유지하고, 체크섬을 수정한 복사본을 반환합니다. 파일을 직접 저장하지는 않습니다. Python에서도 `^`는 XOR입니다.

```python
def fix_save_checksums(data: bytes) -> bytes:
    """Return a checksum-repaired copy without changing the input.
    입력을 변경하지 않고 체크섬을 수정한 복사본을 반환합니다.
    """
    if len(data) != 0x8000:
        raise ValueError("Expected exactly 32768 bytes")
    if data[:4] not in (b"win4", b"Win4"):
        raise ValueError("Expected win4 or Win4 signature")

    save = bytearray(data)

    # Clear all eight checksum slots before calculating any block.
    # 블록을 계산하기 전에 체크섬 8칸 전체를 0으로 비웁니다.
    save[0x7DA6:0x7DAE] = bytes(8)
    results = bytearray(8)

    for block in range(8):
        start = block * 0x1000
        end = start + 0x1000
        for offset in range(start, end):
            results[block] ^= save[offset]

    # Write the results only after all eight calculations are finished.
    # 8개 계산이 모두 끝난 다음 결과를 기록합니다.
    save[0x7DA6:0x7DAE] = results
    return bytes(save)
```

두 예제의 `end`는 **마지막 주소의 다음 주소**입니다. 예를 들어 첫 블록은 `start=0x0000`, `end=0x1000`이며 실제 계산 범위는 `0x0000~0x0FFF`입니다. Python의 `[0x7DA6:0x7DAE]`도 `0x7DAE`는 포함하지 않아 정확히 8바이트입니다.

## 몇 바이트만 바꿀 때의 간단한 방법

**수정 전 체크섬이 정상이고, 변경한 모든 바이트의 원래 값을 알고 있다면**, 전체 블록을 다시 계산하지 않고 다음 식으로 갱신할 수 있습니다.

```text
새 체크섬 = 기존 체크섬 XOR 원래 바이트 XOR 수정 바이트
```

예를 들어 `06E7`의 `06`을 `07`로 바꾸고, `7DA6`의 기존 체크섬이 `A0`이었다면:

```text
A0 XOR 06 XOR 07 = A1
→ 7DA6에 A1 기록
```

여러 바이트를 수정했다면 **같은 블록의 모든 변경 쌍**을 적용합니다.

```text
새 체크섬 = 기존 체크섬 XOR 이전값1 XOR 새값1 XOR 이전값2 XOR 새값2 ...
```

서로 다른 블록을 수정했다면 해당 체크섬을 각각 갱신합니다. 데이터가 블록 경계를 넘으면 양쪽 블록 모두 처리합니다. 이 식은 체크섬 저장 영역 자체를 일반 데이터처럼 수정하는 경우에는 적용하지 않습니다. 기존 체크섬부터 틀렸다면 전체 재계산을 사용하세요.

## 주의할 점

- HEX 에디터의 계산 기능에서 일반 합계·CRC·MD5가 아닌 **8비트 XOR**을 선택합니다. 지원하지 않으면 계산기에서 바이트들을 XOR하거나 전용 도구를 사용합니다.
- 체크섬 8개는 각각 독립된 1바이트 값입니다. 16비트 정수나 little endian 값으로 묶지 않습니다.
- **같은 블록 안에서 두 바이트의 위치만 바꾸면 XOR은 유지**됩니다. 다른 블록 사이의 교환에는 이 규칙을 그대로 적용할 수 없습니다.
- “변경량을 더하거나 뺀다” 또는 “16진수 앞뒤 자리의 합을 맞춘다”는 방식은 일반적인 계산법이 아닙니다.
- 체크섬이 맞아도 아이템 ID·부대 참조·시나리오 상태 등이 잘못되면 게임이 정상적으로 읽지 못할 수 있습니다. 체크섬은 데이터의 의미까지 검증하지 않습니다.

구현 기준: [fq4r_checksum.c](fq4r_checksum.c)의 `fq4r_checksums()`.
