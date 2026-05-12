// // code tong hop tay gap moi

// #define BLYNK_TEMPLATE_ID "TMPL6j2mO1koh"
// #define BLYNK_TEMPLATE_NAME "dk dong co"
// #define BLYNK_AUTH_TOKEN "OUGEmmVetmLJWvYPwK2FdWSj-rXPdnqZ"
// #define BLYNK_PRINT Serial

// #include <Arduino.h>
// #include <WiFi.h>
// #include <BlynkSimpleEsp32.h>
// #include <ESP32Servo.h>
// #include <driver/mcpwm.h>
// #include "MyQTR8C.h"

// // ==================== THÔNG TIN WIFI ====================
// char auth[] = BLYNK_AUTH_TOKEN;
// char ssid[] = "Xuong";
// char pass[] = "68686868";

// // ==================== CHÂN (PINS) ====================
// const int nut1 = 35;
// const int NUT3 = 2;

// const int AIN1 = 26, AIN2 = 27, PWMA = 14;
// const int BIN1 = 18, BIN2 = 5,  PWMB = 4;

// const int PIN_SERVO = 15;

// const uint8_t S_PINS[] = {25, 33, 32};
// const uint8_t SIG_PIN  = 34;

// // ==================== THÔNG SỐ SERVO ====================
// int OPEN_ANGLE  = 0;
// int CLOSE_ANGLE = 72;

// // Lưu góc hiện tại của servo để sweep đúng từ vị trí thực tế
// int currentServoAngle = 0;

// // ==================== PID & TỐC ĐỘ ====================
// const float KP2 = 0.026f, KD2 = 0.15f;
// const int   BASE2 = 90;

// // float KP3 = 0.022f, KD3 = 0.1f;
// // int   BASE3 = 80;
// float KP3 = 0.034f, KD3 = 0.44f;
// int   BASE3 = 120;
// #define SETPOINT 3500

// // ==================== ĐỐI TƯỢNG ====================
// Servo    gripperServo;
// MyQTR8C  qtr(S_PINS, SIG_PIN);

// // ==================== BIẾN TRẠNG THÁI ====================
// bool modeBlynk = false;
// bool modeLine  = false;
// bool runLine   = false;
// bool hasTurned = false;
// bool bamCanh   = false;

// unsigned long startTimeMap3 = 0;
// int  lastError   = 0;
// unsigned int lastLinePos = SETPOINT;
// uint8_t  currentPattern  = 0;
// uint16_t sensorValues[8];
// int  cnt    = 0;
// int  flat   = 0;
// int  flat2  = 0;
// int  speed_dk = 100;

// // ==================== MOTOR MCPWM ====================
// void motorSetup() {
//     mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, PWMA);
//     mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM1A, PWMB);
//     mcpwm_config_t cfg = {5000, 0, 0, MCPWM_DUTY_MODE_0, MCPWM_UP_COUNTER};
//     mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &cfg);
//     mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_1, &cfg);
// }

// void setMotor(int pL, int pR) {
//     mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A,
//                    constrain(abs(pL), 0, 255) * 100.0f / 255.0f);
//     mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_1, MCPWM_OPR_A,
//                    constrain(abs(pR), 0, 255) * 100.0f / 255.0f);
// }

// void move(int left, int right) {
//     digitalWrite(AIN1, left  >= 0 ? LOW  : HIGH);
//     digitalWrite(AIN2, left  >= 0 ? HIGH : LOW);
//     digitalWrite(BIN1, right >= 0 ? LOW  : HIGH);
//     digitalWrite(BIN2, right >= 0 ? HIGH : LOW);
//     setMotor(left, right);
// }

// void stopMotor() { move(0, 0); }

