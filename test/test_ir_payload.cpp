#include <unity.h>
#include "ir_payload.h"

void setUp(void) {}
void tearDown(void) {}

// ── Protocol packet ──────────────────────────────────────────────────────────

void test_protocol_pack_sets_type_tag(void) {
    uint8_t buf[IR_PROTOCOL_PKT_SIZE];
    ir_protocol_pack(buf, 3, 32, 0x20DF10EFul);
    TEST_ASSERT_EQUAL_UINT8(IR_PKT_PROTOCOL, buf[0]);
}

void test_protocol_pkt_size_is_15(void) {
    TEST_ASSERT_EQUAL(15u, IR_PROTOCOL_PKT_SIZE);
}

void test_protocol_roundtrip_zeros(void) {
    uint8_t buf[IR_PROTOCOL_PKT_SIZE];
    ir_protocol_pack(buf, 0, 0, 0);

    uint32_t protocol; uint16_t bits; uint64_t value;
    TEST_ASSERT_TRUE(ir_protocol_unpack(buf, IR_PROTOCOL_PKT_SIZE, &protocol, &bits, &value));
    TEST_ASSERT_EQUAL_UINT32(0, protocol);
    TEST_ASSERT_EQUAL_UINT16(0, bits);
    TEST_ASSERT_EQUAL_UINT64(0, value);
}

void test_protocol_roundtrip_max_values(void) {
    uint8_t buf[IR_PROTOCOL_PKT_SIZE];
    ir_protocol_pack(buf, UINT32_MAX, UINT16_MAX, UINT64_MAX);

    uint32_t protocol; uint16_t bits; uint64_t value;
    TEST_ASSERT_TRUE(ir_protocol_unpack(buf, IR_PROTOCOL_PKT_SIZE, &protocol, &bits, &value));
    TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, protocol);
    TEST_ASSERT_EQUAL_UINT16(UINT16_MAX, bits);
    TEST_ASSERT_EQUAL_UINT64(UINT64_MAX, value);
}

void test_protocol_roundtrip_nec(void) {
    uint8_t buf[IR_PROTOCOL_PKT_SIZE];
    ir_protocol_pack(buf, 3, 32, 0x20DF10EFul);

    uint32_t protocol; uint16_t bits; uint64_t value;
    TEST_ASSERT_TRUE(ir_protocol_unpack(buf, IR_PROTOCOL_PKT_SIZE, &protocol, &bits, &value));
    TEST_ASSERT_EQUAL_UINT32(3,            protocol);
    TEST_ASSERT_EQUAL_UINT16(32,           bits);
    TEST_ASSERT_EQUAL_UINT64(0x20DF10EFul, value);
}

void test_protocol_unpack_rejects_wrong_type_tag(void) {
    uint8_t buf[IR_PROTOCOL_PKT_SIZE] = {};
    buf[0] = IR_PKT_RAW;
    uint32_t protocol; uint16_t bits; uint64_t value;
    TEST_ASSERT_FALSE(ir_protocol_unpack(buf, IR_PROTOCOL_PKT_SIZE, &protocol, &bits, &value));
}

void test_protocol_unpack_rejects_wrong_length(void) {
    uint8_t buf[IR_PROTOCOL_PKT_SIZE + 1] = {};
    buf[0] = IR_PKT_PROTOCOL;
    uint32_t protocol; uint16_t bits; uint64_t value;
    TEST_ASSERT_FALSE(ir_protocol_unpack(buf, IR_PROTOCOL_PKT_SIZE - 1, &protocol, &bits, &value));
    TEST_ASSERT_FALSE(ir_protocol_unpack(buf, IR_PROTOCOL_PKT_SIZE + 1, &protocol, &bits, &value));
}

// ── Raw packet ───────────────────────────────────────────────────────────────

void test_raw_pkt_size_formula(void) {
    TEST_ASSERT_EQUAL(5u,   ir_raw_pkt_size(0));
    TEST_ASSERT_EQUAL(7u,   ir_raw_pkt_size(1));
    TEST_ASSERT_EQUAL(249u, ir_raw_pkt_size(IR_RAW_MAX_ENTRIES));
}

void test_raw_max_fits_espnow(void) {
    TEST_ASSERT_TRUE(IR_RAW_MAX_PKT_SIZE <= 250u);
}

void test_raw_roundtrip_single_entry(void) {
    uint16_t timings_in[] = {9000};
    uint8_t buf[IR_RAW_MAX_PKT_SIZE];
    size_t len = ir_raw_pack(buf, 38000, timings_in, 1);

    TEST_ASSERT_EQUAL(7u, len);
    TEST_ASSERT_EQUAL_UINT8(IR_PKT_RAW, buf[0]);

    uint16_t freq, count, timings_out[IR_RAW_MAX_ENTRIES];
    TEST_ASSERT_TRUE(ir_raw_unpack(buf, (uint8_t)len, &freq, &count, timings_out));
    TEST_ASSERT_EQUAL_UINT16(38000, freq);
    TEST_ASSERT_EQUAL_UINT16(1,     count);
    TEST_ASSERT_EQUAL_UINT16(9000,  timings_out[0]);
}

