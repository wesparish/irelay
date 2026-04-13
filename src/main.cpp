#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
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

// LOG_ROLE is injected into every log entry prefix by log_buffer.h
#if NODE_ROLE == ROLE_RECEIVER
#define LOG_ROLE "receiver"
#elif NODE_ROLE == ROLE_EMITTER
#define LOG_ROLE "emitter"
#endif

#include "log_buffer.h"

// Pin assignments — hardwired on the ESP-01M IR transceiver module PCB
#define IR_RECV_PIN  14  // GPIO14: IR receiver (per manufacturer spec)
#define IR_SEND_PIN   4  // GPIO4:  IR transmitter (per manufacturer spec)

// Carrier frequency used for raw fallback — 38kHz is standard for most remotes
#define IR_CARRIER_FREQ_HZ 38000

#define STATUS_INTERVAL_MS 10000

static ESP8266WebServer webServer(80);
static DNSServer        dnsServer;

static void fmtUptime(char* buf, size_t size) {
    uint32_t ms = millis();
    uint32_t s  = ms / 1000;
    uint32_t d  = s / 86400; s %= 86400;
    uint32_t h  = s / 3600;  s %= 3600;
    uint32_t m  = s / 60;    s %= 60;
    snprintf(buf, size, "%02u:%02u:%02u:%02u", d, h, m, s);
}

static void printUptime() {
    char buf[16];
    fmtUptime(buf, sizeof(buf));
    Serial.print("uptime=");
    Serial.print(buf);
}

// Open an AP on ESPNOW_CHANNEL (so ESP-NOW still works on the same channel),
// start a captive DNS server, and start the HTTP server.
// roleLabel appears in the AP SSID, e.g. "IRelay-Receiver-9868".
static void startWebUI(const char* roleLabel) {
    uint8_t mac[6];
    wifi_get_macaddr(STATION_IF, mac);
    char ssid[32];
    snprintf(ssid, sizeof(ssid), "IRelay-%s-%02X%02X", roleLabel, mac[4], mac[5]);

    WiFi.softAP(ssid, nullptr, ESPNOW_CHANNEL);
    IPAddress apIp = WiFi.softAPIP();
    Serial.printf("web UI: SSID=%s  http://%d.%d.%d.%d/\n",
        ssid, apIp[0], apIp[1], apIp[2], apIp[3]);

    dnsServer.start(53, "*", apIp);
    webServer.onNotFound([]() {
        webServer.sendHeader("Location", "http://192.168.4.1/");
        webServer.send(302, "text/plain", "");
    });
    webServer.begin();
}

// ── Shared HTML helpers ────────────────────────────────────────────────────────
// All use chunked transfer (setContentLength UNKNOWN + sendContent) to avoid
// building the full page in a single heap allocation.

static void webSendHead(const char* role) {
    webServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
    webServer.send(200, "text/html; charset=utf-8", "");
    webServer.sendContent(F(
        "<!DOCTYPE html><html lang='en'><head>"
        "<meta charset='UTF-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<meta http-equiv='refresh' content='5'>"
        "<title>IRelay</title>"
        "<style>"
        ":root{"
          "--bg:#0d1117;--su:#161b22;--br:#30363d;"
          "--tx:#e6edf3;--dm:#8b949e;"
          "--ac:#58a6ff;--gn:#3fb950;--rd:#f85149;"
          "--fn:'SFMono-Regular',Consolas,monospace"
        "}"
        "*{box-sizing:border-box;margin:0;padding:0}"
        "body{"
          "background:var(--bg);color:var(--tx);"
          "font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;"
          "min-height:100vh;padding:16px 12px"
        "}"
        "header{"
          "display:flex;align-items:center;gap:10px;flex-wrap:wrap;"
          "margin-bottom:20px;padding-bottom:14px;border-bottom:1px solid var(--br)"
        "}"
        "h1{font-size:1.15rem;font-weight:600}"
        ".badge{"
          "padding:3px 10px;border-radius:20px;"
          "font-size:.7rem;font-weight:700;text-transform:uppercase;letter-spacing:.06em;"
          "background:#1f6feb33;color:var(--ac);border:1px solid #388bfd55"
        "}"
        ".hint{color:var(--dm);font-size:.72rem;margin-left:auto}"
        ".grid{display:grid;grid-template-columns:1fr;gap:14px}"
        "@media(min-width:600px){.grid{grid-template-columns:1fr 1fr}}"
        ".card{"
          "background:var(--su);border:1px solid var(--br);"
          "border-radius:8px;padding:14px 16px"
        "}"
        ".card h2{"
          "font-size:.68rem;font-weight:700;text-transform:uppercase;"
          "letter-spacing:.08em;color:var(--dm);margin-bottom:10px"
        "}"
        "table{width:100%;border-collapse:collapse}"
        "td{padding:6px 0;border-bottom:1px solid var(--br);font-size:.86rem;vertical-align:top}"
        "tr:last-child td{border-bottom:none}"
        "td:first-child{color:var(--dm);width:52%;padding-right:8px}"
        "td:last-child{color:var(--tx);font-family:var(--fn);font-size:.8rem;word-break:break-all}"
        ".ac{color:var(--ac)}.gn{color:var(--gn)}.rd{color:var(--rd)}"
        ".lc{grid-column:1/-1}"
        ".lh{display:flex;align-items:baseline;gap:8px;margin-bottom:8px}"
        ".lh h2{margin:0}"
        ".lt{color:var(--dm);font-size:.72rem}"
        ".lw{"
          "max-height:280px;overflow-y:auto;"
          "border:1px solid var(--br);border-radius:4px;padding:6px 8px"
        "}"
        ".le{"
          "font-family:var(--fn);font-size:.75rem;color:var(--dm);"
          "padding:2px 0;border-bottom:1px solid #1c1c1c;"
          "white-space:pre-wrap;word-break:break-all;line-height:1.45"
        "}"
        ".le:last-child{color:var(--tx);border-bottom:none}"
        "footer{margin-top:18px;text-align:center;color:var(--dm);font-size:.72rem}"
        "</style></head><body>"
        "<header><h1>IRelay</h1><span class='badge'>"
    ));
    webServer.sendContent(role);
    webServer.sendContent(F(
        "</span><span class='hint'>auto-refreshes every 5s</span>"
        "</header><div class='grid'>"
    ));
}

