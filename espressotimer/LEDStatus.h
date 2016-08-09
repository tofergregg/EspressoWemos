#ifndef LEDSTATUS_H
#define LEDSTATUS_H

#include<QueueArray.h>

const int TOTAL_LED_VALUES = 200;
const float onRange = 350.0;
const float blinkRange = 50.0;
const float offRange = 0.0;

enum LED_STATUS { LED_OFF, LED_BLINK, LED_ON };

class LEDStatus {
  public:
    LEDStatus(int rPin, int mPin, int mValue); // constructor
    void update();
    LED_STATUS status();
    
  private:
    int readPin;
    int muxPin;
    int muxValue;
    float avgVal;
    QueueArray<int> values;
};
#endif
