// ================================================================
// ROBOT BLE - CÓ 2 CHẾ ĐỘ: DÒ LINE KHỐI TRẮNG / KHỐI ĐEN
// Lệnh BLE: WHITE → chọn dò trắng | BLACK → chọn dò đen
// ================================================================
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

// ==================== PINS ====================
const int ENC_LEFT_A  = 36, ENC_LEFT_B  = 39;
const int ENC_RIGHT_A = 23, ENC_RIGHT_B = 19;
const int PIN_SERVO   = 15;
const uint8_t S_PINS[] = {25, 33, 32};
const uint8_t SIG_PIN  = 34;
const int AIN1 = 26, AIN2 = 27, PWMA = 14;
const int BIN1 = 18, BIN2 = 5,  PWMB = 4;

// ==================== BLE GLOBALS ====================
BLEServer*         pServer           = nullptr;
BLECharacteristic* pTxCharacteristic = nullptr;
bool deviceConnected    = false;
bool oldDeviceConnected = false;

// ==================== SERVO ====================
Servo gripperServo;
int OPEN_ANGLE = 0, CLOSE_ANGLE = 65, currentServoAngle = 0;

// Sweep từ từ
void servoSweep(int target) {
    if (target > currentServoAngle)
        for (int a = currentServoAngle; a <= target; a++) { gripperServo.write(a); delay(15); }
    else
        for (int a = currentServoAngle; a >= target; a--) { gripperServo.write(a); delay(15); }
    currentServoAngle = target;
}

// ==================== SENSOR ====================
MyQTR8C qtr(S_PINS, SIG_PIN);
uint16_t sensorValues[8];
uint8_t  currentPattern = 0;
unsigned int lastLinePos = 3500;
#define SETPOINT 3500

int docSensor() {
    lastLinePos = qtr.readLine(sensorValues);
    currentPattern = 0;
    int black = 0;
    for (int i = 0; i < 8; i++) {
        if (sensorValues[i] > 500) { currentPattern |= (1 << (7-i)); black++; }
    }
    return black;
}

// ==================== PID & MOTOR PARAMS ====================
const float KP2 = 0.02f,  KD2 = 0.14f; const int BASE2 = 140;
const float KP3 = 0.022f, KD3 = 0.10f; const int BASE3 = 130;
const int BOOST = 20, BOOST_THRESHOLD = 150;

volatile long pulseLeft = 0, pulseRight = 0;
void IRAM_ATTR isrLeftA()  { if (digitalRead(ENC_LEFT_B))  pulseLeft++;  else pulseLeft--;  }
void IRAM_ATTR isrRightA() { if (digitalRead(ENC_RIGHT_B)) pulseRight++; else pulseRight--; }

// ==================== TRẠNG THÁI ====================
bool modeLine  = false;
bool runLine   = false;
bool hasTurned = false;
bool bamCanh   = false;
int  flat = 0, flat2 = 0, flat3 = 0;
int  cnt = 0;
int  speed_dk = 120;
int  lastError = 0;

// QUAN TRỌNG: chọn chế độ dò line
// true  = dò khối TRẮNG
// false = dò khối ĐEN
bool whiteMode = true;

String currentModeName = "IDLE";
String currentCaseName = "-";
unsigned long lastStatusTime = 0;
#define STATUS_INTERVAL 150

// ==================== MOTOR ====================
void motorSetup() {
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, PWMA);
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM1A, PWMB);
    mcpwm_config_t cfg = {5000, 0, 0, MCPWM_DUTY_MODE_0, MCPWM_UP_COUNTER};
    mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &cfg);
    mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_1, &cfg);
}

void move(int left, int right) {
    digitalWrite(AIN1, left  >= 0 ? LOW : HIGH);
    digitalWrite(AIN2, left  >= 0 ? HIGH : LOW);
    digitalWrite(BIN1, right >= 0 ? LOW : HIGH);
    digitalWrite(BIN2, right >= 0 ? HIGH : LOW);
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, constrain(abs(left), 0,255)*100.0f/255.0f);
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_1, MCPWM_OPR_A, constrain(abs(right),0,255)*100.0f/255.0f);
}

void stopMotor() { move(0, 0); }

