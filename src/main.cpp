#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <espnow.h>
#include <IRrecv.h>
#include <IRsend.h>
#include <IRutils.h>
#include "config.h"
#include "ir_payload.h"

#define ROLE_RECEIVER  1
#define ROLE_EMITTER   2

#ifndef NODE_ROLE
#error "NODE_ROLE must be defined as ROLE_RECEIVER or ROLE_EMITTER via build_flags"
#endif

// Pin assignments — hardwired on the ESP-01M IR transceiver module PCB
#define IR_RECV_PIN  14  // GPIO14: IR receiver (per manufacturer spec)
#define IR_SEND_PIN   4  // GPIO4:  IR transmitter (per manufacturer spec)

// Carrier frequency used for raw fallback — 38kHz is standard for most remotes
#define IR_CARRIER_FREQ_HZ 38000

#define STATUS_INTERVAL_MS 10000

static void printUptime() {
    uint32_t ms = millis();
    uint32_t s  = ms / 1000;
    uint32_t d  = s / 86400; s %= 86400;
    uint32_t h  = s / 3600;  s %= 3600;
    uint32_t m  = s / 60;    s %= 60;
    Serial.printf("uptime=%02u:%02u:%02u:%02u", d, h, m, s);
}

// ── Receiver node ─────────────────────────────────────────────────────────────
#if NODE_ROLE == ROLE_RECEIVER

IRrecv irRecv(IR_RECV_PIN);
decode_results irResult;
static uint8_t peerMac[] = EMITTER_NODE_MAC;

static uint32_t protocolRxCount = 0;
static uint32_t rawRxCount      = 0;
static uint32_t sendOkCount     = 0;
static uint32_t sendFailCount   = 0;

void onSendStatus(uint8_t* /*mac*/, uint8_t status) {
    if (status == 0) {
        sendOkCount++;
        Serial.println("ESP-NOW send ok");
    } else {
        sendFailCount++;
        Serial.println("ESP-NOW send failed");
    }
}

void setup() {
    Serial.begin(115200);

    WiFi.mode(WIFI_STA);
    wifi_set_channel(ESPNOW_CHANNEL);

    int initResult = esp_now_init();
    if (initResult != 0) {
        Serial.printf("ESP-NOW init failed: %d\n", initResult);
    } else {
        Serial.println("ESP-NOW init ok");
    }

    esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
    esp_now_register_send_cb(onSendStatus);

    Serial.printf("Peer MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
        peerMac[0], peerMac[1], peerMac[2], peerMac[3], peerMac[4], peerMac[5]);
    int peerResult = esp_now_add_peer(peerMac, ESP_NOW_ROLE_SLAVE, ESPNOW_CHANNEL, NULL, 0);
    if (peerResult != 0) {
        Serial.printf("ESP-NOW add peer failed: %d\n", peerResult);
    } else {
        Serial.printf("ESP-NOW peer added (channel %d)\n", ESPNOW_CHANNEL);
    }

    irRecv.enableIRIn();
    Serial.println("receiver-node ready");
}

void loop() {
    static uint32_t lastStatus = 0;
    uint32_t now = millis();
    if (now - lastStatus >= STATUS_INTERVAL_MS) {
        uint8_t mac[6];
        wifi_get_macaddr(STATION_IF, mac);
        printUptime();
        Serial.printf(" | mac=%02X:%02X:%02X:%02X:%02X:%02X | peer_mac=%02X:%02X:%02X:%02X:%02X:%02X | ch=%d | protocol_rx=%u | raw_rx=%u | espnow_ok=%u | espnow_fail=%u\n",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
            peerMac[0], peerMac[1], peerMac[2], peerMac[3], peerMac[4], peerMac[5],
            ESPNOW_CHANNEL, protocolRxCount, rawRxCount, sendOkCount, sendFailCount);
        lastStatus = now;
    }

    if (!irRecv.decode(&irResult)) return;

    uint8_t buf[250];
    size_t len;

    if (irResult.decode_type != UNKNOWN) {
        protocolRxCount++;
        // Known protocol — send compact decoded payload
        Serial.printf("IR rx: protocol=%s bits=%u value=0x",
            typeToString(irResult.decode_type, irResult.repeat).c_str(),
            irResult.bits);
        Serial.print(irResult.value, HEX);
        Serial.println();

        ir_protocol_pack(buf,
            static_cast<uint32_t>(irResult.decode_type),
            irResult.bits,
            irResult.value);
        len = IR_PROTOCOL_PKT_SIZE;
    } else {
        // Unknown protocol — fall back to raw timings
        // rawbuf[0] is the pre-signal gap; real timings start at index 1
        uint16_t count = irResult.rawlen - 1;

        if (count > IR_RAW_MAX_ENTRIES) {
            Serial.printf("IR rx: unknown protocol, signal too long (%u entries > %u max), skipping\n",
                count, IR_RAW_MAX_ENTRIES);
            irRecv.resume();
            return;
        }

        rawRxCount++;
        uint16_t timings[IR_RAW_MAX_ENTRIES];
        for (uint16_t i = 0; i < count; i++) {
            timings[i] = irResult.rawbuf[i + 1] * kRawTick;
        }

        Serial.printf("IR rx: unknown protocol, forwarding %u raw timing entries\n", count);
        len = ir_raw_pack(buf, IR_CARRIER_FREQ_HZ, timings, count);
    }

    int sendResult = esp_now_send(peerMac, buf, len);
    if (sendResult != 0) {
        Serial.printf("ESP-NOW send error (not queued): %d\n", sendResult);
    }
    irRecv.resume();
}