// // ==================== SERVO SWEEP TỪ TỪ ====================
// /**
//  * Di chuyển servo từ fromAngle đến toAngle, mỗi bước 1 độ cách nhau stepDelay ms.
//  * stepDelay khuyến nghị:
//  *   5  ms = nhanh
//  *   15 ms = vừa (mặc định)
//  *   25 ms = chậm
//  *   40 ms = rất chậm
//  */
// void servoSweep(int fromAngle, int toAngle, int stepDelay = 15) {
//     if (fromAngle <= toAngle) {
//         for (int angle = fromAngle; angle <= toAngle; angle++) {
//             gripperServo.write(angle);
//             delay(stepDelay);
//         }
//     } else {
//         for (int angle = fromAngle; angle >= toAngle; angle--) {
//             gripperServo.write(angle);
//             delay(stepDelay);
//         }
//     }
// }

// // ==================== HÀNH ĐỘNG ====================
// // Gắp vật – servo đóng từ từ (15 ms/độ)
// void gapVat() {
//     servoSweep(currentServoAngle, CLOSE_ANGLE, 15);
//     currentServoAngle = CLOSE_ANGLE;
//     Serial.println(">>> [ACTION] GAP VAT (TU TU)");
// }

// // Thả vật – servo mở từ từ (12 ms/độ)
// void thaVat() {
//     servoSweep(currentServoAngle, OPEN_ANGLE, 12);
//     currentServoAngle = OPEN_ANGLE;
//     Serial.println(">>> [ACTION] THA VAT (TU TU)");
// }

// // Gắp qua Blynk – servo đóng từ từ đến 72 độ (15 ms/độ)
// void gapVatBlynk() {
//     servoSweep(currentServoAngle, 72, 15);
//     currentServoAngle = 72;
//     Serial.println(">>> [ACTION] GAP VAT BLYNK (TU TU)");
// }

// // ==================== ĐỌC CẢM BIẾN ====================
// int docSensor() {
//     lastLinePos    = qtr.readLine(sensorValues);
//     currentPattern = 0;
//     int black      = 0;
//     for (int i = 0; i < 8; i++) {
//         if (sensorValues[i] > 500) {
//             currentPattern |= (1 << (7 - i));
//             black++;
//         }
//     }
//     return black;
// }

// // ==================== BLYNK CONTROL ====================
// BLYNK_WRITE(V4) {
//     if (param.asInt()) gapVatBlynk();
//     else               thaVat();
// }

// BLYNK_WRITE(V2) {
//     if (modeBlynk) {
//         if (param.asInt()) move(speed_dk - 20, speed_dk - 20);
//         else               stopMotor();
//     }
// }

// BLYNK_WRITE(V1) {
//     if (modeBlynk) {
//         if (param.asInt()) move(-speed_dk + 20, -speed_dk + 20);
//         else               stopMotor();
//     }
// }

// BLYNK_WRITE(V0) {
//     if (modeBlynk) {
//         if (param.asInt()) move(-speed_dk + 20, speed_dk - 20);
//         else               stopMotor();
//     }
// }

// BLYNK_WRITE(V3) {
//     if (modeBlynk) {
//         if (param.asInt()) move(speed_dk - 20, -speed_dk + 20);
//         else               stopMotor();
//     }
// }

// // ==================== LOGIC LINE FOLLOW ====================
// void lineFollowMap2() {
//     int blackCount       = docSensor();
//     int dynamicSetpoint  = SETPOINT;
//     int extraTurn        = 0;
// // vach ngang lần 1
//     if (currentPattern == 0b11111111) {
//         if (cnt == 0) {
//             move(100, 100);   delay(320);
//             stopMotor();      delay(300);
//             move(-140, 140);  delay(300);
//             cnt++;
//             lastError = 0;
//             return;
//         }
// // vạch ngang lần 2
//         if (cnt == 1) {
//             move(100, 100);   delay(700);
//             stopMotor();      delay(300);
//             gapVat();
//             move(-110, -110); delay(500);
//             stopMotor();      delay(200);
//             move(100, -100);  delay(300);
//             cnt++;
//             startTimeMap3 = millis();
//             return;
//         }
// // vạch ngang lần 3
//         if (cnt == 2) {
//             move(100, 100); delay(100);
//             stopMotor();    delay(500);
//             cnt++;
//             return;
//         }
//     }