void movePID(int base, int adjust) {
    int L = base + adjust, R = base - adjust;
    if (adjust >  BOOST_THRESHOLD) L += BOOST;
    if (adjust < -BOOST_THRESHOLD) R += BOOST;
    move(constrain(L,-255,255), constrain(R,-255,255));
}

// ==================== BLE SEND ====================
void bleSend(const char* msg) {
    if (deviceConnected && pTxCharacteristic) {
        pTxCharacteristic->setValue((uint8_t*)msg, strlen(msg));
        pTxCharacteristic->notify();
        delay(5);
    }
    Serial.println(msg);
}

void sendStatus() {
    if (!deviceConnected) return;
    if (millis() - lastStatusTime < STATUS_INTERVAL) return;
    lastStatusTime = millis();

    char patStr[9];
    for (int i = 7; i >= 0; i--)
        patStr[7-i] = (currentPattern & (1<<i)) ? '1' : '0';
    patStr[8] = '\0';

    char buf[256];
    snprintf(buf, sizeof(buf),
        "{\"mode\":\"%s\",\"case\":\"%s\","
        "\"pos\":%u,\"pat\":\"%s\","
        "\"spd\":%d,\"cnt\":%d,\"err\":%d,"
        "\"srv\":%d,\"color\":\"%s\"}",
        currentModeName.c_str(), currentCaseName.c_str(),
        lastLinePos, patStr,
        speed_dk, cnt, lastError,
        currentServoAngle,
        whiteMode ? "WHITE" : "BLACK"   // ← gửi chế độ màu
    );
    pTxCharacteristic->setValue((uint8_t*)buf, strlen(buf));
    pTxCharacteristic->notify();
}

// ==================== SERVO ACTIONS ====================
void gapVat()  { bleSend("LOG:GAPPING");   servoSweep(CLOSE_ANGLE); }
void thaVat()  { bleSend("LOG:RELEASING"); servoSweep(OPEN_ANGLE);  }
void gapVat2() { bleSend("LOG:GAPPING");   servoSweep(65); }
// ==================== BLE COMMANDS ====================
void handleBLECommand(String cmd) {
    cmd.trim();

    if (cmd.startsWith("SPD:")) {
        speed_dk = constrain(cmd.substring(4).toInt(), 30, 255);
        char buf[30]; snprintf(buf, sizeof(buf), "LOG:SPD=%d", speed_dk);
        bleSend(buf); return;
    }
    if (cmd.startsWith("SRV:")) {
        servoSweep(constrain(cmd.substring(4).toInt(), 0, 180));
        char buf[30]; snprintf(buf, sizeof(buf), "LOG:SRV=%d", currentServoAngle);
        bleSend(buf); return;
    }

    // Chọn chế độ màu (có thể đổi ngay cả khi đang MANUAL)
    if (cmd == "WHITE") {
        whiteMode = true;
        bleSend("LOG:MODE_WHITE"); return;
    }
    if (cmd == "BLACK") {
        whiteMode = false;
        bleSend("LOG:MODE_BLACK"); return;
    }

    if (cmd == "AUTO") {
        runLine = true; modeLine = true;
        cnt = 0; flat = 0; flat2 = 0; flat3 = 0;
        hasTurned = false; bamCanh = false;
        pulseLeft = 0; pulseRight = 0;
        lastError = 0; lastLinePos = SETPOINT;
        currentModeName = "AUTO";
        currentCaseName = whiteMode ? "WHITE_MAP2" : "BLACK_MAP2";
        bleSend(whiteMode ? "LOG:AUTO_WHITE_START" : "LOG:AUTO_BLACK_START");
        return;
    }
    if (cmd == "MANUAL") {
        runLine = false; modeLine = false; stopMotor();
        currentModeName = "MANUAL"; currentCaseName = "IDLE";
        bleSend("LOG:MODE_MANUAL"); return;
    }

    // Lệnh điều khiển tay (chỉ khi MANUAL)
    if (!runLine) {
        if      (cmd == "FWD") move(speed_dk, speed_dk);
        else if (cmd == "BWD") move(-speed_dk, -speed_dk);
        else if (cmd == "LFT") move(-speed_dk, speed_dk);
        else if (cmd == "RGT") move(speed_dk, -speed_dk);
        else if (cmd == "STP") stopMotor();
        else if (cmd == "GRP") gapVat2();
        else if (cmd == "REL") thaVat();
    }
}

