#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <math.h>
#include "pitches.h"
#include <WiFi.h>
#include <esp_now.h>


// -------------------
// DATA STRUCTURE - Receiving
// -------------------
typedef struct struct_message {
    int id;
} struct_message;
struct_message incomingData;

// ------------------
// DATA STRUCTURE
// ------------------
typedef struct {
    int id;
    float distance;
    char location[50];
} DataPacket;
DataPacket myData;

// -------------------
// RSSI / Distance Parameters
// -------------------
float A = -55;   // RSSI at 1 meter (calibrate)
float n = 2.0;   // path-loss exponent

int rssiDevice1 = 0;
int rssiDevice2 = 0;
int rssiDevice3 = 0;
float distDevice1 = 0;
float distDevice2 = 0;
float distDevice3 = 0;
float final_distance = 0;
char final_location[50];


Adafruit_MPU6050 mpu;
#define BUFFER_SIZE 20 // number of samples to store for one comparisson
#define INIT_MOTION_THRESH 22.58 // acceleration threshold to determine if "fall" has started (10.58 + n)
#define INIT_ROTATION_THRESH 6 // gyroscope threshold to determine if "fall" has started
#define IMPACT_THRESH 28.58 // threshold for impact of fall in terms of acceleration
#define IDLE_THRESH 11.5 // resting value read from sensor + some threshold (10.58 + n)
#define BUZZER_PIN 23
#define DEVICE_ID 4

float accelBuffer[BUFFER_SIZE];
float gyroBuffer[BUFFER_SIZE];
int bufferIndex = 0;

float lastMaxAccel = 0;
float lastMaxGyro = 0;

bool accel_motion_detected;
bool gyro_motion_detected;
bool fall_started;
bool impact_detected;

float motion_time;
float impact_time;

// uint8_t BaseStationAddress[] = {0xb0, 0xa7, 0x32, 0x2b, 0x27, 0x64};
uint8_t BaseStationAddress[] = {0x24,0xdc,0xc3,0x45,0x75,0x8c};

int melody[] = {
  NOTE_C4, NOTE_G3, NOTE_G3, NOTE_A3, NOTE_G3, 0, NOTE_B3, NOTE_C4
};

int noteDurations[] = {
  4, 8, 8, 4, 4, 4, 4, 4
};


void addToBuffer(float accel_value, float gyro_value){
  accelBuffer[bufferIndex] = abs(accel_value);
  gyroBuffer[bufferIndex] = abs(gyro_value);
  bufferIndex++;

  if(bufferIndex >= BUFFER_SIZE){
    bufferIndex = 0;
  }
}

float get_max_accel(){
  float max_accel = accelBuffer[0];
  for (int i = 0; i < BUFFER_SIZE; i++){
    if (accelBuffer[i] > max_accel){
      max_accel = accelBuffer[i];
    }
  }
  return max_accel;
}

float get_max_gyro(){
  float max_gyro = gyroBuffer[0];
  for (int i = 0; i < BUFFER_SIZE; i++){
    if (gyroBuffer[i] > max_gyro){
      max_gyro = gyroBuffer[i];
    }
  }
  return max_gyro;
}

// void reset_buffers(){
//   for (int i = 0; i < BUFFER_SIZE; i++){
//     accelBuffer[i] = 0;
//     gyroBuffer[i] = 0;
//   }
//   bufferIndex = 0;
// }

void reset_state(){
  fall_started = false;
  impact_detected = false;
  accel_motion_detected = false;
  gyro_motion_detected = false;
}

void buzzAlert() {
  for (int i = 0; i < 8; i++) {
    tone(BUZZER_PIN, 1200);
    delay(120);
    noTone(BUZZER_PIN);
    delay(80);
  }
}

void buzzAcknowledgement() {
  // Soft, friendly 3-note rising chime
  
  tone(BUZZER_PIN, 784, 120);   // G5
  delay(150);

  tone(BUZZER_PIN, 988, 150);   // B5
  delay(180);

  tone(BUZZER_PIN, 1175, 220);  // D6
  delay(250);

  noTone(BUZZER_PIN);
}


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
    } else if(incomingData.id == 4){
        buzzAcknowledgement();
    }

}

// NEW callback signature (IDF 5.x)
void onSent(const wifi_tx_info_t *txInfo, esp_now_send_status_t status) {
    Serial.print("Send Status: ");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}


