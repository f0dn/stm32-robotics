#include "WSerial.h"
#include "wiring_time.h"
#include <Adafruit_BNO08x.h>
#include <Arduino.h>
#include <Wire.h>

#define NUM_MOTORS 8

const uint16_t MOTOR_PINS[NUM_MOTORS] = {GPIO_PIN_8,  GPIO_PIN_9,  GPIO_PIN_10,
                                         GPIO_PIN_11, GPIO_PIN_12, GPIO_PIN_13,
                                         GPIO_PIN_14, GPIO_PIN_15};

const int BNO0X_INTERRUPT_PIN = PA0;
const int BNO0X_RESET_PIN = PA1;

Adafruit_BNO08x bno(BNO0X_RESET_PIN);
sh2_SensorValue_t bnoValue;

/*
  The data received by the microcontroller from UART.

  This is reconstructed from a CSV string in the form of:
  ```
  m1,m2,m3,m4,m5,m6,m7,m8
  ```
  where m_i is the value of the i-th motor, in the range [0, 255].
*/
class Rx {
  public:
    bool isValid = true;
    int32_t motorValues[NUM_MOTORS];

    Rx(const String &csv) {
        // Serial.println("DEBUG, parsing CSV string: " + csv);
        // parse exactly MOTOR_NUMBERS values from the CSV string
        int startIndex = 0;
        for (int motorNum = 0; motorNum < NUM_MOTORS; motorNum++) {
            int commaIndex = csv.indexOf(',', startIndex);
            if (commaIndex == -1 && motorNum < NUM_MOTORS - 1) {
                Serial.println("DEBUG, not enough values in CSV string: " +
                               csv + " at motor number: " + String(motorNum));
                // not enough values in the CSV string
                isValid = false;
                return;
            }

            String motorValueStr = csv.substring(startIndex, commaIndex);
            motorValues[motorNum] = motorValueStr.toInt();
            startIndex = commaIndex + 1;
        }
    }
};

void setup() {
    pinMode(BNO0X_INTERRUPT_PIN, INPUT);
    pinMode(BNO0X_RESET_PIN, OUTPUT);
    digitalWrite(BNO0X_RESET_PIN, HIGH); // Keep BNO085 out of reset
    pinMode(PB8, INPUT_PULLUP); // SCL
    pinMode(PB9, INPUT_PULLUP); // SDA
    delay(2000);                 // allow pio --target monitor to connect
    Serial.begin(38400);
    while (!Serial) {
        delay(10);
    }
    //Wire.begin(PB9, PB8); // SDA, SCL
    Wire.begin();
    Wire.setClock(400000);
    delay(2000);

    int found = 0;
    for (int x = 1; x < 127; x++) {
        Wire.beginTransmission(x);
        byte error = Wire.endTransmission();
        if (error == 0) {
            found++;
            Serial.print("I2C device found at address 0x");
            if (x < 16) {
                Serial.print("0");
            }
            Serial.println(x, HEX);
        }
        Serial.println(error, HEX);
        delay(10);
    }
    Serial.println("on the one!");
    Wire.beginTransmission(0x4A);
    byte error = Wire.endTransmission();
    Serial.println(error, HEX);
    Serial.println("I2C scan complete, found " + String(found) + " device(s).");
    int x = 0x4A;
    if (!bno.begin_I2C(x)) {
        Serial.println("Failed to initialize BNO! Check your connections.");
        while (1) {
            delay(10);
        }
    }

    Serial.println("BNO initialized successfully!");

    for (int motorNum = 0; motorNum < NUM_MOTORS; motorNum++)
        analogWrite(MOTOR_PINS[motorNum], 1500);
}

