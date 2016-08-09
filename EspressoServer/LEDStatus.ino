#include "LEDStatus.h"

LEDStatus::LEDStatus(int rPin, int mPin, int mValue) // constructor
{
  muxPin = mPin;
  muxValue = mValue;
  readPin = rPin;
  avgVal = 0;
}

void LEDStatus::update()
{
  // set mux
  digitalWrite(muxPin,muxValue);

  // read the pin value
  int val = analogRead(readPin);

  int currentCount = values.count();

  // if we don't have enough values yet, just add to the queue
  if (currentCount < TOTAL_LED_VALUES) {
    values.enqueue(val);
    // calculate new average
    avgVal = (avgVal * (currentCount) + val) / (float)(currentCount + 1);
    /*
    Serial.print(val);
    Serial.print(",");
    Serial.println(avgVal);
    */
  }
  else {
    // enqueue new value
    values.enqueue(val);
    // dequeue old value
    int oldVal = values.dequeue();
    // calculate new average
    avgVal = (avgVal * (currentCount) - oldVal + val) / (float)currentCount;
  }
}

LED_STATUS LEDStatus::status()
{
  /*
  if (muxValue == 0) {
    Serial.print("Power Avg: ");
  }
  else {
    Serial.print("Steam Avg: ");
  }
  Serial.println(avgVal);
  */
  if (avgVal > onRange) {
    return LED_ON;
  }
  if (avgVal > blinkRange) {
    return LED_BLINK;
  }
  return LED_OFF;
}