static void webSendLogCard() {
    char total[12];
    snprintf(total, sizeof(total), "%u", s_logTotal);

    webServer.sendContent(F("<div class='card lc'><div class='lh'><h2>Event log</h2><span class='lt'>"));
    webServer.sendContent(total);
    webServer.sendContent(F(" events</span></div><div class='lw'>"));

    uint32_t count = (s_logTotal < LOG_BUF_ENTRIES) ? s_logTotal : (uint32_t)LOG_BUF_ENTRIES;
    if (count == 0) {
        webServer.sendContent(F("<div class='le'>No events yet.</div>"));
    } else {
        uint8_t start = (s_logTotal >= LOG_BUF_ENTRIES) ? s_logHead : 0;
        for (uint32_t i = 0; i < count; i++) {
            uint8_t idx = (start + i) % LOG_BUF_ENTRIES;
            webServer.sendContent(F("<div class='le'>"));
            String esc;
            esc.reserve(LOG_BUF_ENTRY_LEN + 8);
            for (const char* p = s_logBuf[idx]; *p; p++) {
                if      (*p == '<') esc += F("&lt;");
                else if (*p == '>') esc += F("&gt;");
                else if (*p == '&') esc += F("&amp;");
                else                esc += *p;
            }
            webServer.sendContent(esc);
            webServer.sendContent(F("</div>"));
        }
    }
    webServer.sendContent(F("</div></div>"));
}

static void webSendFoot(const char* uptime) {
    webServer.sendContent(F("</div><footer>uptime: "));
    webServer.sendContent(uptime);
    webServer.sendContent(F("</footer></body></html>"));
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
        logPush("espnow: send ok");
    } else {
        sendFailCount++;
        Serial.println("ESP-NOW send failed");
        logPush("espnow: send failed");
    }
}

