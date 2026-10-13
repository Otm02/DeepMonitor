#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#include <cstring>
#include <cstddef>

#include "rtc.h"

/**
 * Property 9: CRC-16 Round-Trip Validation
 *
 * For any valid RtcBlock, computing the CRC-16 and storing it, then calling
 * validate, should return true. For any valid RtcBlock with one or more bits
 * flipped in the data (but not the CRC field), validate should return false.
 *
 * Validates: Requirements 6.3
 */

// Fill an RtcBlock with random bytes (except crc16) using RapidCheck generators
static rtc::RtcBlock generateRandomBlock() {
    rtc::RtcBlock block;
    // Fill the CRC-covered portion (everything before crc16) with random bytes.
    const size_t data_len = offsetof(rtc::RtcBlock, crc16);
    auto* bytes = reinterpret_cast<uint8_t*>(&block);
    for (size_t i = 0; i < data_len; i++) {
        bytes[i] = *rc::gen::inRange(0, 256);
    }
    block.crc16 = 0;
    return block;
}

/**
 * **Validates: Requirements 6.3**
 *
 * Property: After calling rtc::save() to compute and store CRC-16,
 * rtc::validate() returns true for any random RtcBlock data.
 */
RC_GTEST_PROP(CRC16RoundTrip, SaveThenValidateReturnsTrue, ()) {
    rtc::RtcBlock block = generateRandomBlock();

    rtc::save(block);
    RC_ASSERT(rtc::validate(block));
}

/**
 * **Validates: Requirements 6.3**
 *
 * Property: After calling rtc::save(), flipping one or more random bits
 * in the data portion (not the crc16 field) causes rtc::validate() to
 * return false.
 */
RC_GTEST_PROP(CRC16RoundTrip, FlippedBitsDetectedAsCorruption, ()) {
    rtc::RtcBlock block = generateRandomBlock();
    rtc::save(block);

    // Data portion size (everything before crc16)
    const size_t data_len = offsetof(rtc::RtcBlock, crc16);
    RC_PRE(data_len > 0);

    // Pick a random byte index in the data portion and a random bit to flip
    const auto byte_idx = *rc::gen::inRange<size_t>(0, data_len);
    const auto bit_idx = *rc::gen::inRange(0, 8);

    auto* bytes = reinterpret_cast<uint8_t*>(&block);
    bytes[byte_idx] ^= static_cast<uint8_t>(1 << bit_idx);

    RC_ASSERT(!rtc::validate(block));
}