//     if      (currentPattern & 0b00000001) { dynamicSetpoint = 0;    extraTurn = -40; }
//     else if (currentPattern & 0b10000000) { dynamicSetpoint = 7000; extraTurn =  40; }

//     if (blackCount == 0) {
//         move(lastError > 0 ?  BASE2 + 60 : -BASE2,
//              lastError > 0 ? -BASE2       :  BASE2 + 60);
//         return;
//     }

//     int error  = (int)lastLinePos - dynamicSetpoint;
//     int adjust = (int)(error * KP2) + (int)(KD2 * (error - lastError));
//     move(BASE2 + adjust + extraTurn, BASE2 - adjust - extraTurn);
//     lastError = error;
// }

// void lineFollowMap3() {
//     int blackCount      = docSensor();
//     int dynamicSetpoint = SETPOINT;
//     int extraTurn       = 0;

//     if (currentPattern == 0b11111111 && cnt == 4) {
//         move(100, 100); delay(200);
//         stopMotor();    delay(500);
//         thaVat();
//         runLine  = false;
//         modeLine = false;
//         cnt++;
//         return;
//     }

//     if (currentPattern == 0b11111111 && cnt == 3 && flat2 == 1) {
//         flat = 1;
//         move(200, 200); delay(500);
//         cnt++;
//         lastError = 0;
//         return;
//     }

//     if (currentPattern == 0b00000000 && flat == 1) {
//         move(200, 200); delay(200);
//     }

//     if      (currentPattern & 0b00000001) { dynamicSetpoint = 0;    extraTurn = -50; }
//     else if (currentPattern & 0b10000000) { dynamicSetpoint = 7000; extraTurn =  50; }

//     if (!hasTurned &&
//         (currentPattern & 0b10000000) &&
//         lastLinePos < 2500 &&
//         (millis() - startTimeMap3 > 7000))
//     {
//         stopMotor();      delay(50);
//         move(-145, 150);  delay(350);
//         KP3   = 0.034f;
//         KD3   = 0.02f;
//         BASE3 = 120;
//         hasTurned = true;
//         lastError = 0;
//         return;
//     }

//     if (blackCount >= 5) { bamCanh = true; flat2 = 1; }

//     if (bamCanh) {
//         move(lastLinePos < SETPOINT ? BASE3 + 40 : BASE3 - 15,
//              lastLinePos < SETPOINT ? BASE3 - 15 : BASE3 + 40);
//         if (blackCount <= 2) bamCanh = false;
//         return;
//     }

//     if (blackCount == 0) {
//         move(lastError > 0 ?  BASE3 + 50 : -BASE3,
//              lastError > 0 ? -BASE3       :  BASE3 + 50);
//         return;
//     }

//     int error  = (int)lastLinePos - dynamicSetpoint;
//     int adjust = (int)(error * KP3) + (int)(KD3 * (error - lastError));
//     move(BASE3 + adjust + extraTurn, BASE3 - adjust - extraTurn);
//     lastError = error;
// }

// // ==================== SETUP & LOOP ====================
// void setup() {
//     Serial.begin(115200);

//     motorSetup();

//     pinMode(nut1, INPUT_PULLUP);
//     pinMode(NUT3, INPUT_PULLDOWN);
//     pinMode(AIN1, OUTPUT); pinMode(AIN2, OUTPUT);
//     pinMode(BIN1, OUTPUT); pinMode(BIN2, OUTPUT);

//     // Gắn servo và đặt về vị trí mở ban đầu (từ từ từ 0)
//     gripperServo.attach(PIN_SERVO);
//     gripperServo.write(OPEN_ANGLE); // đặt cứng trước
//     currentServoAngle = OPEN_ANGLE;
//     delay(500);