void test_raw_roundtrip_nec_like(void) {
    uint16_t timings_in[] = {9000, 4500, 560, 1690, 560, 560, 560, 1690};
    const uint16_t count = 8;
    uint8_t buf[IR_RAW_MAX_PKT_SIZE];
    size_t len = ir_raw_pack(buf, 38000, timings_in, count);

    TEST_ASSERT_EQUAL(5u + count * 2u, len);

    uint16_t freq, out_count, timings_out[IR_RAW_MAX_ENTRIES];
    TEST_ASSERT_TRUE(ir_raw_unpack(buf, (uint8_t)len, &freq, &out_count, timings_out));
    TEST_ASSERT_EQUAL_UINT16(38000, freq);
    TEST_ASSERT_EQUAL_UINT16(count, out_count);
    for (uint16_t i = 0; i < count; i++) {
        TEST_ASSERT_EQUAL_UINT16(timings_in[i], timings_out[i]);
    }
}

void test_raw_roundtrip_max_entries(void) {
    uint16_t timings_in[IR_RAW_MAX_ENTRIES];
    for (uint16_t i = 0; i < IR_RAW_MAX_ENTRIES; i++) timings_in[i] = i * 10 + 100;

    uint8_t buf[IR_RAW_MAX_PKT_SIZE];
    size_t len = ir_raw_pack(buf, 38000, timings_in, IR_RAW_MAX_ENTRIES);

    TEST_ASSERT_EQUAL(IR_RAW_MAX_PKT_SIZE, len);

    uint16_t freq, count, timings_out[IR_RAW_MAX_ENTRIES];
    TEST_ASSERT_TRUE(ir_raw_unpack(buf, (uint8_t)len, &freq, &count, timings_out));
    TEST_ASSERT_EQUAL_UINT16(IR_RAW_MAX_ENTRIES, count);
    for (uint16_t i = 0; i < IR_RAW_MAX_ENTRIES; i++) {
        TEST_ASSERT_EQUAL_UINT16(timings_in[i], timings_out[i]);
    }
}

void test_raw_unpack_rejects_wrong_type_tag(void) {
    uint8_t buf[IR_PROTOCOL_PKT_SIZE];
    ir_protocol_pack(buf, 3, 32, 0x20DF10EFul);  // type = IR_PKT_PROTOCOL
    uint16_t freq, count, timings_out[IR_RAW_MAX_ENTRIES];
    TEST_ASSERT_FALSE(ir_raw_unpack(buf, IR_PROTOCOL_PKT_SIZE, &freq, &count, timings_out));
}

void test_raw_unpack_rejects_length_mismatch(void) {
    // Pack count=2 (9 bytes), then claim it's 7 bytes (count=1 length)
    uint16_t timings[] = {9000, 4500};
    uint8_t buf[IR_RAW_MAX_PKT_SIZE];
    ir_raw_pack(buf, 38000, timings, 2);
    uint16_t freq, count, timings_out[IR_RAW_MAX_ENTRIES];
    TEST_ASSERT_FALSE(ir_raw_unpack(buf, 7, &freq, &count, timings_out));
}

void test_raw_unpack_rejects_too_short(void) {
    uint8_t buf[4] = {IR_PKT_RAW, 0, 0, 0};
    uint16_t freq, count, timings_out[IR_RAW_MAX_ENTRIES];
    TEST_ASSERT_FALSE(ir_raw_unpack(buf, 4, &freq, &count, timings_out));  // 4 < IR_RAW_HDR_SIZE
}

// ── Type tags are distinct ────────────────────────────────────────────────────

void test_type_tags_differ(void) {
    TEST_ASSERT_NOT_EQUAL(IR_PKT_PROTOCOL, IR_PKT_RAW);
}

// ────────────────────────────────────────────────────────────────────────────

int main(int argc, char** argv) {
    UNITY_BEGIN();

    RUN_TEST(test_protocol_pack_sets_type_tag);
    RUN_TEST(test_protocol_pkt_size_is_15);
    RUN_TEST(test_protocol_roundtrip_zeros);
    RUN_TEST(test_protocol_roundtrip_max_values);
    RUN_TEST(test_protocol_roundtrip_nec);
    RUN_TEST(test_protocol_unpack_rejects_wrong_type_tag);
    RUN_TEST(test_protocol_unpack_rejects_wrong_length);

    RUN_TEST(test_raw_pkt_size_formula);
    RUN_TEST(test_raw_max_fits_espnow);
    RUN_TEST(test_raw_roundtrip_single_entry);
    RUN_TEST(test_raw_roundtrip_nec_like);
    RUN_TEST(test_raw_roundtrip_max_entries);
    RUN_TEST(test_raw_unpack_rejects_wrong_type_tag);
    RUN_TEST(test_raw_unpack_rejects_length_mismatch);
    RUN_TEST(test_raw_unpack_rejects_too_short);

    RUN_TEST(test_type_tags_differ);

    return UNITY_END();
}
