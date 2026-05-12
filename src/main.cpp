#include "WSerial.h"
#include "wiring_time.h"
#include <Adafruit_BNO055.h>
#include <Arduino.h>
#include <Wire.h>

#define NUM_MOTORS 8
#define NUM_SERVOS 5
#define NUM_STEPPERS 3

const uint16_t MOTOR_PINS[NUM_MOTORS] = {GPIO_PIN_8,  GPIO_PIN_9,  GPIO_PIN_10,
                                         GPIO_PIN_11, GPIO_PIN_12, GPIO_PIN_13,
                                         GPIO_PIN_14, GPIO_PIN_15};

Adafruit_BNO055 bno = Adafruit_BNO055();

/*
  The data received by the microcontroller from UART.

  This is reconstructed from 16 32-bit integers
  The first 8 are for the motors
  The next 5 are for the servos
  The last 3 are for the stepper motors
*/
class Rx {
  public:
    int32_t motorValues[NUM_MOTORS];
    int32_t servoValues[NUM_SERVOS];
    int32_t stepperValues[NUM_STEPPERS];

    Rx() {
        int32_t buffer[NUM_MOTORS + NUM_SERVOS + NUM_STEPPERS];
        Serial.readBytes((uint8_t *)buffer, sizeof(buffer));
        for (int i = 0; i < NUM_MOTORS; i++) {
            motorValues[i] = buffer[i];
        }
        for (int i = 0; i < NUM_SERVOS; i++) {
            servoValues[i] = buffer[NUM_MOTORS + i];
        }
        for (int i = 0; i < NUM_STEPPERS; i++) {
            stepperValues[i] = buffer[NUM_MOTORS + NUM_SERVOS + i];
        }
    }
};

void setup() {
    Serial.begin(38400);
    while (!Serial) {
        delay(10);
    }
    Wire.begin();
    delay(2000);

    if (!bno.begin()) {
        Serial.println("Failed to initialize BNO! Check your connections.");
        while (1) {
            delay(10);
        }
    }

    Serial.println("BNO initialized successfully!");

    for (int motorNum = 0; motorNum < NUM_MOTORS; motorNum++) {
        analogWrite(MOTOR_PINS[motorNum], 1500);
    }
}

void loop() {
    if (Serial.available() > 0) {
        Rx rx = Rx();

        for (int motorNum = 0; motorNum < NUM_MOTORS; motorNum++) {
            analogWrite(
                MOTOR_PINS[motorNum],
                rx.motorValues[motorNum]); // this hasnt been tested might need
                                           // to send 20000ms blocks instead.
                                           // see old code
        }

        Serial.printf("DEBUG, received motor values: %ld, %ld, %ld, %ld, %ld, "
                      "%ld, %ld, %ld\n",
                      rx.motorValues[0], rx.motorValues[1], rx.motorValues[2],
                      rx.motorValues[3], rx.motorValues[4], rx.motorValues[5],
                      rx.motorValues[6], rx.motorValues[7]);
    }
}
