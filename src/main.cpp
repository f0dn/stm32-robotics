#include <Adafruit_BNO055.h>
#include <Arduino.h>
#include <STM32FreeRTOS.h>
#include <Wire.h>

#define NUM_MOTORS 8
#define NUM_SERVOS 5
#define NUM_STEPPERS 3
#define TASK_DELAY 100

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

/*
  The data to be sent to the jetson from the microcontroller via UART.

  Currently just IMU orientation data
*/
class Tx {
  public:
    sensors_event_t orientation;

    Tx() { bno.getEvent(&orientation); }
};

SemaphoreHandle_t serialMutex;
QueueHandle_t rxQueue;
QueueHandle_t txQueue;

void sendMotors(void *params);
void readIMU(void *params);
void sendSerial(void *params);
void readSerial(void *params);

void initSemaphore(SemaphoreHandle_t &sem) {
    if (sem == NULL) {
        sem = xSemaphoreCreateMutex();
        if (sem == NULL) {
            Serial.println("Failed to create semaphore!");
            while (1) {
                delay(10);
            }
        }
        xSemaphoreGive(sem);
    }
}

void initQueue(QueueHandle_t &queue, int length, int itemSize) {
    if (queue == NULL) {
        queue = xQueueCreate(length, itemSize);
        if (queue == NULL) {
            Serial.println("Failed to create queue!");
            while (1) {
                delay(10);
            }
        }
    }
}

void setup() {
    Serial.begin(38400);
    while (!Serial) {
        delay(10);
    }

    if (!bno.begin()) {
        Serial.println("Failed to initialize BNO!");
        while (1) {
            delay(10);
        }
    }

    initSemaphore(serialMutex);
    initQueue(rxQueue, 8, sizeof(Rx));
    initQueue(txQueue, 8, sizeof(Tx));

    for (int motorNum = 0; motorNum < NUM_MOTORS; motorNum++) {
        analogWrite(MOTOR_PINS[motorNum], 1500);
    }

    xTaskCreate(sendMotors, "Send Motors", 256, NULL, 1, NULL);
    xTaskCreate(readIMU, "Read IMU", 256, NULL, 1, NULL);
    xTaskCreate(sendSerial, "Send Serial", 256, NULL, 1, NULL);
    xTaskCreate(readSerial, "Read Serial", 256, NULL, 1, NULL);

    vTaskStartScheduler();
    Serial.println("Insufficient RAM");
    while (1) {
    }
}

void loop() {}

void sendMotors(void *params) {
    Rx rx;
    while (1) {
        if (xQueueReceive(rxQueue, &rx, portMAX_DELAY) == pdPASS) {
            for (int motorNum = 0; motorNum < NUM_MOTORS; motorNum++) {
                analogWrite(
                    MOTOR_PINS[motorNum],
                    rx.motorValues[motorNum]); // this hasnt been tested might
                                               // need to send 20000ms blocks
                                               // instead. see old code
            }

            // TODO - add code to control servos and stepper motors
        }
    }
}

void readIMU(void *params) {
    Tx tx;
    while (1) {
        tx = Tx();
        xQueueSend(txQueue, &tx, portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(TASK_DELAY));
    }
}

void sendSerial(void *params) {
    Tx tx;
    while (1) {
        if (xQueueReceive(txQueue, &tx, portMAX_DELAY) == pdPASS) {
            xSemaphoreTake(serialMutex, portMAX_DELAY);
            Serial.write((uint8_t *)&tx.orientation, sizeof(tx.orientation));
            xSemaphoreGive(serialMutex);
        }
    }
}

void readSerial(void *params) {
    Rx rx;
    while (1) {
        xSemaphoreTake(serialMutex, portMAX_DELAY);
        if (Serial.available() > 0) {
            rx = Rx();
            xQueueSend(rxQueue, &rx, portMAX_DELAY);
            vTaskDelay(pdMS_TO_TICKS(TASK_DELAY));
        }
        xSemaphoreGive(serialMutex);
    }
}