void loop() {
    bno.getSensorEvent(&bnoValue);
    Serial.printf("%f\n", bnoValue.un.accelerometer.x);

    if (Serial.available() > 0) {

        String csv = Serial.readStringUntil('\n');
        if (csv.length() == 0)
            return;

        Rx rx(csv);

        if (!rx.isValid) {
            Serial.println("WARN, invalid CSV string received: " + csv);
            return;
        }

        for (int motorNum = 0; motorNum < NUM_MOTORS; motorNum++)
            analogWrite(
                MOTOR_PINS[motorNum],
                rx.motorValues[motorNum]); // this hasnt been tested might need
                                           // to send 20000ms blocks instead.
                                           // see old code

        Serial.printf("DEBUG, received motor values: %ld, %ld, %ld, %ld, %ld, "
                      "%ld, %ld, %ld\n",
                      rx.motorValues[0], rx.motorValues[1], rx.motorValues[2],
                      rx.motorValues[3], rx.motorValues[4], rx.motorValues[5],
                      rx.motorValues[6], rx.motorValues[7]);
    }
}

#define SDA_PIN PB9 // Change to your SDA pin
#define SCL_PIN PB8 // Change to your SCL pin

void i2c_delay() {
    delayMicroseconds(5); // Adjust as needed for your board/speed
}

void i2c_start() {
    pinMode(SDA_PIN, OUTPUT);
    pinMode(SCL_PIN, OUTPUT);
    digitalWrite(SDA_PIN, HIGH);
    digitalWrite(SCL_PIN, HIGH);
    i2c_delay();
    digitalWrite(SDA_PIN, LOW);
    i2c_delay();
    digitalWrite(SCL_PIN, LOW);
    i2c_delay();
}

void i2c_stop() {
    pinMode(SDA_PIN, OUTPUT);
    digitalWrite(SDA_PIN, LOW);
    digitalWrite(SCL_PIN, HIGH);
    i2c_delay();
    digitalWrite(SDA_PIN, HIGH);
    i2c_delay();
}

void print(String message) {
    Serial.print(message);
    Serial.print(" STUFF: ");
    Serial.print("SDA: ");
    Serial.print(digitalRead(SDA_PIN));
    Serial.print(" SCL: ");
    Serial.println(digitalRead(SCL_PIN));
}

bool i2c_write_byte(uint8_t data) {
    // Serial.println("writing byte: " + String(data, HEX));
    for (int i = 0; i < 8; i++) {
        // print("before writing bit");
        digitalWrite(SDA_PIN, (data & 0x80) ? HIGH : LOW);
        // print("after writing bit");
        i2c_delay();
        digitalWrite(SCL_PIN, HIGH);
        i2c_delay();
        digitalWrite(SCL_PIN, LOW);
        i2c_delay();
        data <<= 1;
    }
    // ACK bit
    pinMode(SDA_PIN, INPUT_PULLUP);
    i2c_delay();
    digitalWrite(SCL_PIN, HIGH);
    i2c_delay();
    bool ack = !digitalRead(SDA_PIN);
    // print("resp");
    digitalWrite(SCL_PIN, LOW);
    pinMode(SDA_PIN, OUTPUT);
    return ack;
}

void ssetup() {
    Serial.begin(38400);
    pinMode(SDA_PIN, OUTPUT);
    pinMode(SCL_PIN, OUTPUT);
    digitalWrite(SDA_PIN, HIGH);
    digitalWrite(SCL_PIN, HIGH);
    delay(100);
    Serial.println("Bit-bang I2C Scanner");
}

void sloop() {
    // print("before high");
    // digitalWrite(SCL_PIN, HIGH);
    // i2c_delay();
    // print("before low");
    // digitalWrite(SCL_PIN, LOW);
    // i2c_delay();
    int found = 0;
    for (uint8_t addr = 0; addr < 127; addr++) {
        i2c_start();
        bool ack = i2c_write_byte(addr << 1);
        i2c_stop();
        if (ack) {
            Serial.print("Device found at 0x");
            if (addr < 16)
                Serial.print("0");
            Serial.println(addr, HEX);
            found++;
        }
    }
    if (!found)
        Serial.println("No devices found.");
    delay(2000);
}
