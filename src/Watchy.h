#ifndef WATCHY_H
#define WATCHY_H

#include <Arduino.h>
#include <WiFiManager.h>
#include <HTTPClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Arduino_JSON.h>
#include <GxEPD2_BW.h>
#include <Wire.h>
//#include <Fonts/FreeMonoBold9pt7b.h>
//#include <Fonts/FreeMonoBold24pt7b.h>
//#include "FreeMonoBold8pt7b.h"
#include <Fonts/includefont.h>
#include "DSEG7_Classic_Bold_53.h"
#include "DSEG7_Classic_Bold_25.h"
#include "AFreeMonoBold8pt7b.h"
#include "Display.h"
#include "WatchyRTC.h"
#include "BLE.h"
#include "bma.h"
#include "config.h"
#include "Picture.h"
#include <iostream>
#include "Todoist.h"


typedef struct weatherData {
  int8_t temperature;
  int16_t weatherConditionCode;
  bool isMetric;
  String weatherDescription;
  bool external;
} weatherData;

typedef struct watchySettings {
  // Weather Settings
  String cityID;
  String weatherAPIKey;
  String weatherURL;
  String weatherUnit;
  String weatherLang;
  int8_t weatherUpdateInterval;
  bool useInternalTemperature;
  // NTP Settings
  String ntpServer;
  int gmtOffset;
  //Vibrate Settings
  bool vibrateOClock;
  bool vibrateHalfOClock;

  String ssid[WIFI_STA_NB];
  String pwd[WIFI_STA_NB];

  String todoistToken;
  String todoistUrl;

} watchySettings;

typedef struct todoistData {

  char todoistTask[22];
  int todoistId;

} todoistData;

typedef void(*GeneralFunction) ();

typedef struct guiData {
  int guiId;
  int nextGuiId[4];
  const GeneralFunction buttonFunction[4];

} guiData;




class Watchy {
public:
  static WatchyRTC RTC;
  static GxEPD2_BW<WatchyDisplay, WatchyDisplay::HEIGHT> display;
  tmElements_t currentTime;
  watchySettings settings;
  

public:
  explicit Watchy(const watchySettings &s) : settings(s) {} // constructor
  void init(String datetime = "");
  void initGuiData();
  void deepSleep();
  static void displayBusyCallback(const void *);
  float getBatteryVoltage();
  void vibMotor(uint8_t intervalMs = 100, uint8_t length = 20);
  u_int16_t getTextColor();
  u_int16_t getBackColor();
  void refreshData();
  void noRefreshData();

  virtual void handleButtonPress();
  void showCalendar(tmElements_t calendarTime);
  void showCalendarOLD(tmElements_t calendarTime);
  void showTodoist();
  void displayTodoist();
  bool getTodoistData(); 
  void showAlarme();
  void showSleep();
  void showMenu(byte menuIndex, bool partialRefresh);
  void showFastMenu(byte menuIndex);
  void showAbout();
  void showBuzz();
  void showAccelerometer();
  void showUpdateFW();
  void showSyncNTP();
  bool syncNTP();
  bool syncNTP(long gmt);
  bool syncNTP(long gmt, String ntpServer);
  void setTime();
  void setupWifi();
  bool connectWiFi();
  void showScanWiFi();
  weatherData getWeatherData();
  weatherData getWeatherData(String cityID, String units, String lang,
                             String url, String apiKey, uint8_t updateInterval,
                             bool useInternalTemperature);
  void updateFWBegin();

  void showWatchFace(bool partialRefresh);
  virtual void drawWatchFace(); // override this method for different watch
                                // faces

private:
  void _bmaConfig();
  static void _configModeCallback(WiFiManager *myWiFiManager);
  static uint16_t _readRegister(uint8_t address, uint8_t reg, uint8_t *data,
                                uint16_t len);
  static uint16_t _writeRegister(uint8_t address, uint8_t reg, uint8_t *data,
                                 uint16_t len);
  int startDayOfWeek(int y, int m, int d);
  String dayToString(int _day);
};

extern RTC_DATA_ATTR int guiState;
extern RTC_DATA_ATTR int menuIndex;
extern RTC_DATA_ATTR BMA423 sensor;
extern RTC_DATA_ATTR bool WIFI_CONFIGURED;
extern RTC_DATA_ATTR bool BLE_CONFIGURED;

#endif
