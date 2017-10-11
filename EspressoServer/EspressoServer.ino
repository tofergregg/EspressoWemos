/*
  SDWebServer - Example WebServer with SD Card backend for esp8266

  Copyright (c) 2015 Hristo Gochkov. All rights reserved.
  This file is part of the ESP8266WebServer library for Arduino environment.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

  Have a FAT Formatted SD Card connected to the SPI port of the ESP8266
  The web root is the SD Card root folder
  File extensions with more than 3 charecters are not supported by the SD Library
  File Names longer than 8 charecters will be truncated by the SD library, so keep filenames shorter
  index.htm is the default index (works on subfolders as well)

  upload the contents of SdRoot to the root of the SDcard and access the editor by going to http://esp8266sd.local/edit

*/
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <SPI.h>
#include <SD.h>
#include <TimeLib.h>
#include "LEDStatus.h"
#include "EspTimer.h"

bool cgiRequest(String path);

#define DBG_OUTPUT_PORT Serial

//const char* ssid = "EECS";
//const char* password = "";
const char* ssid = "Emerson St.";
const char* password = "ThomasEdison";
const char* host = "esp8266sd";
int timezone = -4; // EDT
EspTimer *allTimes;

ESP8266WebServer server(80);

static bool hasSD = false;
File uploadFile;

int serialByte;
const int MUX = D1;
const int LED_IN = A0;
const int PWRBUT = D3;
const int STMBUT = D4;

// queues to hold recent LED values
LEDStatus power_led(LED_IN,MUX,0);
LEDStatus steam_led(LED_IN,MUX,1);

int currentMux = 0; // start at off

void returnOK() {
  server.send(200, "text/plain", "");
}

void returnFail(String msg) {
  server.send(500, "text/plain", msg + "\r\n");
}

bool loadFromSdCard(String path){
  if (path.endsWith(".cgi")) {
    return cgiRequest(path);
  }
  String dataType = "text/plain";
  if(path.endsWith("/")) path += "index.htm";

  if(path.endsWith(".src")) path = path.substring(0, path.lastIndexOf("."));
  else if(path.endsWith(".htm")) dataType = "text/html";
  else if(path.endsWith(".css")) dataType = "text/css";
  else if(path.endsWith(".js")) dataType = "application/javascript";
  else if(path.endsWith(".png")) dataType = "image/png";
  else if(path.endsWith(".gif")) dataType = "image/gif";
  else if(path.endsWith(".jpg")) dataType = "image/jpeg";
  else if(path.endsWith(".ico")) dataType = "image/x-icon";
  else if(path.endsWith(".xml")) dataType = "text/xml";
  else if(path.endsWith(".pdf")) dataType = "application/pdf";
  else if(path.endsWith(".zip")) dataType = "application/zip";

  File dataFile = SD.open(path.c_str());
  if(dataFile.isDirectory()){
    path += "/index.htm";
    dataType = "text/html";
    dataFile = SD.open(path.c_str());
  }

  if (!dataFile)
    return false;

  if (server.hasArg("download")) dataType = "application/octet-stream";

  // send a cache header
  if (dataType == "image/jpeg" or dataType == "image/png") {
    server.sendHeader("Cache-Control","max-age=31536000, public");
  }
  if (server.streamFile(dataFile, dataType) != dataFile.size()) {
    DBG_OUTPUT_PORT.println("Sent less data than expected!");
  }

  dataFile.close();
  return true;
}

bool getServerTime()
{
  long utc_time = now()-3600 * timezone;
  server.send(200, "text/plain", String(utc_time));
  return true;
}

bool getTimes(){
  server.send(200,"text/plain",allTimes->fullString());
  return true;
}

bool setClock()
{
  // server should have two arguments:
  // epoch: the unix epoch in seconds
  // tz: the timezone offset (e.g., -4 for EDT)
  String epoch = server.arg("epoch");
  String tz = server.arg("timezone");
  Serial.println("Epoch: "+epoch);
  Serial.println("Timezone offset: "+tz);
  server.send(200,"text/plain",epoch+","+tz);
  timezone = tz.toInt();
  // save to file
  // first, remove the old file
  SD.remove("timezone.txt"); // don't worry if it fails
  File dataFile = SD.open("timezone.txt",FILE_WRITE);
  if (dataFile){
    dataFile.write(tz.c_str(),tz.length());
    dataFile.write("\n");
    dataFile.close();
  }
  // set time
  long unixTime = epoch.toInt() + 3600*timezone;
  Serial.println(unixTime);
  setTime(unixTime);
  return true;
}
bool setTimes(){
  String times = server.arg("times");
  allTimes->addSettings(times);
  Serial.println("New Times:");
  for (int i=0;i<7;i++){
      Serial.println(allTimes->toString(i));
  }
  allTimes->saveTimes();
  server.send(200,"text/plain","times saved");
  return true;
}