//     Blynk.begin(auth, ssid, pass);
//     qtr.begin();

//     Serial.println("CALIBRATING...");
//     unsigned long t = millis();
//     while (millis() - t < 5000) qtr.calibrate();
//     Serial.println("READY!");
// }

// void loop() {
//     Blynk.run();

//     // Nút 1: chuyển sang chế độ Blynk
//     if (digitalRead(nut1) == HIGH) {
//         modeBlynk = true;
//         modeLine  = false;
//         runLine   = false;
//         stopMotor();
//         Serial.println("MODE: BLYNK");
//         delay(300);
//     }

//     // Nút 3: chuyển sang chế độ tự động theo line
//     if (digitalRead(NUT3) == HIGH) {
//         delay(200);
//         modeLine  = true;
//         modeBlynk = false;
//         runLine   = true;
//         cnt       = 0;
//         hasTurned = false;
//         bamCanh   = false;
//         flat      = 0;
//         flat2     = 0;
//         lastError    = 0;
//         lastLinePos  = SETPOINT;
//         Serial.println("\n=== ROBOT START AUTO ===");
//         delay(500);
//     }

//     if (modeLine && runLine) {
//         if (cnt <= 2) lineFollowMap2();
//         else          lineFollowMap3();
//     }
// }



#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <ESP32Servo.h>
#include <driver/mcpwm.h>
#include "MyQTR8C.h"

// ==================== BLE UUIDs ====================
#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

BLEServer*         pServer          = nullptr;
BLECharacteristic* pTxCharacteristic = nullptr;
bool deviceConnected    = false;
bool oldDeviceConnected = false;

// ==================== PINS ====================
const int nut1 = 35;
const int NUT3 = 2;

const int AIN1 = 26, AIN2 = 27, PWMA = 14;
const int BIN1 = 18, BIN2 = 5,  PWMB = 4;
const int PIN_SERVO = 15; // Sử dụng 1 chân Servo duy nhất
const uint8_t S_PINS[] = {25, 33, 32};
const uint8_t SIG_PIN  = 34;

// ==================== THÔNG SỐ SERVO ====================
int GOC_MO = 0;
int GOC_DONG = 80;
int toc_do_servo = 20; // Thời gian delay giữa mỗi độ (ms), càng lớn càng chậm
int goc_hien_tai = GOC_MO;

// ==================== THÔNG SỐ LINE ====================
const float KP2 = 0.026f, KD2 = 0.15f; const int BASE2 = 90;
float KP3 = 0.022f, KD3 = 0.1f; int BASE3 = 80;
#define SETPOINT 3500

Servo myServo; 
MyQTR8C qtr(S_PINS, SIG_PIN);

bool modeBlynk = false, modeLine = false, runLine = false, hasTurned = false, bamCanh = false;
unsigned long startTimeMap3 = 0, lastStatusTime = 0;
#define STATUS_INTERVAL 150
int lastError = 0;
unsigned int lastLinePos = SETPOINT;
uint8_t currentPattern = 0;
uint16_t sensorValues[8];
int cnt = 0, flat = 0, flat2 = 0, speed_dk = 100;
String currentModeName = "IDLE", currentCaseName = "-";
void bleSend(const char* msg) {
    if (deviceConnected && pTxCharacteristic) {
        pTxCharacteristic->setValue((uint8_t*)msg, strlen(msg));
        pTxCharacteristic->notify();
    }
    Serial.println(msg);
}
// ==================== HÀM ĐIỀU KHIỂN SERVO TỪ TỪ ====================
void moveServoSmooth(int targetAngle) {
    if (targetAngle > goc_hien_tai) {
        for (int pos = goc_hien_tai; pos <= targetAngle; pos++) {
            myServo.write(pos);
            delay(toc_do_servo);
        }
    } else {
        for (int pos = goc_hien_tai; pos >= targetAngle; pos--) {
            myServo.write(pos);
            delay(toc_do_servo);
        }
    }
    goc_hien_tai = targetAngle;
}

