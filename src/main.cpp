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
    bool isValid = true;
    int32_t motorValues[NUM_MOTORS];

    Rx(const String &csv)
    {
        // Serial.println("DEBUG, parsing CSV string: " + csv);
        // parse exactly MOTOR_NUMBERS values from the CSV string
        int startIndex = 0;
        for (int motorNum = 0; motorNum < NUM_MOTORS; motorNum++)
        {
            int commaIndex = csv.indexOf(',', startIndex);
            if (commaIndex == -1 && motorNum < NUM_MOTORS - 1)
            {
                Serial.println("DEBUG, not enough values in CSV string: " + csv + " at motor number: " + String(motorNum));
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

void setup()
{
    Serial.begin(115200);
    pinMode(LED_BUILTIN, OUTPUT);
}

void loop()
{
    if (Serial.available() > 0)
    {
        String csv = Serial.readStringUntil('\n');
        if (csv.length() == 0)
            return;

        Rx rx(csv);

        if (!rx.isValid)
        {
            Serial.println("WARN, invalid CSV string received: " + csv);
            return;
        }

        for (int motorNum = 0; motorNum < NUM_MOTORS; motorNum++)
            digitalWrite(MOTOR_PINS[motorNum], rx.motorValues[motorNum] > 0 ? HIGH : LOW);

        Serial.printf("DEBUG, received motor values: %ld, %ld, %ld, %ld, %ld, %ld, %ld, %ld\n",
                      rx.motorValues[0], rx.motorValues[1], rx.motorValues[2], rx.motorValues[3],
                      rx.motorValues[4], rx.motorValues[5], rx.motorValues[6], rx.motorValues[7]);
    }
}
