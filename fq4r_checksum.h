#ifndef FQ4R_CHECKSUM_H
#define FQ4R_CHECKSUM_H
/* Shared checksum interface, independent of console and Windows APIs.
 * 콘솔이나 Windows API에 의존하지 않는 공통 체크섬 인터페이스입니다. */
#include <stddef.h>
#include <stdint.h>
/* Define the fixed file size, checksum storage offset, and independent blocks.
 * 고정 파일 크기, 체크섬 저장 위치, 독립적으로 검사할 블록 구성을 정의합니다. */
#define FQ4R_SAVE_SIZE 0x8000
#define FQ4R_CHECKSUM_OFFSET 0x7DA6
#define FQ4R_BLOCK_SIZE 0x1000
#define FQ4R_BLOCK_COUNT 8
/**
 * @brief Return 1 for a valid size/signature, otherwise 0; input is not modified.
 * 크기와 signature가 맞으면 1, 아니면 0을 반환하며 입력을 변경하지 않습니다.
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
int fq4r_validate(const uint8_t *data, size_t size);
/**
 * @brief Requires validated 32768-byte input and a non-overlapping 8-byte output buffer.
 * 검증된 32768바이트 입력과 8바이트 출력 버퍼가 필요합니다. 두 버퍼는 겹치면 안 됩니다.
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
void fq4r_checksums(const uint8_t *data, uint8_t expected[FQ4R_BLOCK_COUNT]);
/**
 * @brief Update only eight checksum bytes in place and return 1; invalid input is unchanged and returns 0.
 * 유효한 입력의 체크섬 8바이트만 제자리 수정하고 1을 반환합니다. 잘못된 입력은 변경 없이 0입니다.
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
int fq4r_repair(uint8_t *data, size_t size);
#endif