void sendFallAlert() {
    myData.id = DEVICE_ID;  // fill data packet
    myData.distance = final_distance;
   // strcpy(myData.location, "LR");

    const int attempts = 3; // number of retries
    const int delayMs = 200; // delay between retries

    Serial.println(myData.distance);
    Serial.println(myData.location);

    for (int i = 0; i < attempts; i++) {
        esp_err_t result = esp_now_send(BaseStationAddress, (uint8_t*)&myData, sizeof(myData));

        if (result == ESP_OK) {
            Serial.print("Packet sent successfully (attempt ");
            Serial.print(i+1);
            Serial.println(")");
            break; // exit loop if success
        } else {
            Serial.print("Send failed (attempt ");
            Serial.print(i+1);
            Serial.println(")");
            delay(delayMs);
        }
    }
}


// VOID SETUP ==========================================================================
void setup(void) {
  pinMode(2, OUTPUT); 
  pinMode(BUZZER_PIN, OUTPUT);

  Serial.begin(115200);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  if(esp_now_init() != ESP_OK){
          Serial.println("ESP-NOW init failed!");
          return;
      }
  esp_now_register_recv_cb(OnDataRecv); // NEW callback signature //changing

  Serial.println("Receiver ready...");
  esp_now_register_send_cb(onSent);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, BaseStationAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;  

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
      Serial.println("Failed to add peer");
      return;
  }
  Serial.println("Transmitter ");
  Serial.print(DEVICE_ID);
  Serial.println(" ready.");


  while (!Serial)
    delay(10); // will pause Zero, Leonardo, etc until serial console opens

  Serial.println("Adafruit MPU6050 test!");

  // Try to initialize!
  if (!mpu.begin()) {
    Serial.println("Failed to find MPU6050 chip");
    while (1) {
      delay(10);
    }
  }
  Serial.println("MPU6050 Found!");

  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  Serial.print("Accelerometer range set to: ");
  switch (mpu.getAccelerometerRange()) {
  case MPU6050_RANGE_2_G:
    Serial.println("+-2G");
    break;
  case MPU6050_RANGE_4_G:
    Serial.println("+-4G");
    break;
  case MPU6050_RANGE_8_G:
    Serial.println("+-8G");
    break;
  case MPU6050_RANGE_16_G:
    Serial.println("+-16G");
    break;
  }
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  Serial.print("Gyro range set to: ");
  switch (mpu.getGyroRange()) {
  case MPU6050_RANGE_250_DEG:
    Serial.println("+- 250 deg/s");
    break;
  case MPU6050_RANGE_500_DEG:
    Serial.println("+- 500 deg/s");
    break;
  case MPU6050_RANGE_1000_DEG:
    Serial.println("+- 1000 deg/s");
    break;
  case MPU6050_RANGE_2000_DEG:
    Serial.println("+- 2000 deg/s");
    break;
  }

  mpu.setFilterBandwidth(MPU6050_BAND_44_HZ);
  Serial.print("Filter bandwidth set to: ");
  switch (mpu.getFilterBandwidth()) {
  case MPU6050_BAND_260_HZ:
    Serial.println("260 Hz");
    break;
  case MPU6050_BAND_184_HZ:
    Serial.println("184 Hz");
    break;
  case MPU6050_BAND_94_HZ:
    Serial.println("94 Hz");
    break;
  case MPU6050_BAND_44_HZ:
    Serial.println("44 Hz");
    break;
  case MPU6050_BAND_21_HZ:
    Serial.println("21 Hz");
    break;
  case MPU6050_BAND_10_HZ:
    Serial.println("10 Hz");
    break;
  case MPU6050_BAND_5_HZ:
    Serial.println("5 Hz");
    break;
  }
  digitalWrite(2, HIGH);
  Serial.println("");
  delay(100);
}