String statusString(LED_STATUS ls)
{
  return ls == LED_ON ? "on" : (ls == LED_BLINK ? "blink" : "off");
}

bool ledStatus()
{
  String led_to_check = server.arg("LED");
  if (led_to_check == "power") {
    server.send(200,"text/plain",statusString(power_led.status()));
  }
  if (led_to_check == "steam") {
    server.send(200,"text/plain",statusString(steam_led.status()));
  }
  return true;  
}

void turnOnMachine(){
  digitalWrite(PWRBUT,1);
  // delay for four seconds to turn on
  delay(4000);
  digitalWrite(PWRBUT,0);
}

void turnOffMachine(){
  digitalWrite(PWRBUT,1);
  // only delay for 250ms to turn off
  delay(250);
  digitalWrite(PWRBUT,0);
}

void turnOnSteam(){
  digitalWrite(STMBUT,1);
  // only delay for 250ms to turn on
  delay(250);
  digitalWrite(STMBUT,0);
}

void turnOffSteam(){
  digitalWrite(STMBUT,1);
  // delay for four seconds to turn off
  delay(4000);
  digitalWrite(STMBUT,0);
}

bool buttonPress()
{
  String button_function = server.arg("function");
  if (button_function == "power_on"){
    turnOnMachine();
    server.send(200,"text/plain","powered on");

  }
  else if (button_function == "power_off"){
    turnOffMachine();
    server.send(200,"text/plain","powered off");

  }
  else if (button_function == "steam_on"){
    turnOnSteam();
    server.send(200,"text/plain","steam on");
  }
  else if (button_function == "steam_off"){
    turnOffSteam();
    server.send(200,"text/plain","steam off");
  }
  return true;
}

bool cgiRequest(String path){
  DBG_OUTPUT_PORT.println("requested cgi: "+path);
  
  if (path.startsWith("/getServerTime")) return getServerTime();
  if (path.startsWith("/getTimes")) return getTimes();
  if (path.startsWith("/setClock")) return setClock();
  if (path.startsWith("/setTimes")) return setTimes();
  if (path.startsWith("/LEDStatus")) return ledStatus();
  if (path.startsWith("/buttonPress")) return buttonPress(); 
  
  server.send(404, "text/plain", "Unknown cgi request.");
  return false;
}

void handleFileUpload(){
  if(server.uri() != "/edit") return;
  HTTPUpload& upload = server.upload();
  if(upload.status == UPLOAD_FILE_START){
    if(SD.exists((char *)upload.filename.c_str())) SD.remove((char *)upload.filename.c_str());
    uploadFile = SD.open(upload.filename.c_str(), FILE_WRITE);
    DBG_OUTPUT_PORT.print("Upload: START, filename: "); DBG_OUTPUT_PORT.println(upload.filename);
  } else if(upload.status == UPLOAD_FILE_WRITE){
    if(uploadFile) uploadFile.write(upload.buf, upload.currentSize);
    DBG_OUTPUT_PORT.print("Upload: WRITE, Bytes: "); DBG_OUTPUT_PORT.println(upload.currentSize);
  } else if(upload.status == UPLOAD_FILE_END){
    if(uploadFile) uploadFile.close();
    DBG_OUTPUT_PORT.print("Upload: END, Size: "); DBG_OUTPUT_PORT.println(upload.totalSize);
  }
}

void deleteRecursive(String path){
  File file = SD.open((char *)path.c_str());
  if(!file.isDirectory()){
    file.close();
    SD.remove((char *)path.c_str());
    return;
  }

  file.rewindDirectory();
  while(true) {
    File entry = file.openNextFile();
    if (!entry) break;
    String entryPath = path + "/" +entry.name();
    if(entry.isDirectory()){
      entry.close();
      deleteRecursive(entryPath);
    } else {
      entry.close();
      SD.remove((char *)entryPath.c_str());
    }
    yield();
  }

  SD.rmdir((char *)path.c_str());
  file.close();
}