// ==================== BLE CALLBACKS ====================
class MyCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pChar) override {
        std::string v = pChar->getValue();
        if (v.length() > 0) handleBLECommand(String(v.c_str()));
    }
};
class MyServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* p) override {
        deviceConnected = true;
        bleSend("LOG:BLE_CONNECTED");
    }
    void onDisconnect(BLEServer* p) override {
        deviceConnected = false;
        stopMotor(); runLine = false;
        BLEDevice::startAdvertising();
    }
};

// ==================== MAP2 - KHỐI TRẮNG ====================
void lineFollowMap2_WHITE() {
    int blackCount = docSensor();

    // VẠCH 1 (cnt=0): Rẽ trái
    if (currentPattern == 0b11111111 && cnt == 0) {
        currentCaseName = "W_TURN_LEFT";
        move(100,100); delay(290); stopMotor(); delay(300);
        move(-120,110); delay(510); stopMotor(); delay(300);
        cnt++; lastError = 0; lastLinePos = SETPOINT;
        Serial.println(">>> WHITE VACH1: RE TRAI cnt=1"); return;
    }

    // VẠCH 2 (cnt=1): Rẽ phải
    if (currentPattern == 0b11111111 && cnt == 1) {
        currentCaseName = "W_TURN_RIGHT";
        move(100,100); delay(290); stopMotor(); delay(300);
        move(120,-110); delay(510); stopMotor(); delay(300);
        cnt++; lastError = 0; lastLinePos = SETPOINT;
        Serial.println(">>> WHITE VACH2: RE PHAI cnt=2"); return;
    }

    // VẠCH 3 (cnt=2): Tiến → gắp → lùi → xoay → bám line → Map3
    if (currentPattern == 0b11111111 && cnt == 2) {
        currentCaseName = "W_GRIP";
        move(100,100); delay(290); stopMotor(); delay(500);
        move(-100,100); delay(510); stopMotor(); delay(500);
        move(100,95); delay(950); stopMotor(); delay(500);
        gapVat();
        move(-100,-100); delay(900);
        move(120,-110); delay(510); stopMotor(); delay(500);

        // Bám line PID 2.2s
        unsigned long tStart = millis();
        currentCaseName = "W_FOLLOW_2S";
        while (millis() - tStart < 2200) {
            int bc = docSensor();
            int sp = SETPOINT, et = 0;
            if (currentPattern & 0b00000001) { sp = 0;    et = -40; }
            else if (currentPattern & 0b10000000) { sp = 7000; et = 40; }
            if (bc == 0) {
                if (lastError > 0) move(BASE2-20+BOOST, -BASE2);
                else               move(-BASE2, BASE2-20+BOOST);
            } else {
                int err = (int)lastLinePos - sp;
                int adj = (int)(err*KP2) + (int)(KD2*(err-lastError));
                lastError = err;
                movePID(BASE2+et, adj);
            }
        }
        stopMotor(); delay(1000);
        cnt = 3; lastError = 0; lastLinePos = qtr.readLine(sensorValues);
        bamCanh = false; hasTurned = false; flat = 0; flat2 = 0;
        currentCaseName = "W_TO_MAP3";
        Serial.printf(">>> WHITE → MAP3 | pos=%d\n", lastLinePos); return;
    }

    // PID bám line thường
    currentCaseName = "W_FOLLOW";
    int dynamicSetpoint = SETPOINT, extraTurn = 0;
    if (currentPattern & 0b00000001) { move(BASE2+60, BASE2-80); return; }
    if (currentPattern & 0b10000000) { move(BASE2-80, BASE2+60); return; }
    if (blackCount == 0) {
        if (lastError > 0) move(BASE2+60+BOOST, -BASE2);
        else               move(-BASE2, BASE2+60+BOOST);
        return;
    }
    int error  = (int)lastLinePos - dynamicSetpoint;
    int adjust = (int)(error*KP2) + (int)(KD2*(error-lastError));
    lastError = error;
    movePID(BASE2+extraTurn, adjust);
}

