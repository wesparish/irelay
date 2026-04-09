#include <unity.h>
#include "ir_payload.h"

void setUp(void) {}
void tearDown(void) {}

// ── Struct layout ────────────────────────────────────────────────────────────

void test_sizeof_is_16(void) {
    // uint32_t(4) + uint16_t(2) + 2-byte padding + uint64_t(8) = 16
    // Both nodes must agree on this size; a change here breaks the wire format.
    TEST_ASSERT_EQUAL(16u, sizeof(IrPayload));
}

void test_field_offsets(void) {
    TEST_ASSERT_EQUAL(0u, offsetof(IrPayload, protocol));
    TEST_ASSERT_EQUAL(4u, offsetof(IrPayload, bits));
    TEST_ASSERT_EQUAL(8u, offsetof(IrPayload, value));
}

// ── ir_payload_decode: length guard ─────────────────────────────────────────

void test_decode_rejects_empty(void) {
    IrPayload out;
    uint8_t buf[16] = {};
    TEST_ASSERT_FALSE(ir_payload_decode(buf, 0, &out));
}

void test_decode_rejects_one_byte(void) {
    IrPayload out;
    uint8_t buf[16] = {};
    TEST_ASSERT_FALSE(ir_payload_decode(buf, 1, &out));
}

void test_decode_rejects_short(void) {
    IrPayload out;
    uint8_t buf[16] = {};
    TEST_ASSERT_FALSE(ir_payload_decode(buf, sizeof(IrPayload) - 1, &out));
}

void test_decode_rejects_long(void) {
    IrPayload out;
    uint8_t buf[32] = {};
    TEST_ASSERT_FALSE(ir_payload_decode(buf, sizeof(IrPayload) + 1, &out));
}

void test_decode_accepts_exact(void) {
    IrPayload out;
    uint8_t buf[sizeof(IrPayload)] = {};
    TEST_ASSERT_TRUE(ir_payload_decode(buf, sizeof(IrPayload), &out));
}

// ── Encode / decode roundtrip ────────────────────────────────────────────────

static void roundtrip(uint32_t protocol, uint16_t bits, uint64_t value) {
    IrPayload in  = { protocol, bits, value };
    IrPayload out = {};
    uint8_t buf[sizeof(IrPayload)];

    ir_payload_encode(&in, buf);
    TEST_ASSERT_TRUE(ir_payload_decode(buf, sizeof(IrPayload), &out));
    TEST_ASSERT_EQUAL_UINT32(protocol, out.protocol);
    TEST_ASSERT_EQUAL_UINT16(bits,     out.bits);
    TEST_ASSERT_EQUAL_UINT64(value,    out.value);
}

void test_roundtrip_zeros(void) {
    roundtrip(0, 0, 0);
}

void test_roundtrip_max_values(void) {
    roundtrip(UINT32_MAX, UINT16_MAX, UINT64_MAX);
}

void test_roundtrip_nec_protocol(void) {
    // NEC protocol = 3 in IRremoteESP8266; 32-bit code; realistic remote value
    roundtrip(3, 32, 0x20DF10EFul);
}

void test_roundtrip_sony_protocol(void) {
    // SONY = 12 in IRremoteESP8266; 12-bit code
    roundtrip(12, 12, 0xA90ul);
}

void test_roundtrip_preserves_all_fields_independently(void) {
    // Vary each field while holding others constant to check for cross-field clobber
    roundtrip(0xDEADBEEFul, 0x0000u,  0x0000000000000000ull);
    roundtrip(0x00000000ul, 0xFFFFu,  0x0000000000000000ull);
    roundtrip(0x00000000ul, 0x0000u,  0xCAFEBABEDEADF00Dull);
}

// ────────────────────────────────────────────────────────────────────────────

int main(int argc, char** argv) {
    UNITY_BEGIN();

    RUN_TEST(test_sizeof_is_16);
    RUN_TEST(test_field_offsets);

    RUN_TEST(test_decode_rejects_empty);
    RUN_TEST(test_decode_rejects_one_byte);
    RUN_TEST(test_decode_rejects_short);
    RUN_TEST(test_decode_rejects_long);
    RUN_TEST(test_decode_accepts_exact);

    RUN_TEST(test_roundtrip_zeros);
    RUN_TEST(test_roundtrip_max_values);
    RUN_TEST(test_roundtrip_nec_protocol);
    RUN_TEST(test_roundtrip_sony_protocol);
    RUN_TEST(test_roundtrip_preserves_all_fields_independently);

    return UNITY_END();
}
