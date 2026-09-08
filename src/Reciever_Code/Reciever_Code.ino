#include <WiFi.h>
#include <esp_now.h>

// -------------------
// DATA STRUCTURE
// -------------------
typedef struct struct_message {
    int id;
} struct_message;

struct_message incomingData;

// -------------------
// RSSI / Distance Parameters
// -------------------
float A = -55;   // RSSI at 1 meter (calibrate)
float n = 2.9;   // path-loss exponent

int rssiDevice1 = 0;
int rssiDevice2 = 0;
int rssiDevice3 = 0;
float distDevice1 = 0;
float distDevice2 = 0;
float distDevice3 = 0;

// -------------------
// Distance calculation
// -------------------
float calcDist(int rssi){
    return pow(10, (A - rssi)/(10*n));
}

// -------------------
// Callback (NEW ESP-NOW signature)
// -------------------
void OnDataRecv(const esp_now_recv_info_t *recvInfo, const uint8_t *incomingDataRaw, int len) {
    memcpy(&incomingData, incomingDataRaw, sizeof(incomingData));

    // RSSI is now inside recvInfo->rx_ctrl
    int rssi = recvInfo->rx_ctrl->rssi;

    if(incomingData.id == 1){
        rssiDevice1 = rssi;
        distDevice1 = calcDist(rssi);
    } else if(incomingData.id == 2){
        rssiDevice2 = rssi;
        distDevice2 = calcDist(rssi);
    } else if(incomingData.id == 3){
        rssiDevice3 = rssi;
        distDevice3 = calcDist(rssi);
    }

    // Print latest distances
    Serial.println("----- Live Distance Update -----");
    Serial.print("Device 1 | RSSI: "); Serial.print(rssiDevice1);
    Serial.print(" | Distance: "); Serial.println(distDevice1);

    Serial.print("Device 2 | RSSI: "); Serial.print(rssiDevice2);
    Serial.print(" | Distance: "); Serial.println(distDevice2);

    Serial.print("Device 3 | RSSI: "); Serial.print(rssiDevice3);
    Serial.print(" | Distance: "); Serial.println(distDevice3);
    Serial.println("--------------------------------");
}

// -------------------
// Setup
// -------------------
void setup() {
    Serial.begin(115200);
    WiFi.mode(WIFI_STA);

    if(esp_now_init() != ESP_OK){
        Serial.println("ESP-NOW init failed!");
        return;
    }

    esp_now_register_recv_cb(OnDataRecv); // NEW callback signature
    Serial.println("Receiver ready...");
}

// -------------------
// Loop
// -------------------
void loop() {
    // nothing needed here, callback handles packets
}