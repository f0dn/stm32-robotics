#include <Arduino.h>

void setup()
{
    Serial.begin(115200);

    pinMode(LED_BUILTIN, OUTPUT);
}

void loop()
{
    while (true)
    {
        Serial.println("on");
        digitalWrite(LED_BUILTIN, HIGH);
        delay(1000);
        Serial.println("off");
        digitalWrite(LED_BUILTIN, LOW);
        delay(1000);
    }
}