void gapVat() {
    bleSend("LOG:DANG_GAP...");
    moveServoSmooth(GOC_DONG);
    bleSend("LOG:DA_GAP");
}

void thaVat() {
    bleSend("LOG:DANG_THA...");
    moveServoSmooth(GOC_MO);
    bleSend("LOG:DA_THA");
}

// ==================== CÁC HÀM HỖ TRỢ KHÁC ====================


void sendStatus() {
    if (!deviceConnected || millis() - lastStatusTime < STATUS_INTERVAL) return;
    lastStatusTime = millis();
    char patStr[9];
    for (int i = 7; i >= 0; i--) patStr[7-i] = (currentPattern & (1 << i)) ? '1' : '0';
    patStr[8] = '\0';
    char buf[200];
    snprintf(buf, sizeof(buf), "{\"mode\":\"%s\",\"case\":\"%s\",\"pos\":%u,\"pat\":\"%s\",\"spd\":%d,\"cnt\":%d,\"err\":%d}",
             currentModeName.c_str(), currentCaseName.c_str(), lastLinePos, patStr, speed_dk, cnt, lastError);
    pTxCharacteristic->setValue((uint8_t*)buf, strlen(buf));
    pTxCharacteristic->notify();
}

void motorSetup() {
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, PWMA);
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM1A, PWMB);
    mcpwm_config_t cfg = {5000, 0, 0, MCPWM_DUTY_MODE_0, MCPWM_UP_COUNTER};
    mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &cfg);
    mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_1, &cfg);
}

void setMotor(int pL, int pR) {
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, constrain(abs(pL),0,255)*100.0f/255.0f);
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_1, MCPWM_OPR_A, constrain(abs(pR),0,255)*100.0f/255.0f);
}

void move(int left, int right) {
    digitalWrite(AIN1, left >= 0 ? LOW : HIGH);
    digitalWrite(AIN2, left >= 0 ? HIGH : LOW);
    digitalWrite(BIN1, right >= 0 ? LOW : HIGH);
    digitalWrite(BIN2, right >= 0 ? HIGH : LOW);
    setMotor(left, right);
}

void stopMotor() { move(0, 0); }

int docSensor() {
    lastLinePos = qtr.readLine(sensorValues);
    currentPattern = 0;
    int black = 0;
    for (int i = 0; i < 8; i++) {
        if (sensorValues[i] > 500) { currentPattern |= (1 << (7-i)); black++; }
    }
    return black;
}

void handleBLECommand(String cmd) {
    cmd.trim();
    if (cmd.startsWith("SPD:")) {
        speed_dk = constrain(cmd.substring(4).toInt(), 30, 255);
        return;
    }
    if (cmd == "FWD") move(speed_dk, speed_dk);
    else if (cmd == "BWD") move(-speed_dk, -speed_dk);
    else if (cmd == "LFT") move(-(speed_dk-20), speed_dk-20);
    else if (cmd == "RGT") move(speed_dk-20, -(speed_dk-20));
    else if (cmd == "STP") stopMotor();
    else if (cmd == "GRP") gapVat();
    else if (cmd == "REL") thaVat();
    else if (cmd == "MANUAL") {
        modeBlynk = true; modeLine = false; runLine = false;
        currentModeName = "MANUAL"; stopMotor();
    }
    else if (cmd == "AUTO") {
        modeLine = true; modeBlynk = false; runLine = true;
        cnt = 0; currentModeName = "AUTO"; currentCaseName = "MAP2";
    }
}

class MyServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* p) override { deviceConnected = true; bleSend("LOG:CONNECTED"); }
    void onDisconnect(BLEServer* p) override { deviceConnected = false; stopMotor(); }
};

class MyCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pChar) override {
        std::string v = pChar->getValue();
        if (v.length() > 0) handleBLECommand(String(v.c_str()));
    }
};

