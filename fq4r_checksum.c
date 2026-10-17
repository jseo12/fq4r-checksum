#include "fq4r_checksum.h"
#include <string.h>

/**
 * @brief Short-circuit evaluation checks size before reading the signature; WIN4 is not accepted.
 * 단락 평가로 크기를 먼저 확인한 뒤 signature에 접근합니다. WIN4는 허용하지 않습니다.
 *
 * @param[in] data Save buffer, or NULL to reject the input.
 * 세이브 버퍼입니다. NULL이면 유효하지 않은 입력으로 처리합니다.
 * @param[in] size Number of accessible bytes in data.
 * data에서 접근 가능한 바이트 수입니다.
 * @return 1 if size is 32768 and signature is win4 or Win4; otherwise 0.
 * 크기가 32768이고 signature가 win4 또는 Win4이면 1, 아니면 0입니다.
 * @note Does not modify the input or validate game-state semantics.
 * 입력을 변경하거나 게임 상태의 의미적 유효성을 검사하지 않습니다.
 */
int fq4r_validate(const uint8_t *data, size_t size) {
    return data && size == FQ4R_SAVE_SIZE &&
        (data[0] == 'w' || data[0] == 'W') &&
        data[1] == 'i' && data[2] == 'n' && data[3] == '4';
}

/**
 * @brief The caller must provide a validated complete save buffer; each block has an independent XOR.
 * 호출자는 검증된 전체 세이브 버퍼를 제공해야 합니다. 각 블록의 XOR은 서로 독립적입니다.
 *
 * @param[in] data Validated save buffer of FQ4R_SAVE_SIZE bytes.
 * 형식 검증을 마친 FQ4R_SAVE_SIZE바이트 세이브 버퍼입니다.
 * @param[out] expected Writable array of FQ4R_BLOCK_COUNT checksum bytes.
 * 계산 결과 FQ4R_BLOCK_COUNT바이트를 받을 쓰기 가능한 배열입니다.
 * @pre data and expected are non-NULL and do not overlap.
 * data와 expected는 NULL이 아니며 서로 겹치지 않아야 합니다.
 * @note All eight stored checksum bytes are treated as zero, including those in block 7.
 * 7번 블록에 포함된 저장 체크섬 8바이트 전체를 0으로 취급합니다.
 */
void fq4r_checksums(const uint8_t *data, uint8_t expected[FQ4R_BLOCK_COUNT]) {
    size_t i;
    memset(expected, 0, FQ4R_BLOCK_COUNT);
    for (i = 0; i < FQ4R_SAVE_SIZE; ++i) {
        /* Treat all eight stored checksum bytes within the last block as zero.
         * 마지막 블록에 포함된 체크섬 8바이트 전체를 0으로 취급합니다. */
        if (i < FQ4R_CHECKSUM_OFFSET || i >= FQ4R_CHECKSUM_OFFSET + FQ4R_BLOCK_COUNT)
            expected[i / FQ4R_BLOCK_SIZE] ^= data[i];
    }
}

/**
 * @brief Compute every expected value before writing, keeping the input stable during calculation.
 * 모든 기대값을 먼저 계산한 뒤 기록하여 계산 중 입력이 바뀌지 않도록 합니다.
 *
 * @param[in,out] data Save buffer whose checksum bytes may be updated in place.
 * 체크섬 바이트를 제자리 수정할 세이브 버퍼입니다.
 * @param[in] size Number of accessible bytes in data.
 * data에서 접근 가능한 바이트 수입니다.
 * @return 1 on valid input, including already-correct checksums; 0 on invalid format.
 * 이미 정상인 경우를 포함하여 유효한 입력이면 1, 형식이 잘못되었으면 0입니다.
 * @note Invalid input is unchanged; valid input is modified only at offsets 0x7DA6 through 0x7DAD.
 * 잘못된 입력은 변경하지 않으며, 유효한 입력도 0x7DA6부터 0x7DAD까지만 수정합니다.
 */
int fq4r_repair(uint8_t *data, size_t size) {
    uint8_t expected[FQ4R_BLOCK_COUNT];
    if (!fq4r_validate(data, size)) return 0;
    fq4r_checksums(data, expected);
    memcpy(data + FQ4R_CHECKSUM_OFFSET, expected, FQ4R_BLOCK_COUNT);
    return 1;
}