// ── Emitter node ──────────────────────────────────────────────────────────────
#elif NODE_ROLE == ROLE_EMITTER

IRsend irSend(IR_SEND_PIN);

static uint32_t protocolTxCount = 0;
static uint32_t rawTxCount      = 0;
static uint32_t badPktCount     = 0;

void onReceive(uint8_t* /*mac*/, uint8_t* data, uint8_t len) {
    if (len == 0) {
        Serial.println("ESP-NOW rx: empty packet");
        badPktCount++;
        return;
    }

    switch (data[0]) {
        case IR_PKT_PROTOCOL: {
            uint32_t protocol;
            uint16_t bits;
            uint64_t value;
            if (!ir_protocol_unpack(data, len, &protocol, &bits, &value)) {
                Serial.printf("ESP-NOW rx: bad protocol packet (len=%u)\n", len);
                badPktCount++;
                return;
            }
            protocolTxCount++;
            Serial.printf("ESP-NOW rx: protocol=%s bits=%u value=0x",
                typeToString(static_cast<decode_type_t>(protocol), false).c_str(),
                bits);
            Serial.print(value, HEX);
            Serial.println();
            irSend.send(static_cast<decode_type_t>(protocol), value, bits);
            Serial.println("IR tx done");
            break;
        }
        case IR_PKT_RAW: {
            uint16_t freq, count;
            uint16_t timings[IR_RAW_MAX_ENTRIES];
            if (!ir_raw_unpack(data, len, &freq, &count, timings)) {
                Serial.printf("ESP-NOW rx: bad raw packet (len=%u)\n", len);
                badPktCount++;
                return;
            }
            rawTxCount++;
            Serial.printf("ESP-NOW rx: %u raw timing entries @ %uHz\n", count, freq);
            irSend.sendRaw(timings, count, freq);
            Serial.println("IR tx done");
            break;
        }
        default:
            Serial.printf("ESP-NOW rx: unknown packet type 0x%02X\n", data[0]);
            badPktCount++;
            break;
    }
}

void setup() {
    Serial.begin(115200);

    WiFi.mode(WIFI_STA);
    wifi_set_channel(ESPNOW_CHANNEL);

    irSend.begin();  // initialize before registering callback to avoid send on uninitialized IRsend

    int initResult = esp_now_init();
    if (initResult != 0) {
        Serial.printf("ESP-NOW init failed: %d\n", initResult);
    } else {
        Serial.println("ESP-NOW init ok");
    }

    esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
    esp_now_register_recv_cb(onReceive);
    Serial.println("emitter-node ready");
}

void loop() {
    static uint32_t lastStatus = 0;
    uint32_t now = millis();
    if (now - lastStatus >= STATUS_INTERVAL_MS) {
        uint8_t mac[6];
        wifi_get_macaddr(STATION_IF, mac);
        printUptime();
        Serial.printf(" | mac=%02X:%02X:%02X:%02X:%02X:%02X | ch=%d | protocol_tx=%u | raw_tx=%u | bad_pkt=%u\n",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
            ESPNOW_CHANNEL, protocolTxCount, rawTxCount, badPktCount);
        lastStatus = now;
    }
}

#endif