// ==================== LOGIC LINE (GIỮ NGUYÊN) ====================
void lineFollowMap2() {
    int blackCount = docSensor();
    int dynamicSetpoint = SETPOINT;
    int extraTurn = 0;
    if (currentPattern == 0b11111111) {
        if (cnt == 0) {
            move(100,100); delay(320); stopMotor(); delay(300);
            move(-140,190); delay(300); cnt++; return;
        }
        if (cnt == 1) {
            move(100,100); delay(680); stopMotor(); delay(300);
            gapVat();
            move(-110,-110); delay(690); stopMotor(); delay(200);
            move(100,-100); delay(300); cnt++; startTimeMap3 = millis(); return;
        }
        if (cnt == 2) { move(100,100); delay(100); stopMotor(); delay(500); cnt++; return; }
    }
    if (currentPattern & 0b00000001) { dynamicSetpoint = 0; extraTurn = -40; }
    else if (currentPattern & 0b10000000) { dynamicSetpoint = 7000; extraTurn = 40; }
    if (blackCount == 0) { move(lastError > 0 ? BASE2+60 : -BASE2, lastError > 0 ? -BASE2 : BASE2+60); return; }
    int error = (int)lastLinePos - dynamicSetpoint;
    int adjust = (int)(error*KP2) + (int)(KD2*(error-lastError));
    move(BASE2+adjust+extraTurn, BASE2-adjust-extraTurn);
    lastError = error;
}

void lineFollowMap3() {
    int blackCount = docSensor();
    if (currentPattern == 0b11111111 && cnt == 4) {
        move(100,100); delay(200); stopMotor(); delay(500);
        thaVat(); runLine = false; modeLine = false; currentModeName = "IDLE"; cnt++; return;
    }
    // ... (Các phần logic Map 3 khác giữ nguyên)
    int error = (int)lastLinePos - SETPOINT;
    int adjust = (int)(error*KP3) + (int)(KD3*(error-lastError));
    move(BASE3+adjust, BASE3-adjust);
    lastError = error;
}

void setup() {
    Serial.begin(115200);
    motorSetup();
    pinMode(nut1, INPUT_PULLUP);
    pinMode(NUT3, INPUT_PULLDOWN);
    pinMode(AIN1,OUTPUT); pinMode(AIN2,OUTPUT);
    pinMode(BIN1,OUTPUT); pinMode(BIN2,OUTPUT);

    myServo.attach(PIN_SERVO);
    myServo.write(GOC_MO); // Khởi tạo vị trí mở

    BLEDevice::init("ROBOT_BLE");
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());
    BLEService* pService = pServer->createService(SERVICE_UUID);
    pTxCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_TX, BLECharacteristic::PROPERTY_NOTIFY);
    pTxCharacteristic->addDescriptor(new BLE2902());
    BLECharacteristic* pRx = pService->createCharacteristic(CHARACTERISTIC_UUID_RX, BLECharacteristic::PROPERTY_WRITE);
    pRx->setCallbacks(new MyCallbacks());
    pService->start();
    BLEDevice::startAdvertising();

    qtr.begin();
    unsigned long t = millis();
    while (millis()-t < 5000) qtr.calibrate();
    bleSend("LOG:READY");
}

void loop() {
    if (!deviceConnected && oldDeviceConnected) { delay(300); BLEDevice::startAdvertising(); oldDeviceConnected = false; }
    if (deviceConnected && !oldDeviceConnected) oldDeviceConnected = true;

    if (digitalRead(nut1) == HIGH) { modeBlynk=true; modeLine=false; stopMotor(); delay(300); }
    if (digitalRead(NUT3) == HIGH) { modeLine=true; modeBlynk=false; runLine=true; cnt=0; delay(500); }

    if (modeLine && runLine) {
        if (cnt <= 2) lineFollowMap2();
        else lineFollowMap3();
    }
    sendStatus();
}