void handleDelete(){
  if(server.args() == 0) return returnFail("BAD ARGS");
  String path = server.arg(0);
  if(path == "/" || !SD.exists((char *)path.c_str())) {
    returnFail("BAD PATH");
    return;
  }
  deleteRecursive(path);
  returnOK();
}

void handleCreate(){
  if(server.args() == 0) return returnFail("BAD ARGS");
  String path = server.arg(0);
  if(path == "/" || SD.exists((char *)path.c_str())) {
    returnFail("BAD PATH");
    return;
  }

  if(path.indexOf('.') > 0){
    File file = SD.open((char *)path.c_str(), FILE_WRITE);
    if(file){
      file.write((const char *)0);
      file.close();
    }
  } else {
    SD.mkdir((char *)path.c_str());
  }
  returnOK();
}

void printDirectory() {
  if(!server.hasArg("dir")) return returnFail("BAD ARGS");
  String path = server.arg("dir");
  if(path != "/" && !SD.exists((char *)path.c_str())) return returnFail("BAD PATH");
  File dir = SD.open((char *)path.c_str());
  path = String();
  if(!dir.isDirectory()){
    dir.close();
    return returnFail("NOT DIR");
  }
  dir.rewindDirectory();
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/json", "");
  WiFiClient client = server.client();

  server.sendContent("[");
  for (int cnt = 0; true; ++cnt) {
    File entry = dir.openNextFile();
    if (!entry)
    break;

    String output;
    if (cnt > 0)
      output = ',';

    output += "{\"type\":\"";
    output += (entry.isDirectory()) ? "dir" : "file";
    output += "\",\"name\":\"";
    output += entry.name();
    output += "\"";
    output += "}";
    server.sendContent(output);
    entry.close();
 }
 server.sendContent("]");
 dir.close();
}

void handleNotFound(){
  if(hasSD && loadFromSdCard(server.uri())) return;
  String message = "Could not load file...\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " NAME:"+server.argName(i) + "\n VALUE:" + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
  DBG_OUTPUT_PORT.print(message);
}
void setTimeFromEcosimulation()
{
  const char *host = "ecosimulation.com";
  const char *url = "/espresso/getdate.cgi"; // gets UTC from server
  Serial.print("connecting to ");
  Serial.println(host);
  
  // Use WiFiClient class to create TCP connections
  WiFiClient client;
  const int httpPort = 80;
  if (!client.connect(host, httpPort)) {
    Serial.println("connection failed");
    return;
  }
  
  Serial.print("Requesting URL: ");
  Serial.println(url);
  
  // This will send the request to the server
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" + 
               "Connection: close\r\n\r\n");
               
  // Wait for data from client to become available
  while (client.connected() && !client.available()) {
    // delay up to 5 seconds, but fail after
    static int waiting = 0;
    if (waiting > 500) break; 
    delay(10);
    waiting++;
  }
  
  // Read all the lines of the reply from server and print them to Serial
  while(client.available()){
    String line = client.readStringUntil('\r');
    int time_idx = line.indexOf("TIME");
    if (time_idx != -1) {
      line.remove(0,time_idx+4);
      long unixTime = line.toInt()+3600*timezone;
      Serial.println(unixTime);
      setTime(unixTime);
    }
  }
  
  Serial.println();
  Serial.println("closing connection");
}

int loadTimezone(){
  File dataFile = SD.open("timezone.txt");
  if (dataFile){
    String line;
    while (dataFile.available()){
        // read until we get a newline
        char b = dataFile.read();
        if (b == '\n') break;
        line += b;
    }
    dataFile.close();
    Serial.println("Timezone: "+line);
    return line.toInt();
  }
  return 0; // probably not good!
}

