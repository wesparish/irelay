#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <espnow.h>
#include <IRrecv.h>
#include <IRsend.h>
#include <IRutils.h>
#include "config.h"
#include "ir_payload.h"

#define ROLE_MEDIA  1
#define ROLE_SERVER 2

#ifndef NODE_ROLE
#error "NODE_ROLE must be defined as ROLE_MEDIA or ROLE_SERVER via build_flags"
#endif

// Pin assignments — hardwired on the ESP-01M IR transceiver module PCB
#define IR_RECV_PIN  14  // GPIO14: IR receiver (per manufacturer spec)
#define IR_SEND_PIN   4  // GPIO4:  IR transmitter (per manufacturer spec)

// ── Media node (media room) ──────────────────────────────────────────────────
#if NODE_ROLE == ROLE_MEDIA

IRrecv irRecv(IR_RECV_PIN);
decode_results irResult;
static uint8_t peerMac[] = SERVER_NODE_MAC;

void setup() {
    Serial.begin(115200);

    WiFi.mode(WIFI_STA);
    wifi_set_channel(ESPNOW_CHANNEL);

    esp_now_init();
    esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
    esp_now_add_peer(peerMac, ESP_NOW_ROLE_SLAVE, ESPNOW_CHANNEL, NULL, 0);

    irRecv.enableIRIn();
    Serial.println("media-node ready");
}

void loop() {
    if (!irRecv.decode(&irResult)) return;

    IrPayload payload = {
        .protocol = static_cast<uint32_t>(irResult.decode_type),
        .bits     = irResult.bits,
        .value    = irResult.value,
    };

    esp_now_send(peerMac, reinterpret_cast<uint8_t*>(&payload), sizeof(payload));
    irRecv.resume();
}

// ── Server node (server room) ────────────────────────────────────────────────
#elif NODE_ROLE == ROLE_SERVER

IRsend irSend(IR_SEND_PIN);

void onReceive(uint8_t* /*mac*/, uint8_t* data, uint8_t len) {
    IrPayload payload;
    if (!ir_payload_decode(data, len, &payload)) return;
    irSend.send(static_cast<decode_type_t>(payload.protocol), payload.value, payload.bits);
}

void setup() {
    Serial.begin(115200);

    WiFi.mode(WIFI_STA);
    wifi_set_channel(ESPNOW_CHANNEL);

    esp_now_init();
    esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
    esp_now_register_recv_cb(onReceive);

    irSend.begin();
    Serial.println("server-node ready");
}

void loop() {
    // ESP-NOW recv callback drives everything; nothing to poll
}

#endif
