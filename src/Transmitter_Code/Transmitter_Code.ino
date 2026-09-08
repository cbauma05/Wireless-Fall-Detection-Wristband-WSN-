#include <esp_now.h>
#include <WiFi.h>

#define DEVICE_ID 3

uint8_t receiverAddress[] = {0x24, 0xdC, 0xC3, 0x46, 0xC2, 0x90};

typedef struct {
    int id;
} DataPacket;

DataPacket myData;

// NEW callback signature (IDF 5.x)
void onSent(const wifi_tx_info_t *txInfo, esp_now_send_status_t status) {
    Serial.print("Send Status: ");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

void setup() {
    Serial.begin(115200);

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW Init Failed");
        return;
    }

    esp_now_register_send_cb(onSent);

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, receiverAddress, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Failed to add peer");
        return;
    }

    Serial.println("Transmitter ");
    Serial.print(DEVICE_ID);
    Serial.println(" ready.");
}

void loop() {
    myData.id = DEVICE_ID;
    esp_err_t result = esp_now_send(receiverAddress, (uint8_t*)&myData, sizeof(myData));
    if (result == ESP_OK) {
        Serial.println("Packet sent");
    } else {
        Serial.println("Send failed");
    }

    delay(500);  // send 2× per second
}