static void handleRoot() {
    char uptime[16];
    fmtUptime(uptime, sizeof(uptime));

    uint8_t mac[6];
    wifi_get_macaddr(STATION_IF, mac);
    char macStr[18], peerStr[18];
    snprintf(macStr,  sizeof(macStr),  "%02X:%02X:%02X:%02X:%02X:%02X",
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    snprintf(peerStr, sizeof(peerStr), "%02X:%02X:%02X:%02X:%02X:%02X",
        peerMac[0], peerMac[1], peerMac[2], peerMac[3], peerMac[4], peerMac[5]);

    webSendHead("Receiver");

    String s;
    s.reserve(300);
    s  = F("<div class='card'><h2>Status</h2><table>");
    s += F("<tr><td>MAC</td><td class='ac'>"); s += macStr; s += F("</td></tr>");
    s += F("<tr><td>Peer MAC</td><td class='ac'>"); s += peerStr; s += F("</td></tr>");
    s += F("<tr><td>Channel</td><td>"); s += (int)ESPNOW_CHANNEL; s += F("</td></tr>");
    s += F("</table></div>");
    webServer.sendContent(s);

    String c;
    c.reserve(320);
    c  = F("<div class='card'><h2>Counters</h2><table>");
    c += F("<tr><td>IR Protocol RX</td><td class='ac'>"); c += protocolRxCount; c += F("</td></tr>");
    c += F("<tr><td>IR Raw RX</td><td class='ac'>"); c += rawRxCount; c += F("</td></tr>");
    c += F("<tr><td>ESP-NOW sent OK</td><td class='gn'>"); c += sendOkCount; c += F("</td></tr>");
    c += F("<tr><td>ESP-NOW send fail</td><td class='rd'>"); c += sendFailCount; c += F("</td></tr>");
    c += F("</table></div>");
    webServer.sendContent(c);

    webSendLogCard();
    webSendFoot(uptime);
}

void setup() {
    Serial.begin(115200);

    WiFi.mode(WIFI_AP_STA);
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

    webServer.on("/", HTTP_GET, handleRoot);
    startWebUI("Receiver");

    Serial.println("receiver-node ready");
}

void loop() {
    dnsServer.processNextRequest();
    webServer.handleClient();

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
        Serial.printf("IR rx: protocol=%s bits=%u value=0x",
            typeToString(irResult.decode_type, irResult.repeat).c_str(),
            irResult.bits);
        Serial.print(irResult.value, HEX);
        Serial.println();

        char logMsg[LOG_BUF_ENTRY_LEN - LOG_PREFIX_LEN];
        snprintf(logMsg, sizeof(logMsg), "IR rx: %s %u-bit 0x%08X%08X",
            typeToString(irResult.decode_type, irResult.repeat).c_str(),
            irResult.bits,
            (uint32_t)(irResult.value >> 32), (uint32_t)irResult.value);
        logPush(logMsg);

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
        logPushf("IR rx: raw %u entries", count);

        len = ir_raw_pack(buf, IR_CARRIER_FREQ_HZ, timings, count);
    }

    int sendResult = esp_now_send(peerMac, buf, len);
    if (sendResult != 0) {
        Serial.printf("ESP-NOW send error (not queued): %d\n", sendResult);
        logPushf("espnow: queue error %d", sendResult);
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
        logPush("espnow rx: empty packet");
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
                logPushf("espnow rx: bad protocol pkt (len=%u)", len);
                badPktCount++;
                return;
            }
            protocolTxCount++;
            Serial.printf("ESP-NOW rx: protocol=%s bits=%u value=0x",
                typeToString(static_cast<decode_type_t>(protocol), false).c_str(),
                bits);
            Serial.print(value, HEX);
            Serial.println();

            char logMsg[LOG_BUF_ENTRY_LEN - LOG_PREFIX_LEN];
            snprintf(logMsg, sizeof(logMsg), "IR tx: %s %u-bit 0x%08X%08X",
                typeToString(static_cast<decode_type_t>(protocol), false).c_str(),
                bits,
                (uint32_t)(value >> 32), (uint32_t)value);
            logPush(logMsg);

            irSend.send(static_cast<decode_type_t>(protocol), value, bits);
            Serial.println("IR tx done");
            break;
        }
        case IR_PKT_RAW: {
            uint16_t freq, count;
            uint16_t timings[IR_RAW_MAX_ENTRIES];
            if (!ir_raw_unpack(data, len, &freq, &count, timings)) {
                Serial.printf("ESP-NOW rx: bad raw packet (len=%u)\n", len);
                logPushf("espnow rx: bad raw pkt (len=%u)", len);
                badPktCount++;
                return;
            }
            rawTxCount++;
            Serial.printf("ESP-NOW rx: %u raw timing entries @ %uHz\n", count, freq);
            logPushf("IR tx: raw %u entries @ %uHz", count, freq);
            irSend.sendRaw(timings, count, freq);
            Serial.println("IR tx done");
            break;
        }
        default:
            Serial.printf("ESP-NOW rx: unknown packet type 0x%02X\n", data[0]);
            logPushf("espnow rx: unknown type 0x%02X", data[0]);
            badPktCount++;
            break;
    }
}

static void handleRoot() {
    char uptime[16];
    fmtUptime(uptime, sizeof(uptime));

    uint8_t mac[6];
    wifi_get_macaddr(STATION_IF, mac);
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    webSendHead("Emitter");

    String s;
    s.reserve(220);
    s  = F("<div class='card'><h2>Status</h2><table>");
    s += F("<tr><td>MAC</td><td class='ac'>"); s += macStr; s += F("</td></tr>");
    s += F("<tr><td>Channel</td><td>"); s += (int)ESPNOW_CHANNEL; s += F("</td></tr>");
    s += F("</table></div>");
    webServer.sendContent(s);

    String c;
    c.reserve(280);
    c  = F("<div class='card'><h2>Counters</h2><table>");
    c += F("<tr><td>IR Protocol TX</td><td class='ac'>"); c += protocolTxCount; c += F("</td></tr>");
    c += F("<tr><td>IR Raw TX</td><td class='ac'>"); c += rawTxCount; c += F("</td></tr>");
    c += F("<tr><td>Bad packets</td><td class='rd'>"); c += badPktCount; c += F("</td></tr>");
    c += F("</table></div>");
    webServer.sendContent(c);

    webSendLogCard();
    webSendFoot(uptime);
}

void setup() {
    Serial.begin(115200);

    WiFi.mode(WIFI_AP_STA);
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

    webServer.on("/", HTTP_GET, handleRoot);
    startWebUI("Emitter");

    Serial.println("emitter-node ready");
}

void loop() {
    dnsServer.processNextRequest();
    webServer.handleClient();

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
