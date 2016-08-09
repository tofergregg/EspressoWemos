#include "EspTimer.h"
#include <string.h>

EspTimer::EspTimer()
{
  count = 0;
}

EspTimer::EspTimer(String filename) // constructor to read in file
{
  count = 0;
  File dataFile = SD.open((char *)timesFilename);
  if (dataFile){
    while (dataFile.available()){
        String line;
        while (dataFile.available()) { // in case we don't end with a newline
        // read until we get a newline
        char b = dataFile.read();
        if (b == '\n') break;
        line += b;
      }
        addSetting(String(line));
    }
  }
  Serial.println("Done reading "+filename);
}
void EspTimer::addSettings(String settings)
{
  count = 0; // reset if we are adding the settings
  String setting,*savePtr=NULL;
  // walk through string until we find the first newline
  for (int i=0;i<settings.length();i++) {
    if (settings[i] == '\n'){
      Serial.println(String("adding:")+setting+":");
      addSetting(setting);
      setting = "";
    }
    else {
      setting += settings[i];
    }
  }
}
void EspTimer::addSetting(String settingStr)
{
  // format:
  // Sunday,06,45,off,Steam
  // Monday,05,30,off,Steam
  // Tuesday,05,30,on,No Steam
  // Wednesday,05,30,off,Steam
  // Thursday,05,50,on,No Steam
  // Friday,05,30,off,Steam
  // Saturday,06,45,off,Steam
  
  char *setting = (char *)settingStr.c_str();
  char *nextToken,*savePtr;
  settings[count].day = strtok_r(setting,",",&savePtr);
  settings[count].hour = atoi(strtok_r(NULL,",",&savePtr));
  settings[count].minute = atoi(strtok_r(NULL,",",&savePtr));
  settings[count].on = strtok_r(NULL,",",&savePtr);
  settings[count].steam = strtok_r(NULL,",",&savePtr);
  count++;
}

// returns whether we should power on the machine.
// will return true if we are within the minute passed in 
bool EspTimer::shouldPowerOn(int day_of_week,int hour, int minute)
{
  // check all times (this may be necessary later, if we enhance
  // the program to have more than one timer per day

  // there is conflicting information online about this, but empirically,
  // Sunday is day 1.
  String days[7] = {"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};
  for (int i=0;i<count;i++){
    Setting s = settings[i];
    if (s.day == days[day_of_week-1] && s.hour == hour && s.minute == minute 
        && s.on == "on") {
      return true;
    }
  }
  return false;
}

String EspTimer::fullString()
{
  String fs;
  for (int i=0;i<count;i++){
    fs += toString(i) + "\n";
  }
  return fs;
}


String EspTimer::toString(int day)
{
  String settingStr;
  Setting s = settings[day]; // day is 0-6 (0==Sunday)
  settingStr = s.day;
  settingStr += ",";
  if (s.hour < 10) {
    settingStr += "0";
  }
  settingStr+=String(s.hour)+",";
  if (s.minute < 10) {
    settingStr += "0";
  }
  settingStr +=String(s.minute)+",";
  settingStr += s.on + ",";
  settingStr += s.steam;
  return settingStr;
}

bool EspTimer::saveTimes()
{
  if(SD.exists((char *)timesFilename)) SD.remove((char *)timesFilename);
  File f = SD.open((char *)timesFilename, FILE_WRITE);
  if (f){
    for (int i=0;i<count;i++){
      String timeStr = toString(i)+"\n";
      f.write(timeStr.c_str());
    }
    Serial.println("Done writing times to SD card.");
    f.close();
    return true;
  }
  else {
    Serial.println("Could not write times to SD card!");
    return false;
  }
}