void setup(void){
  DBG_OUTPUT_PORT.begin(115200);
  DBG_OUTPUT_PORT.setDebugOutput(true);
  DBG_OUTPUT_PORT.print("\n");

  // set up pins
  pinMode(PWRBUT,OUTPUT);
  pinMode(STMBUT,OUTPUT);
  digitalWrite(PWRBUT,0);
  digitalWrite(STMBUT,0);

  pinMode(MUX,OUTPUT);
  digitalWrite(MUX,currentMux);

  pinMode(LED_IN,INPUT);
  
  WiFi.begin(ssid, password);
  DBG_OUTPUT_PORT.print("Connecting to ");
  DBG_OUTPUT_PORT.println(ssid);

  // Wait for connection
  uint8_t i = 0;
  while (WiFi.status() != WL_CONNECTED && i++ < 20) {//wait 10 seconds
    delay(500);
  }
  if(i == 21){
    DBG_OUTPUT_PORT.print("Could not connect to");
    DBG_OUTPUT_PORT.println(ssid);
    while(1) delay(500);
  }
  DBG_OUTPUT_PORT.print("Connected! IP address: ");
  DBG_OUTPUT_PORT.println(WiFi.localIP());

  if (MDNS.begin(host)) {
    MDNS.addService("http", "tcp", 80);
    DBG_OUTPUT_PORT.println("MDNS responder started");
    DBG_OUTPUT_PORT.print("You can now connect to http://");
    DBG_OUTPUT_PORT.print(host);
    DBG_OUTPUT_PORT.println(".local");
  }

  server.on("/list", HTTP_GET, printDirectory);
  server.on("/edit", HTTP_DELETE, handleDelete);
  server.on("/edit", HTTP_PUT, handleCreate);
  server.on("/edit", HTTP_POST, [](){ returnOK(); }, handleFileUpload);
  server.onNotFound(handleNotFound);

  server.begin();
  DBG_OUTPUT_PORT.println("HTTP server started");

  if (SD.begin(SS)){
     DBG_OUTPUT_PORT.println("SD Card initialized.");
     hasSD = true;
  }
  else {
    DBG_OUTPUT_PORT.println("Could not initialize SD card!");
    hasSD = false;
  }
  if (hasSD) {
    allTimes = new EspTimer("times.txt");
    for (int i=0;i<7;i++){
      Serial.println(allTimes->toString(i));
    }
    timezone = loadTimezone();
    Serial.println("timezone after loading:"+String(timezone));
  }
  // set the timer
  setTimeFromEcosimulation();
}

void printDigits(int digits){
  // utility function for digital clock display: prints preceding colon and leading 0
  Serial.print(":");
  if(digits < 10)
    Serial.print('0');
  Serial.print(digits);
}

void digitalClockDisplay(){
  // digital clock display of the time
  Serial.print(hour());
  printDigits(minute());
  printDigits(second());
  Serial.print(" ");
  Serial.print(day());
  Serial.print(" ");
  Serial.print(month());
  Serial.print(" ");
  Serial.print(year()); 
  Serial.println(); 
}

void toggleMux(){
  currentMux == 0 ? currentMux = 1 : currentMux = 0;
  digitalWrite(MUX,currentMux);
}

void updateLEDs(){
  power_led.update();
  steam_led.update();
}

void check_for_power_on()
{
  static int last_minute = 0;
  int day_of_week = weekday();
  int current_hour = hour();
  int current_min = minute();

  if (last_minute == current_min) {
    return; // checked this minute already
  }
  Serial.println(day_of_week);
  Serial.println(current_hour);
  Serial.println(current_min);
  
  if (allTimes->shouldPowerOn(day_of_week,current_hour, current_min)) {
    Serial.println("Turning on the machine!");
    turnOnMachine();
  }
  else {
    Serial.println("nope...");
  }
  last_minute = current_min;
}

void loop(void){
  static int lastSecond = -1;
  static int oldMillis = 0;

  int newMillis = millis();
  int newSecond;
  server.handleClient();
  newSecond = second();
  // print every 5 seconds
  if (newSecond >= lastSecond) {
    check_for_power_on();
    if (newSecond - lastSecond > 4) {
      digitalClockDisplay();
      LED_STATUS power_status = power_led.status();
      LED_STATUS steam_status = steam_led.status();
      Serial.println("Power: "+statusString(power_status));
      Serial.println("Steam: "+statusString(steam_status)+"\n");
      lastSecond = newSecond;
    }
  }
  else {
    // we've wrapped the clock
    if (newSecond + 60 - lastSecond > 4) {
      digitalClockDisplay();
      lastSecond = newSecond; 
    }
  }
  // read serial for testing
  if (Serial.available() > 0) {
                // read the incoming byte:
                serialByte = Serial.read();

                // say what you got:
                Serial.print("I received: ");
                Serial.println(serialByte, DEC);
                switch(serialByte) {
                  case 'p' : turnOnMachine(); // turn on
                    break;
                  case 'P' : turnOffMachine(); // turn off
                    break;
                  case 's' : turnOnSteam();
                    break;
                  case 'S' : turnOffSteam();
                    break; 
                  case 't' : toggleMux();
                    break;
                }

  }
  if (newMillis - oldMillis > 20) {
    // update LEDs about 50 times per second
    updateLEDs();
    oldMillis = newMillis;
  }

}