// VOID LOOP ===================================================================
void loop() {

  sensors_event_t a, g, temp; // struct for raw value of accel, gyro, and temp
  mpu.getEvent(&a, &g, &temp); // read in values 

  // Assign raw values with labels
  float ax, ay, az, gx, gy, gz;
  ax = a.acceleration.x;
  ay = a.acceleration.y;
  az = a.acceleration.z;
  gx = g.gyro.x;
  gy = g.gyro.y;
  gz = g.gyro.z;

  // Compute magnitudes vectors for acceleration and magnitude
  float accel_mag = sqrt(ax*ax + ay*ay + az*az);
  float gyro_mag  = sqrt(gx*gx + gy*gy + gz*gz);

  // Add vectors to buffer based on BUFFER_SIZE
  addToBuffer(accel_mag, gyro_mag);

  // Find max value in each buffer
  float max_accel = get_max_accel();
  float max_gyro = get_max_gyro();


  // FALL START CONDITION
  if (!fall_started) {

      if (max_accel > INIT_MOTION_THRESH)
          accel_motion_detected = true;

      if (max_gyro > INIT_ROTATION_THRESH)
          gyro_motion_detected = true;

      if (accel_motion_detected || gyro_motion_detected) {
          fall_started = true;
          motion_time = millis();
          Serial.println("Potential Fall Detected!");
          Serial.println("========================");
          Serial.println("========================");
      }
  }


  // FALL IMPACT CONDITION
  if (fall_started && !impact_detected) {

      // check only within first 2 seconds after motion
      if (millis() - motion_time < 3000) {

          if (max_accel > IMPACT_THRESH) {
              impact_detected = true;
              impact_time = millis();
              Serial.println("IMPACT DETECTED!");
              Serial.println("========================");
              Serial.println("========================");
          }
      }
  }



  // RESET CASE - Fall started but no impact detected within timer
  if (fall_started && !impact_detected){
    if (millis() - motion_time > 3000){
    
      reset_state();

      Serial.println("NO IMPACT DETECTED - RESETTING STATE");
      Serial.println("========================");
      Serial.println("========================");
    }
  }


  // IDLE CONDITION
if (impact_detected) {
    // check if motion occurred after 4 seconds - if no motion -> fall occurred
    if (millis() - impact_time > 5000) {
        if (max_accel < IDLE_THRESH) {
            Serial.println("FALL CONFIRMED! - SEND HELP");
            Serial.println("========================");
            Serial.println("========================");
            buzzAlert();

            // Determine closest device
            if (distDevice1 < distDevice2 && distDevice1 < distDevice3) {
                Serial.printf("Device 1 is the closest at %.2f meters\n", distDevice1);
                Serial.printf("Device 2 : %.2f meters\n", distDevice2);
                Serial.printf("Device 3 : %.2f meters\n", distDevice3);
                final_distance = distDevice1;
                strcpy(final_location, "Upstairs");
            }
            else if (distDevice2 < distDevice1 && distDevice2 < distDevice3) {
                Serial.printf("Device 2 is the closest at %.2f meters\n", distDevice2);
                Serial.printf("Device 1 : %.2f meters\n", distDevice1);
                Serial.printf("Device 3 : %.2f meters\n", distDevice3);
                final_distance = distDevice2;
                strcpy(final_location, "Main Floor");
            }
            else {
                Serial.printf("Device 3 is the closest at %.2f meters\n", distDevice3);
                Serial.printf("Device 1 : %.2f meters\n", distDevice1);
                Serial.printf("Device 2: %.2f meters\n", distDevice2);
                final_distance = distDevice3;
                strcpy(final_location, "Basement");
            }
        myData.distance = final_distance;
        strcpy(myData.location, final_location);
        sendFallAlert();
        delay(500);  // send 2× per second

        } else {
            Serial.println("FALSE ALARM!");
            Serial.println("========================");
            Serial.println("========================");
        }

        // Reset flags and buffers
        reset_state();
    }
}

  // if (max_accel != lastMaxAccel){
  //   Serial.print("Max acceleration vector is: ");
  //   Serial.print(max_accel);
  //   Serial.println(" m/s^2");
  //   lastMaxAccel = max_accel;
  //   Serial.println("");
  // }
  
  // if (max_gyro != lastMaxGyro){
  //   Serial.print("Max gyro rate vector is: ");
  //   Serial.print(max_gyro);
  //   Serial.println(" rad/s");
  //   lastMaxGyro = max_gyro;
  //   Serial.println("");
  // }

  // Serial.print("Acceleration X: ");
  // Serial.print(a.acceleration.x);
  // Serial.print(", Y: ");
  // Serial.print(a.acceleration.y);
  // Serial.print(", Z: ");
  // Serial.print(a.acceleration.z);
  // Serial.println(" m/s^2");

  // Serial.print("Rotation X: ");
  // Serial.print(g.gyro.x);
  // Serial.print(", Y: ");
  // Serial.print(g.gyro.y);
  // Serial.print(", Z: ");
  // Serial.print(g.gyro.z);
  // Serial.println(" rad/s");

  delay(20);
}