#include <Arduino.h>

#define NUM_MOTORS 8

const uint16_t MOTOR_PINS[NUM_MOTORS] = {GPIO_PIN_8, GPIO_PIN_9, GPIO_PIN_10,
                                         GPIO_PIN_11, GPIO_PIN_12, GPIO_PIN_13,
                                         GPIO_PIN_14, GPIO_PIN_15};

/*
  The data received by the microcontroller from UART.

  This is reconstructed from a CSV string in the form of:
  ```
  m1,m2,m3,m4,m5,m6,m7,m8
  ```
  where m_i is the value of the i-th motor, in the range [0, 255].
*/
class Rx
{
public:
    int64_t motorValues[NUM_MOTORS];

    Rx(const String &csv)
    {
        int start = 0;
        for (int i = 0; i < NUM_MOTORS; i++)
        {
            int end = csv.indexOf(',', start);
            if (end == -1)
                end = csv.length();
            motorValues[i] = csv.substring(start, end).toInt();
            start = end + 1;
        }
    }
};

void setup()
{
    Serial.begin(38400);
    pinMode(LED_BUILTIN, OUTPUT);
}

void loop()
{
    if (Serial.available() > 0)
    {
        String csv = Serial.readStringUntil('\n');
        Rx rx(csv);

        for (int motorNum = 0; motorNum < NUM_MOTORS; motorNum++)
            digitalWrite(MOTOR_PINS[motorNum], rx.motorValues[motorNum] > 0 ? HIGH : LOW);
    }
}