// ==================== MAP2 - KHỐI ĐEN ====================
void lineFollowMap2_BLACK() {
    int blackCount = docSensor();

    // VẠCH 1 (cnt=0): Rẽ trái
    if (currentPattern == 0b11111111 && cnt == 0) {
        currentCaseName = "B_TURN_LEFT";
        move(100,100); delay(290); stopMotor(); delay(300);
        move(-120,110); delay(510); stopMotor(); delay(300);
        cnt++; lastError = 0; lastLinePos = SETPOINT;
        Serial.println(">>> BLACK VACH1: RE TRAI cnt=1"); return;
    }

    // VẠCH 2 (cnt=1): Tiến → gắp → lùi → xoay
    if (currentPattern == 0b11111111 && cnt == 1) {
        currentCaseName = "B_GRIP";
        move(100,100); delay(500); stopMotor(); delay(600);
        gapVat(); delay(500);
        move(-120,-120); delay(650); stopMotor(); delay(200);
        move(145,-100); delay(300); stopMotor(); delay(300);
        move(BASE2, BASE2); delay(200); stopMotor(); delay(100);
        cnt++; lastError = 0; lastLinePos = SETPOINT;
        Serial.println(">>> BLACK GAP VAT XONG cnt=2"); return;
    }

    // VẠCH 3 (cnt=2): Bám line 2.2s → Map3
    if (currentPattern == 0b11111111 && cnt == 2) {
        currentCaseName = "B_FOLLOW_2S";
        move(100,100); delay(200); stopMotor(); delay(500);

        unsigned long tStart = millis();
        while (millis() - tStart < 2200) {
            int bc = docSensor();
            int sp = SETPOINT, et = 0;
            if (currentPattern & 0b00000001) { sp = 0;    et = -40; }
            else if (currentPattern & 0b10000000) { sp = 7000; et = 40; }
            if (bc == 0) {
                if (lastError > 0) move(BASE2-20+BOOST, -BASE2);
                else               move(-BASE2, BASE2-20+BOOST);
            } else {
                int err = (int)lastLinePos - sp;
                int adj = (int)(err*KP2) + (int)(KD2*(err-lastError));
                lastError = err;
                movePID(BASE2+et, adj);
            }
        }
        stopMotor(); delay(1000);
        cnt = 3; lastError = 0; lastLinePos = qtr.readLine(sensorValues);
        bamCanh = false; hasTurned = false; flat = 0; flat2 = 0;
        currentCaseName = "B_TO_MAP3";
        Serial.printf(">>> BLACK → MAP3 | pos=%d\n", lastLinePos); return;
    }

    // PID bám line thường
    currentCaseName = "B_FOLLOW";
    int dynamicSetpoint = SETPOINT, extraTurn = 0;
    if (currentPattern & 0b00000001) { move(BASE2+60, BASE2-80); return; }
    if (currentPattern & 0b10000000) { move(BASE2-80, BASE2+60); return; }
    if (blackCount == 0) {
        if (lastError > 0) move(BASE2+60+BOOST, -BASE2);
        else               move(-BASE2, BASE2+60+BOOST);
        return;
    }
    int error  = (int)lastLinePos - dynamicSetpoint;
    int adjust = (int)(error*KP2) + (int)(KD2*(error-lastError));
    lastError = error;
    movePID(BASE2+extraTurn, adjust);
}

