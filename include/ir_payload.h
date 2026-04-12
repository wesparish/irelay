#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

// Packet type discriminator — first byte of every ESP-NOW packet.
enum IrPacketType : uint8_t {
    IR_PKT_PROTOCOL = 0,
    IR_PKT_RAW      = 1,
};

// ── Protocol packet ──────────────────────────────────────────────────────────
// Wire layout (15 bytes):
//   [0]    type     uint8_t   = IR_PKT_PROTOCOL
//   [1-4]  protocol uint32_t  (IRremoteESP8266 decode_type_t cast to uint32_t)
//   [5-6]  bits     uint16_t
//   [7-14] value    uint64_t
//
// Used when IRremoteESP8266 successfully decodes the received signal.
// Both nodes must agree on this layout — never reorder or resize fields
// without bumping the packet type or adding a new type.

#define IR_PROTOCOL_PKT_SIZE 15u

inline void ir_protocol_pack(uint8_t* buf,
                              uint32_t protocol, uint16_t bits, uint64_t value) {
    buf[0] = IR_PKT_PROTOCOL;
    memcpy(buf + 1, &protocol, 4);
    memcpy(buf + 5, &bits,     2);
    memcpy(buf + 7, &value,    8);
}

// Returns false if len != IR_PROTOCOL_PKT_SIZE or type tag is wrong.
inline bool ir_protocol_unpack(const uint8_t* data, uint8_t len,
                                uint32_t* protocol, uint16_t* bits, uint64_t* value) {
    if (len != IR_PROTOCOL_PKT_SIZE || data[0] != IR_PKT_PROTOCOL) return false;
    memcpy(protocol, data + 1, 4);
    memcpy(bits,     data + 5, 2);
    memcpy(value,    data + 7, 8);
    return true;
}

// ── Raw packet ───────────────────────────────────────────────────────────────
// Wire layout (5 + count×2 bytes, max 249):
//   [0]   type    uint8_t   = IR_PKT_RAW
//   [1-2] freq    uint16_t  carrier frequency in Hz (typically 38000)
//   [3-4] count   uint16_t  number of timing entries
//   [5+]  timings uint16_t  mark/space durations in microseconds
//
// Used when the signal protocol is unknown. Timings are the raw capture
// from IRrecv (rawbuf[1..rawlen-1] × kRawTick), ready for IRsend::sendRaw().
// ESP-NOW caps payloads at 250 bytes → max (250 − 5) / 2 = 122 entries.

#define IR_RAW_HDR_SIZE    5u
#define IR_RAW_MAX_ENTRIES 122u
#define IR_RAW_MAX_PKT_SIZE (IR_RAW_HDR_SIZE + IR_RAW_MAX_ENTRIES * 2u)  // 249 bytes

inline size_t ir_raw_pkt_size(uint16_t count) {
    return IR_RAW_HDR_SIZE + (size_t)count * 2;
}

inline size_t ir_raw_pack(uint8_t* buf,
                           uint16_t freq, const uint16_t* timings, uint16_t count) {
    buf[0] = IR_PKT_RAW;
    memcpy(buf + 1, &freq,   2);
    memcpy(buf + 3, &count,  2);
    memcpy(buf + 5, timings, count * 2);
    return ir_raw_pkt_size(count);
}

// Returns false if type tag is wrong or len doesn't match the declared count.
// timings_out must point to a buffer of at least IR_RAW_MAX_ENTRIES uint16_ts.
inline bool ir_raw_unpack(const uint8_t* data, uint8_t len,
                           uint16_t* freq, uint16_t* count, uint16_t* timings_out) {
    if (len < IR_RAW_HDR_SIZE || data[0] != IR_PKT_RAW) return false;
    memcpy(freq,  data + 1, 2);
    memcpy(count, data + 3, 2);
    if (ir_raw_pkt_size(*count) != len) return false;
    memcpy(timings_out, data + 5, *count * 2);
    return true;
}
