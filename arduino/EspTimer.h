#ifndef ESPTIMER_H
#define ESPTIMER_H

#include<SD.h>

const int DAYS = 7;
const char *timesFilename = "times.txt";

struct Setting {
    String day;
    int hour;
    int minute;
    String on;
    String steam;
};

class EspTimer {
  public:
    EspTimer();
    EspTimer(String filename); // constructor to read in file
    void addSettings(String settings);
    void addSetting(String settingstr);
    bool shouldPowerOn(int day_of_week,int hour, int minute);
    String toString(int day);
    String fullString();
    bool saveTimes();
    
  private:
    int count;
    Setting settings[DAYS]; // seven day timer  
  };
#endif
 