// ==================== MAP3 (dùng chung) ====================
void lineFollowMap3() {
    int blackCount = docSensor();
    int dynamicSetpoint = SETPOINT, extraTurn = 0;

    // Thả vật
    if (currentPattern == 0b11111111 && cnt == 4 && flat == 1) {
        currentCaseName = "MAP3_DONE";
        move(100,100); delay(200); stopMotor(); delay(500);
        thaVat(); runLine = false; modeLine = false;
        currentModeName = "IDLE"; cnt++; return;
    }

    // Vượt vòng tròn
    if (currentPattern == 0b11111111 && cnt == 3 && flat2 == 1) {
        currentCaseName = "MAP3_SPRINT";
        flat = 1; move(200,200); delay(500);
        cnt++; lastError = 0; lastLinePos = SETPOINT; return;
    }

    // Đang vượt mà mất line
    if (currentPattern == 0b00000000 && flat == 1) {
        currentCaseName = "MAP3_BLIND";
        move(200,200); delay(200);
    }

    if (currentPattern & 0b00000001) { dynamicSetpoint = 0;    extraTurn = -50; }
    else if (currentPattern & 0b10000000) { dynamicSetpoint = 7000; extraTurn = 50; }

    // Cua trái 45°
    if (!hasTurned && (currentPattern & 0b10000000) && lastLinePos < 2500) {
        currentCaseName = "MAP3_UTURN";
        stopMotor(); delay(50);
        move(-145,190); delay(300); stopMotor(); delay(150);
        hasTurned = true; lastError = 0; flat3 = 1; return;
    }

    // Bám cạnh
    if (blackCount >= 5) { bamCanh = true; flat2 = 1; }
    if (bamCanh && flat3 == 1) {
        currentCaseName = "MAP3_WALL";
        if (lastLinePos < SETPOINT) move(BASE3+50, BASE3-35);
        else                        move(BASE3-35, BASE3+50);
        if (blackCount <= 2) { bamCanh = false; lastError = 0; }
        return;
    }

    // Mất line
    if (blackCount == 0) {
        currentCaseName = "MAP3_LOST";
        if (lastError > 0) move(BASE3+50+BOOST, -BASE3);
        else               move(-BASE3, BASE3+50+BOOST);
        return;
    }

    // PID
    currentCaseName = "MAP3_FOLLOW";
    int error  = (int)lastLinePos - dynamicSetpoint;
    int adjust = (int)(error*KP3) + (int)(KD3*(error-lastError));
    lastError = error;
    movePID(BASE3+extraTurn, adjust);
}

// ==================== SETUP ====================
void setup() {
    Serial.begin(115200);
    motorSetup();
    pinMode(AIN1,OUTPUT); pinMode(AIN2,OUTPUT);
    pinMode(BIN1,OUTPUT); pinMode(BIN2,OUTPUT);
    pinMode(ENC_LEFT_A,  INPUT);
    pinMode(ENC_LEFT_B,  INPUT);
    pinMode(ENC_RIGHT_A, INPUT_PULLUP);
    pinMode(ENC_RIGHT_B, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(ENC_LEFT_A),  isrLeftA,  RISING);
    attachInterrupt(digitalPinToInterrupt(ENC_RIGHT_A), isrRightA, RISING);

    gripperServo.attach(PIN_SERVO);
    gripperServo.write(OPEN_ANGLE);
    currentServoAngle = OPEN_ANGLE;

    // BLE
    BLEDevice::init("ROBOT_BLE");
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());
    BLEService* pSvc = pServer->createService(SERVICE_UUID);
    pTxCharacteristic = pSvc->createCharacteristic(
        CHARACTERISTIC_UUID_TX, BLECharacteristic::PROPERTY_NOTIFY);
    pTxCharacteristic->addDescriptor(new BLE2902());
    BLECharacteristic* pRx = pSvc->createCharacteristic(
        CHARACTERISTIC_UUID_RX,
        BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
    pRx->setCallbacks(new MyCallbacks());
    pSvc->start();
    BLEDevice::startAdvertising();
    Serial.println("BLE advertising... ROBOT_BLE");

    qtr.begin();
    Serial.println("CALIBRATING 5s...");
    unsigned long t = millis();
    while (millis()-t < 5000) qtr.calibrate();
    Serial.println("READY!");
    bleSend("LOG:ROBOT_READY");
}

// ==================== LOOP ====================
void loop() {
    // BLE reconnect
    if (!deviceConnected && oldDeviceConnected) {
        delay(300); BLEDevice::startAdvertising(); oldDeviceConnected = false;
    }
    if (deviceConnected && !oldDeviceConnected) oldDeviceConnected = true;

    if (runLine) {
        if (cnt <= 2) {
            // Gọi đúng hàm theo chế độ đã chọn
            if (whiteMode) lineFollowMap2_WHITE();
            else           lineFollowMap2_BLACK();
        } else {
            lineFollowMap3(); // Map3 dùng chung
        }
    }

    sendStatus();
}
