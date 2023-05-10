#include "Watchy.h"

WatchyRTC Watchy::RTC;
GxEPD2_BW<WatchyDisplay, WatchyDisplay::HEIGHT> Watchy::display(
    WatchyDisplay(DISPLAY_CS, DISPLAY_DC, DISPLAY_RES, DISPLAY_BUSY));

RTC_DATA_ATTR int guiState;
RTC_DATA_ATTR int menuIndex;
RTC_DATA_ATTR BMA423 sensor;
RTC_DATA_ATTR bool WIFI_CONFIGURED;
RTC_DATA_ATTR bool BLE_CONFIGURED;
RTC_DATA_ATTR weatherData currentWeather;
RTC_DATA_ATTR int weatherIntervalCounter = -1;
RTC_DATA_ATTR bool displayFullInit       = true;
RTC_DATA_ATTR long gmtOffset = 0;
RTC_DATA_ATTR bool alreadyInMenu         = true;
RTC_DATA_ATTR tmElements_t bootTime;
RTC_DATA_ATTR tmElements_t CalendarTime;
RTC_DATA_ATTR todoistData todoistTask[TODOIST_TASK_MAX];
//guiData guiDataTab[GUI_NUMBER];



void Watchy::init(String datetime) {
  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause(); // get wake up reason
  Wire.begin(SDA, SCL);                         // init i2c
  RTC.init();
  Serial.begin(115200);

  // Init the display here for all cases, if unused, it will do nothing
  display.epd2.selectSPI(SPI, SPISettings(20000000, MSBFIRST, SPI_MODE0)); // Set SPI to 20Mhz (default is 4Mhz)
  display.init(0, displayFullInit, 10,
               true); // 10ms by spec, and fast pulldown reset
  display.epd2.setBusyCallback(displayBusyCallback);

  switch (wakeup_reason) {
  case ESP_SLEEP_WAKEUP_EXT0: // RTC Alarm
    RTC.read(currentTime);
    switch (guiState) {
    case WATCHFACE_STATE:
      showWatchFace(true); // partial updates on tick
      if (settings.vibrateOClock) {
        if (currentTime.Minute == 0) {
          // The RTC wakes us up once per minute
          vibMotor(75, 4);
        }
      }
      if (settings.vibrateHalfOClock) {
        if (currentTime.Minute == 30) {
          // The RTC wakes us up once per minute
          vibMotor(75, 2);
        }
      }
      break;
    case MAIN_MENU_STATE:
      // Return to watchface if in menu for more than one tick
      if (alreadyInMenu) {
        guiState = WATCHFACE_STATE;
        showWatchFace(false);
      } else {
        alreadyInMenu = true;
      }
      break;
    }
    break;
  case ESP_SLEEP_WAKEUP_EXT1: // button Press
    handleButtonPress();
    break;
  default: // reset
    RTC.config(datetime);
    _bmaConfig();
    gmtOffset = settings.gmtOffset;
    RTC.read(currentTime);
    RTC.read(bootTime);
    showWatchFace(false); // full update on reset
    vibMotor(75, 10);
    break;
  }
  deepSleep();
}

void Watchy::initGuiData() {
  
}


void Watchy::displayBusyCallback(const void *) {
  gpio_wakeup_enable((gpio_num_t)DISPLAY_BUSY, GPIO_INTR_LOW_LEVEL);
  esp_sleep_enable_gpio_wakeup();
  esp_light_sleep_start();
}

void Watchy::deepSleep() {
  display.hibernate();
  if (displayFullInit) // For some reason, seems to be enabled on first boot
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  displayFullInit = false; // Notify not to init it again
  RTC.clearAlarm();        // resets the alarm flag in the RTC

  // Set GPIOs 0-39 to input to avoid power leaking out
  const uint64_t ignore = 0b11110001000000110000100111000010; // Ignore some GPIOs due to resets
  for (int i = 0; i < GPIO_NUM_MAX; i++) {
    if ((ignore >> i) & 0b1)
      continue;
    pinMode(i, INPUT);
  }
  if(guiState != SLEEP_STATE)
  {
    esp_sleep_enable_ext0_wakeup((gpio_num_t)RTC_INT_PIN,0); // enable deep sleep wake on RTC interrupt
  }

  esp_sleep_enable_ext1_wakeup(
      BTN_PIN_MASK,
      ESP_EXT1_WAKEUP_ANY_HIGH); // enable deep sleep wake on button press
  esp_deep_sleep_start();
}

void Watchy::handleButtonPress() {
  uint64_t wakeupBit = esp_sleep_get_ext1_wakeup_status();
  Serial.println(wakeupBit);
  // Menu Button
  if (wakeupBit & MENU_BTN_MASK) {
    if (guiState ==
        WATCHFACE_STATE) { // enter menu state if coming from watch face
      showMenu(menuIndex, true);
    } else if (guiState ==
               MAIN_MENU_STATE) { // if already in menu, then select menu item
      switch (menuIndex) {
      case 0:
        showAbout();
        break;
      case 1:
        showScanWiFi();
        //showBuzz();
        break;
      case 2:
        showAccelerometer();
        break;
      case 3:
        setTime();
        break;
      case 4:
        setupWifi();
        break;
      case 5:
        showUpdateFW();
        break;
      case 6:
        showSyncNTP();
        break;
      default:
        break;
      }
    } else if (guiState == FW_UPDATE_STATE) {
      updateFWBegin();
    }else if (guiState == CALENDAR_STATE) {
      return;
    }
  }
  // Back Button
  else if (wakeupBit & BACK_BTN_MASK) {
    if (guiState == MAIN_MENU_STATE) { // exit to watch face if already in menu
      RTC.read(currentTime);
      showWatchFace(true);
    } else if (guiState == APP_STATE) {
      showMenu(menuIndex, true); // exit to menu if already in app
    } else if (guiState == FW_UPDATE_STATE) {
      showMenu(menuIndex, true); // exit to menu if already in app
    } else if (guiState == WATCHFACE_STATE) {
      // display this month
      RTC.read(CalendarTime);
      showCalendar(CalendarTime);
      return;
    }else if ((guiState == CALENDAR_STATE)||(guiState == TODOIST_STATE)||(guiState == SLEEP_STATE)) {
      RTC.read(currentTime);
      showWatchFace(true);
      return;
    }

    
  }
  // Up Button
  else if (wakeupBit & UP_BTN_MASK) {
    if (guiState == MAIN_MENU_STATE) { // increment menu index
      menuIndex--;
      if (menuIndex < 0) {
        menuIndex = MENU_LENGTH - 1;
      }
      showMenu(menuIndex, true);
    } else if (guiState == WATCHFACE_STATE) {
      showTodoist();
      return;
    } else if (guiState == CALENDAR_STATE) {
      // display last month
      //RTC.read(CalendarTime);
      Serial.println("Last month");
      Serial.println(CalendarTime.Month);
      Serial.println(CalendarTime.Year);
      
      if (CalendarTime.Month == 1)
      {
        CalendarTime.Month = 12;
        CalendarTime.Year--;
      }
      else
      {
        CalendarTime.Month --;
      }
      Serial.println(CalendarTime.Month);
      Serial.println(CalendarTime.Year);
      showCalendar(CalendarTime);
      return;
    }
  }
  // Down Button
  else if (wakeupBit & DOWN_BTN_MASK) {
    if (guiState == MAIN_MENU_STATE) { // decrement menu index
      menuIndex++;
      if (menuIndex > MENU_LENGTH - 1) {
        menuIndex = 0;
      }
      showMenu(menuIndex, true);
    } else if (guiState == WATCHFACE_STATE) {
      showSleep();
      guiState = SLEEP_STATE;
      return;
    } else if (guiState == CALENDAR_STATE) {
      // display Next month
      //RTC.read(CalendarTime);
      Serial.println("next month");
      Serial.println(CalendarTime.Month);
      Serial.println(CalendarTime.Year);

      if (CalendarTime.Month == 12)
      {
        CalendarTime.Month = 1;
        CalendarTime.Year++;
      }
      else
      {
        CalendarTime.Month ++;
      }
      Serial.println(CalendarTime.Month);
      Serial.println(CalendarTime.Year);
      showCalendar(CalendarTime);
      return;
    }
  }
}

void Watchy::showMenu(byte menuIndex, bool partialRefresh) {
  display.setFullWindow();
  display.fillScreen(DARKMODE ? GxEPD_BLACK : GxEPD_WHITE);
  display.setFont(&FreeMonoBold9pt7b);

  int16_t x1, y1;
  uint16_t w, h;
  int16_t yPos;

  const char *menuItems[] = {
      "About Watchy", "Scan Wifi", "Show Accelerometer",
      "Set Time",     "Setup WiFi",    "Update Firmware",
      "Sync NTP"};
  for (int i = 0; i < MENU_LENGTH; i++) {
    yPos = MENU_HEIGHT + (MENU_HEIGHT * i);
    display.setCursor(0, yPos);
    if (i == menuIndex) {
      display.getTextBounds(menuItems[i], 0, yPos, &x1, &y1, &w, &h);
      display.fillRect(x1 - 1, y1 - 10, 200, h + 15, (DARKMODE ? GxEPD_WHITE : GxEPD_BLACK));
      display.setTextColor(DARKMODE ? GxEPD_BLACK : GxEPD_WHITE);
      display.println(menuItems[i]);
    } else {
      display.setTextColor(DARKMODE ? GxEPD_WHITE : GxEPD_BLACK);
      display.println(menuItems[i]);
    }
  }

  display.display(partialRefresh);

  guiState = MAIN_MENU_STATE;
  alreadyInMenu = false;
}

void Watchy::showAlarme() {

  int16_t x = 5;
  int16_t y = 10;
  int16_t x1, y1;
  uint16_t w, h;

  display.setFullWindow();
  //display.fillScreen(DARKMODE ? GxEPD_BLACK : GxEPD_WHITE);
  //display.setFont(&FreeMonoBold8pt7b);
  //display.setTextColor(DARKMODE ? GxEPD_WHITE : GxEPD_BLACK);
  //Centre calendar word
  //display.getTextBounds("ALARME",x,y,&x1,&y1,&w,&h);
  //display.setCursor((DISPLAY_WIDTH-w)/2, y);
  //display.println("ALARME");

  display.fillScreen(GxEPD_BLACK);
  display.drawBitmap(0, 0, pictureSleep, 200, 200,GxEPD_WHITE);
  
  
  display.display(true);

  
}

void Watchy::showSleep() {
  display.setFullWindow();
  display.fillScreen(GxEPD_BLACK);
  display.drawBitmap(0, 0, pictureSleep, 200, 200,GxEPD_WHITE);
  display.display(true);
}

void Watchy::showTodoist() {

  displayTodoist();
  if (connectWiFi())
  {
    if(getTodoistData()== true)
    {
      displayTodoist();
    }
  }
  

  display.display(true);
  guiState = TODOIST_STATE;
}

void Watchy::displayTodoist() {

  int16_t x = 5;
  int16_t y = 30;
  int16_t x1, y1;
  uint16_t w, h;
  bool taskFound = false;

  /*display.setFullWindow();
  display.fillScreen(GxEPD_BLACK);
  display.drawBitmap(0, 0, sully, 200, 200,GxEPD_WHITE);
  display.display(true);*/

  display.setFullWindow();
  display.fillScreen(DARKMODE ? GxEPD_BLACK : GxEPD_WHITE);
  display.setFont(&DSEG7_Classic_Bold_25);
  display.setTextColor(DARKMODE ? GxEPD_WHITE : GxEPD_BLACK);
  //Centre calendar word
  display.getTextBounds("ToDoist",x,y,&x1,&y1,&w,&h);
  display.setCursor((DISPLAY_WIDTH-w)/2, y);
  display.println("ToDoist");

  display.setFont(&FreeMonoBold8pt7b);
  
  for(int index=0;index<TODOIST_TASK_MAX ; index++)
  {
    if(todoistTask[index].todoistId != 0)
    {
      taskFound = true;
      display.setCursor(0, y+=20); 
      display.println((char*)todoistTask[index].todoistTask);
    }
  }
  if(taskFound == false)
  {
    display.println("No Task Today !!!");
  }
  display.display(true);
}

bool Watchy::getTodoistData() {

  bool ret = false;
  HTTPClient http; // Use Todoist API for live data if WiFi is connected
  http.setConnectTimeout(3000); // 3 second max timeout
  String todoistQueryURL = settings.todoistUrl;

  http.begin(todoistQueryURL.c_str());
  http.addHeader("Authorization",("Bearer "+ settings.todoistToken));
  int httpResponseCode = http.GET();

  if (httpResponseCode == 200) 
  {
    String payload             = http.getString();
    JSONVar responseObject     = JSON.parse(payload);

    ret = true;

    int nbTask = responseObject.length();
    if(nbTask> TODOIST_TASK_MAX)
    {
      nbTask = TODOIST_TASK_MAX;
    }
    //clean task array
    for(int index=0; index<TODOIST_TASK_MAX ; index++)
    {
      memset(todoistTask[index].todoistTask,0x00,22);
      todoistTask[index].todoistId = 0;
    }
    //record task
    for(int index=0;index<nbTask ; index++)
    {
      String task = responseObject[index]["content"];
      task = task.substring(0,22);
      task.toCharArray(todoistTask[index].todoistTask,22);
      String taskId = responseObject[index]["id"];
      todoistTask[index].todoistId = taskId.toInt();
    }
  } 
  http.end();
  return(ret); 
}

void Watchy::showCalendar(tmElements_t calendarTime) {
  
  int16_t y=20;
  int16_t x=10;
  int16_t x1, y1;
  uint16_t w, h;
  String monthYear ="";
  tmElements_t timeNow;
  bool thisMonth = false;

  display.setFullWindow();
  display.fillScreen(DARKMODE ? GxEPD_BLACK : GxEPD_WHITE);
  display.setFont(&FreeMonoBold8pt7b);
  display.setTextColor(DARKMODE ? GxEPD_WHITE : GxEPD_BLACK);
  //Centre calendar word
  display.getTextBounds("Calendar",x,y,&x1,&y1,&w,&h);
  display.setCursor((DISPLAY_WIDTH-w)/2, y);
  display.println("Calendar");

  //Display Month an year of calendar
  monthYear = monthStr(calendarTime.Month) + String(" ") + String(tmYearToCalendar(calendarTime.Year));
  display.getTextBounds(monthYear,x,y,&x1,&y1,&w,&h);
  display.setCursor((DISPLAY_WIDTH-w)/2, y+=20);
  display.println(monthYear);
  
  // display day of this week 
  display.getTextBounds("Mo Tu We Th Fr Sa Su",x,y,&x1,&y1,&w,&h);
  display.setCursor((DISPLAY_WIDTH-w)/2, y+=20);
  display.println("Mo Tu We Th Fr Sa Su");

  //get day start of the first week
  int startDay = startDayOfWeek(tmYearToCalendar(calendarTime.Year), calendarTime.Month,1);

  // get number of days in month
  int monthLength = 0;
  
  if (calendarTime.Month == 1 || calendarTime.Month == 3 || calendarTime.Month == 5 || calendarTime.Month == 7 || calendarTime.Month == 8 || calendarTime.Month == 10 || calendarTime.Month == 12){
    monthLength = 31;
  } else if(calendarTime.Month== 2){
    if (tmYearToCalendar(calendarTime.Year)==(tmYearToCalendar(calendarTime.Year)/4)*4){
      monthLength = 29;
    }else{
      monthLength = 28;
    }
  } else {
    monthLength = 30;
  }

  int lineIndex = startDay * 3;
  String week="";

  for(int i=0 ; i<lineIndex ; i+=3)
  {
    week += "   ";
  }

  int day = 1;

  while( day <= monthLength )
  {
    while((lineIndex < 21) && (day <= monthLength))
    {
      week += dayToString(day);
      lineIndex += 3;
      day++;
    }
    lineIndex = 0;
    display.setCursor(x,y+=20);
    display.println(week);
    week="";
  }

  //if the displayed month is this month, display a square around the day
  RTC.read(timeNow);

  if((timeNow.Year == calendarTime.Year)&&(timeNow.Month == calendarTime.Month))
  {
    int line = (timeNow.Day + startDay - 1) / 7;
    int pos =  (timeNow.Day + startDay - 1) % 7;

    x= 10 + ((9*3)*pos);
    y = 65 + (20 * line);
    display.drawRoundRect(x,y,20,20,2,DARKMODE ? GxEPD_WHITE : GxEPD_BLACK);
    display.drawRect(0,0,200,200,DARKMODE ? GxEPD_WHITE : GxEPD_BLACK);
  }

  display.display(true);
  guiState = CALENDAR_STATE;
}

void Watchy::showCalendarOLD(tmElements_t calendarTime) {

  int monthLength = 0;
  int startDay = 0; // Sunday's value is 0, Saturday is 6
  String week1 ="";
  String week2 ="";
  String week3 ="";
  String week4 ="";
  String week5 ="";
  String week6 ="";
  String monthYear ="";
  int newWeekStart = 0; // used to show start of next week of the month
  char monthString2[37]= {"JanFebMarAprMayJunJulAugSepOctNovDec"};
  int  monthIndex2[122] ={0,3,6,9,12,15,18,21,24,27,30,33};
  char monthName2[3]="";
  int16_t x = 5;
  int16_t y = 10;
  int16_t x1, y1;
  uint16_t w, h;

  display.setFullWindow();
  display.fillScreen(DARKMODE ? GxEPD_BLACK : GxEPD_WHITE);
  display.setFont(&FreeMonoBold8pt7b);
  display.setTextColor(DARKMODE ? GxEPD_WHITE : GxEPD_BLACK);
  //Centre calendar word
  display.getTextBounds("Calendar",x,y,&x1,&y1,&w,&h);
  display.setCursor((DISPLAY_WIDTH-w)/2, y);
  display.println("Calendar");

  //Display Month an year of calendar
  y+=20;
  monthYear = monthStr(calendarTime.Month) + String(" ") + String(tmYearToCalendar(calendarTime.Year));
  display.getTextBounds(monthYear,x,y,&x1,&y1,&w,&h);
  display.setCursor((DISPLAY_WIDTH-w)/2, y);
  display.println(monthYear);
  
  // display a full month on a calendar 
  y+=20;
  //Centre calendar word
  display.getTextBounds("Mo Tu We Th Fr Sa Su",x,y,&x1,&y1,&w,&h);
  display.setCursor((DISPLAY_WIDTH-w)/2, y);
  display.println("Mo Tu We Th Fr Sa Su");

  //
  // get number of days in month
  if (calendarTime.Month == 1 || calendarTime.Month == 3 || calendarTime.Month == 5 || calendarTime.Month == 7 || calendarTime.Month == 8 || calendarTime.Month == 10 || calendarTime.Month == 12){
    monthLength = 31;
  } else if(calendarTime.Month== 2){
    monthLength = 28;
  } else {
    monthLength = 30;
  }
  startDay = startDayOfWeek(tmYearToCalendar(calendarTime.Year), calendarTime.Month,1); // Sunday's value is 0
  // now build first week string
  switch (startDay){
    case 0:
      // Monday
      week1 = " 1  2  3  4  5  6  7";
      break;
    case 1:
      // Tuesday
      week1 = "    1  2  3  4  5  6";
      break;      
     case 2:
      // Wednesday
      week1 = "       1  2  3  4  5";
      break;           
     case 3:
      // Thursday
      week1 = "          1  2  3  4";
      break;  
     case 4:
      // Friday
      week1 = "             1  2  3";
      break; 
     case 5:
      // Saturday
      week1 = "                1  2";     
      if(monthLength == 31){
        week6 = "31                  ";
      }      
      break; 
     case 6:
      // Sunday
      week1 = "                   1";
      if(monthLength == 30){
        week6 = "30                  ";
      } else if(monthLength == 31){
        week6 = "30 31               ";
      }       
      
      break;           
  } // end first week
  newWeekStart = (7-startDay)+1;
  const char* newWeek1 = (const char*) week1.c_str();  
  y+=20;
  display.setCursor(x, y);
  display.println(newWeek1);
  // display week 2
  week2 ="";
  for (int f = newWeekStart; f < newWeekStart + 7; f++){
    if(f<10){
      week2 = week2 +  " " + String(f) + " ";
    }  
    else{week2 = week2 + String(f) + " ";}    
  }
  const char* newWeek2 = (const char*) week2.c_str();
  y+=20;
  display.setCursor(x, y);
  display.println(newWeek2);  
  // display week 3
  newWeekStart = (14-startDay)+1; 
  week3 ="";
  for (int f = newWeekStart; f < newWeekStart + 7; f++){
    if(f<10){
      week3 = week3 +  " " + String(f) + " ";
    }  
    else{week3 = week3 + String(f) + " ";}    
  }
  const char* newWeek3 = (const char*) week3.c_str(); 
  y+=20;
  display.setCursor(x, y);
  display.println(newWeek3);    
  // display week 4
  newWeekStart = (21-startDay)+1; 
  week4 ="";
  for (int f = newWeekStart; f < newWeekStart + 7; f++){
    if(f<10){
      week4 = week4 +  " " + String(f) + " ";
    }  
    else{week4 = week4 + String(f) + " ";}    
  }
  const char* newWeek4 = (const char*) week4.c_str();  
  y+=20;
  display.setCursor(x, y);
  display.println(newWeek4);

  // do we need a fifth week
  week5="";
  newWeekStart = (28-startDay)+1;   
  // is is February?
  if(newWeekStart > 28 && calendarTime.Month == 2){
  // do nothing unless its a leap year
    if (tmYearToCalendar(calendarTime.Year)==(tmYearToCalendar(calendarTime.Year)/4)*4){ // its a leap year
      week5 = "29";
    }       
  }
  else{ // print up to 30 anyway
    if(calendarTime.Month == 2){  // its February
      for (int f = newWeekStart; f < 29; f++){
        week5 = week5 + String(f) + " ";  
      }  
      // is it a leap year
      if (tmYearToCalendar(calendarTime.Year)==(tmYearToCalendar(calendarTime.Year)/4)*4){ // its a leap year
        week5 = week5 + "29";
      }        
    }
    else{
      for (int f = newWeekStart; f < 31; f++){
        if (week5.length() < 19){
          week5 = week5 + String(f) + " ";
        }
      }
      // are there 31 days
      if (monthLength == 31 && week5.length() < 19){
        week5 = week5 + "31"; 
      } 
    } 
  }
  const char* newWeek5 = (const char*) week5.c_str();  
  y+=20;
  display.setCursor(x, y);
  display.println(newWeek5);
  if(week6 != null){
    const char* newWeek6 = (const char*) week6.c_str();
    y+=20;
    display.setCursor(x, y);
    display.println(newWeek6);
  }

  display.display(true);
  guiState = CALENDAR_STATE;
}

void Watchy::showFastMenu(byte menuIndex) {
  display.setFullWindow();
  display.fillScreen(GxEPD_BLACK);
  display.setFont(&FreeMonoBold9pt7b);

  int16_t x1, y1;
  uint16_t w, h;
  int16_t yPos;

  const char *menuItems[] = {
      "About Watchy", "Vibrate Motor", "Show Accelerometer",
      "Set Time",     "Setup WiFi",    "Update Firmware",
      "Sync NTP"};
  for (int i = 0; i < MENU_LENGTH; i++) {
    yPos = MENU_HEIGHT + (MENU_HEIGHT * i);
    display.setCursor(0, yPos);
    if (i == menuIndex) {
      display.getTextBounds(menuItems[i], 0, yPos, &x1, &y1, &w, &h);
      display.fillRect(x1 - 1, y1 - 10, 200, h + 15, GxEPD_WHITE);
      display.setTextColor(GxEPD_BLACK);
      display.println(menuItems[i]);
    } else {
      display.setTextColor(GxEPD_WHITE);
      display.println(menuItems[i]);
    }
  }

  display.display(true);

  guiState = MAIN_MENU_STATE;
}

void Watchy::showAbout() {
  display.setFullWindow();
  display.fillScreen(GxEPD_BLACK);
  display.setFont(&FreeMonoBold9pt7b);
  display.setTextColor(GxEPD_WHITE);
  display.setCursor(0, 20);

  display.print("LibVer: ");
  display.println(WATCHY_LIB_VER);

  const char *RTC_HW[3] = {"<UNKNOWN>", "DS3231", "PCF8563"};
  display.print("RTC: ");
  display.println(RTC_HW[RTC.rtcType]); // 0 = UNKNOWN, 1 = DS3231, 2 = PCF8563

  display.print("Batt: ");
  float voltage = getBatteryVoltage();
  display.print(voltage);
  display.println("V");

  display.print("Uptime: ");
  RTC.read(currentTime);
  time_t b = makeTime(bootTime);
  time_t c = makeTime(currentTime);
  int totalSeconds = c-b;
  //int seconds = (totalSeconds % 60);
  int minutes = (totalSeconds % 3600) / 60;
  int hours = (totalSeconds % 86400) / 3600;
  int days = (totalSeconds % (86400 * 30)) / 86400; 
  display.print(days);
  display.print("d");
  display.print(hours);
  display.print("h");
  display.print(minutes);
  display.println("m");   

  display.print("Free Heap: ");
  display.print(ESP.getFreeHeap());
    

  display.display(true); // full refresh

  guiState = APP_STATE;
}

void Watchy::showBuzz() {
  display.setFullWindow();
  display.fillScreen(GxEPD_BLACK);
  display.setFont(&FreeMonoBold9pt7b);
  display.setTextColor(GxEPD_WHITE);
  display.setCursor(70, 80);
  display.println("Buzz!");
  display.display(true); // full refresh
  vibMotor();
  showMenu(menuIndex, false);
}

void Watchy::vibMotor(uint8_t intervalMs, uint8_t length) {
  pinMode(VIB_MOTOR_PIN, OUTPUT);
  bool motorOn = false;
  for (int i = 0; i < length; i++) {
    motorOn = !motorOn;
    digitalWrite(VIB_MOTOR_PIN, motorOn);
    delay(intervalMs);
  }
}

void Watchy::setTime() {

  guiState = APP_STATE;

  RTC.read(currentTime);

  int8_t minute = currentTime.Minute;
  int8_t hour   = currentTime.Hour;
  int8_t day    = currentTime.Day;
  int8_t month  = currentTime.Month;
  int8_t year   = tmYearToY2k(currentTime.Year);

  int8_t setIndex = SET_HOUR;

  int8_t blink = 0;

  pinMode(DOWN_BTN_PIN, INPUT);
  pinMode(UP_BTN_PIN, INPUT);
  pinMode(MENU_BTN_PIN, INPUT);
  pinMode(BACK_BTN_PIN, INPUT);

  display.setFullWindow();

  while (1) {

    if (digitalRead(MENU_BTN_PIN) == 1) {
      setIndex++;
      if (setIndex > SET_DAY) {
        break;
      }
    }
    if (digitalRead(BACK_BTN_PIN) == 1) {
      if (setIndex != SET_HOUR) {
        setIndex--;
      }
    }

    blink = 1 - blink;

    if (digitalRead(DOWN_BTN_PIN) == 1) {
      blink = 1;
      switch (setIndex) {
      case SET_HOUR:
        hour == 23 ? (hour = 0) : hour++;
        break;
      case SET_MINUTE:
        minute == 59 ? (minute = 0) : minute++;
        break;
      case SET_YEAR:
        year == 99 ? (year = 0) : year++;
        break;
      case SET_MONTH:
        month == 12 ? (month = 1) : month++;
        break;
      case SET_DAY:
        day == 31 ? (day = 1) : day++;
        break;
      default:
        break;
      }
    }

    if (digitalRead(UP_BTN_PIN) == 1) {
      blink = 1;
      switch (setIndex) {
      case SET_HOUR:
        hour == 0 ? (hour = 23) : hour--;
        break;
      case SET_MINUTE:
        minute == 0 ? (minute = 59) : minute--;
        break;
      case SET_YEAR:
        year == 0 ? (year = 99) : year--;
        break;
      case SET_MONTH:
        month == 1 ? (month = 12) : month--;
        break;
      case SET_DAY:
        day == 1 ? (day = 31) : day--;
        break;
      default:
        break;
      }
    }

    display.fillScreen(GxEPD_BLACK);
    display.setTextColor(GxEPD_WHITE);
    display.setFont(&DSEG7_Classic_Bold_53);

    display.setCursor(5, 80);
    if (setIndex == SET_HOUR) { // blink hour digits
      display.setTextColor(blink ? GxEPD_WHITE : GxEPD_BLACK);
    }
    if (hour < 10) {
      display.print("0");
    }
    display.print(hour);

    display.setTextColor(GxEPD_WHITE);
    display.print(":");

    display.setCursor(108, 80);
    if (setIndex == SET_MINUTE) { // blink minute digits
      display.setTextColor(blink ? GxEPD_WHITE : GxEPD_BLACK);
    }
    if (minute < 10) {
      display.print("0");
    }
    display.print(minute);

    display.setTextColor(GxEPD_WHITE);

    display.setFont(&FreeMonoBold9pt7b);
    display.setCursor(45, 150);
    if (setIndex == SET_YEAR) { // blink minute digits
      display.setTextColor(blink ? GxEPD_WHITE : GxEPD_BLACK);
    }
    display.print(2000 + year);

    display.setTextColor(GxEPD_WHITE);
    display.print("/");

    if (setIndex == SET_MONTH) { // blink minute digits
      display.setTextColor(blink ? GxEPD_WHITE : GxEPD_BLACK);
    }
    if (month < 10) {
      display.print("0");
    }
    display.print(month);

    display.setTextColor(GxEPD_WHITE);
    display.print("/");

    if (setIndex == SET_DAY) { // blink minute digits
      display.setTextColor(blink ? GxEPD_WHITE : GxEPD_BLACK);
    }
    if (day < 10) {
      display.print("0");
    }
    display.print(day);
    display.display(true); // partial refresh
  }

  tmElements_t tm;
  tm.Month  = month;
  tm.Day    = day;
  tm.Year   = y2kYearToTm(year);
  tm.Hour   = hour;
  tm.Minute = minute;
  tm.Second = 0;

  RTC.set(tm);

  showMenu(menuIndex, false);
}

void Watchy::showAccelerometer() {
  display.setFullWindow();
  display.fillScreen(GxEPD_BLACK);
  display.setFont(&FreeMonoBold9pt7b);
  display.setTextColor(GxEPD_WHITE);

  Accel acc;

  long previousMillis = 0;
  long interval       = 200;

  guiState = APP_STATE;

  pinMode(BACK_BTN_PIN, INPUT);

  while (1) {

    unsigned long currentMillis = millis();

    if (digitalRead(BACK_BTN_PIN) == 1) {
      break;
    }

    if (currentMillis - previousMillis > interval) {
      previousMillis = currentMillis;
      // Get acceleration data
      bool res          = sensor.getAccel(acc);
      uint8_t direction = sensor.getDirection();
      display.fillScreen(GxEPD_BLACK);
      display.setCursor(0, 30);
      if (res == false) {
        display.println("getAccel FAIL");
      } else {
        display.print("  X:");
        display.println(acc.x);
        display.print("  Y:");
        display.println(acc.y);
        display.print("  Z:");
        display.println(acc.z);

        display.setCursor(30, 130);
        switch (direction) {
        case DIRECTION_DISP_DOWN:
          display.println("FACE DOWN");
          break;
        case DIRECTION_DISP_UP:
          display.println("FACE UP");
          break;
        case DIRECTION_BOTTOM_EDGE:
          display.println("BOTTOM EDGE");
          break;
        case DIRECTION_TOP_EDGE:
          display.println("TOP EDGE");
          break;
        case DIRECTION_RIGHT_EDGE:
          display.println("RIGHT EDGE");
          break;
        case DIRECTION_LEFT_EDGE:
          display.println("LEFT EDGE");
          break;
        default:
          display.println("ERROR!!!");
          break;
        }
      }
      display.display(true); // full refresh
    }
  }

  showMenu(menuIndex, false);
}

void Watchy::showWatchFace(bool partialRefresh) {
  display.setFullWindow();
  drawWatchFace();
  display.display(partialRefresh); // partial refresh
  guiState = WATCHFACE_STATE;
}

void Watchy::drawWatchFace() {
  display.setFont(&DSEG7_Classic_Bold_53);
  display.setCursor(5, 53 + 60);
  if (currentTime.Hour < 10) {
    display.print("0");
  }
  display.print(currentTime.Hour);
  display.print(":");
  if (currentTime.Minute < 10) {
    display.print("0");
  }
  display.println(currentTime.Minute);
}

weatherData Watchy::getWeatherData() {
  return getWeatherData(settings.cityID, settings.weatherUnit,
                        settings.weatherLang, settings.weatherURL,
                        settings.weatherAPIKey, settings.weatherUpdateInterval,
                        settings.useInternalTemperature);
}

weatherData Watchy::getWeatherData(String cityID, String units, String lang,
                                   String url, String apiKey,
                                   uint8_t updateInterval,
                                  bool useInternalTemperature) {
  currentWeather.isMetric = units == String("metric");
  if (weatherIntervalCounter < 0) { //-1 on first run, set to updateInterval
    weatherIntervalCounter = updateInterval;
  }
  if (weatherIntervalCounter >=
      updateInterval) { // only update if WEATHER_UPDATE_INTERVAL has elapsed
                        // i.e. 30 minutes
    if (connectWiFi()) {
      HTTPClient http; // Use Weather API for live data if WiFi is connected
      http.setConnectTimeout(3000); // 3 second max timeout
      String weatherQueryURL = url + cityID + String("&units=") + units +
                               String("&lang=") + lang + String("&appid=") +
                               apiKey;
      http.begin(weatherQueryURL.c_str());
      int httpResponseCode = http.GET();
      if (httpResponseCode == 200) {
        String payload             = http.getString();
        JSONVar responseObject     = JSON.parse(payload);
        currentWeather.temperature = int(responseObject["main"]["temp"]);
        currentWeather.weatherConditionCode =
            int(responseObject["weather"][0]["id"]);
        currentWeather.weatherDescription =
	  JSONVar::stringify(responseObject["weather"][0]["main"]);
	    currentWeather.external = true;
        // sync NTP during weather API call and use timezone of city
        gmtOffset = int(responseObject["timezone"]);
        syncNTP(gmtOffset);
        //get todoist data
        getTodoistData();
      } else {
        // http error
      }
      http.end();
      // turn off radios
      WiFi.mode(WIFI_OFF);
      btStop();
    } else if(useInternalTemperature){ // No WiFi, use internal temperature sensor
      uint8_t temperature = sensor.readTemperature(); // celsius
      if (!currentWeather.isMetric) {
        temperature = temperature * 9. / 5. + 32.; // fahrenheit
      }
      currentWeather.temperature          = temperature;
      currentWeather.weatherConditionCode = 800;
      currentWeather.external             = false;
    }
    weatherIntervalCounter = 0;
  } else {
    weatherIntervalCounter++;
  }
  return currentWeather;
}

float Watchy::getBatteryVoltage() {
  if (RTC.rtcType == DS3231) {
    return analogReadMilliVolts(BATT_ADC_PIN) / 1000.0f *
           2.0f; // Battery voltage goes through a 1/2 divider.
  } else {
    return analogReadMilliVolts(BATT_ADC_PIN) / 1000.0f * 2.0f;
  }
}

uint16_t Watchy::_readRegister(uint8_t address, uint8_t reg, uint8_t *data,
                               uint16_t len) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  Wire.endTransmission();
  Wire.requestFrom((uint8_t)address, (uint8_t)len);
  uint8_t i = 0;
  while (Wire.available()) {
    data[i++] = Wire.read();
  }
  return 0;
}

uint16_t Watchy::_writeRegister(uint8_t address, uint8_t reg, uint8_t *data,
                                uint16_t len) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  Wire.write(data, len);
  return (0 != Wire.endTransmission());
}

void Watchy::_bmaConfig() {

  if (sensor.begin(_readRegister, _writeRegister, delay) == false) {
    // fail to init BMA
    return;
  }

  // Accel parameter structure
  Acfg cfg;
  /*!
      Output data rate in Hz, Optional parameters:
          - BMA4_OUTPUT_DATA_RATE_0_78HZ
          - BMA4_OUTPUT_DATA_RATE_1_56HZ
          - BMA4_OUTPUT_DATA_RATE_3_12HZ
          - BMA4_OUTPUT_DATA_RATE_6_25HZ
          - BMA4_OUTPUT_DATA_RATE_12_5HZ
          - BMA4_OUTPUT_DATA_RATE_25HZ
          - BMA4_OUTPUT_DATA_RATE_50HZ
          - BMA4_OUTPUT_DATA_RATE_100HZ
          - BMA4_OUTPUT_DATA_RATE_200HZ
          - BMA4_OUTPUT_DATA_RATE_400HZ
          - BMA4_OUTPUT_DATA_RATE_800HZ
          - BMA4_OUTPUT_DATA_RATE_1600HZ
  */
  cfg.odr = BMA4_OUTPUT_DATA_RATE_100HZ;
  /*!
      G-range, Optional parameters:
          - BMA4_ACCEL_RANGE_2G
          - BMA4_ACCEL_RANGE_4G
          - BMA4_ACCEL_RANGE_8G
          - BMA4_ACCEL_RANGE_16G
  */
  cfg.range = BMA4_ACCEL_RANGE_2G;
  /*!
      Bandwidth parameter, determines filter configuration, Optional parameters:
          - BMA4_ACCEL_OSR4_AVG1
          - BMA4_ACCEL_OSR2_AVG2
          - BMA4_ACCEL_NORMAL_AVG4
          - BMA4_ACCEL_CIC_AVG8
          - BMA4_ACCEL_RES_AVG16
          - BMA4_ACCEL_RES_AVG32
          - BMA4_ACCEL_RES_AVG64
          - BMA4_ACCEL_RES_AVG128
  */
  cfg.bandwidth = BMA4_ACCEL_NORMAL_AVG4;

  /*! Filter performance mode , Optional parameters:
      - BMA4_CIC_AVG_MODE
      - BMA4_CONTINUOUS_MODE
  */
  cfg.perf_mode = BMA4_CONTINUOUS_MODE;

  // Configure the BMA423 accelerometer
  sensor.setAccelConfig(cfg);

  // Enable BMA423 accelerometer
  // Warning : Need to use feature, you must first enable the accelerometer
  // Warning : Need to use feature, you must first enable the accelerometer
  sensor.enableAccel();

  struct bma4_int_pin_config config;
  config.edge_ctrl = BMA4_LEVEL_TRIGGER;
  config.lvl       = BMA4_ACTIVE_HIGH;
  config.od        = BMA4_PUSH_PULL;
  config.output_en = BMA4_OUTPUT_ENABLE;
  config.input_en  = BMA4_INPUT_DISABLE;
  // The correct trigger interrupt needs to be configured as needed
  sensor.setINTPinConfig(config, BMA4_INTR1_MAP);

  struct bma423_axes_remap remap_data;
  remap_data.x_axis      = 1;
  remap_data.x_axis_sign = 0xFF;
  remap_data.y_axis      = 0;
  remap_data.y_axis_sign = 0xFF;
  remap_data.z_axis      = 2;
  remap_data.z_axis_sign = 0xFF;
  // Need to raise the wrist function, need to set the correct axis
  sensor.setRemapAxes(&remap_data);

  // Enable BMA423 isStepCounter feature
  sensor.enableFeature(BMA423_STEP_CNTR, true);
  // Enable BMA423 isTilt feature
  sensor.enableFeature(BMA423_TILT, true);
  // Enable BMA423 isDoubleClick feature
  sensor.enableFeature(BMA423_WAKEUP, true);

  // Reset steps
  sensor.resetStepCounter();

  // Turn on feature interrupt
  sensor.enableStepCountInterrupt();
  sensor.enableTiltInterrupt();
  // It corresponds to isDoubleClick interrupt
  sensor.enableWakeupInterrupt();
}

void Watchy::setupWifi() {
  display.epd2.setBusyCallback(0); // temporarily disable lightsleep on busy
  WiFiManager wifiManager;
  wifiManager.resetSettings();
  wifiManager.setTimeout(WIFI_AP_TIMEOUT);
  wifiManager.setAPCallback(_configModeCallback);
  display.setFullWindow();
  display.fillScreen(GxEPD_BLACK);
  display.setFont(&FreeMonoBold9pt7b);
  display.setTextColor(GxEPD_WHITE);
  if (!wifiManager.autoConnect(WIFI_AP_SSID)) { // WiFi setup failed
    display.println("Setup failed &");
    display.println("timed out!");
  } else {
    display.println("Connected to");
    display.println(WiFi.SSID());
		display.println("Local IP:");
		display.println(WiFi.localIP());
    weatherIntervalCounter = -1; // Reset to force weather to be read again
  }
  display.display(true); // full refresh
  // turn off radios
  WiFi.mode(WIFI_OFF);
  btStop();
  display.epd2.setBusyCallback(displayBusyCallback); // enable lightsleep on
                                                     // busy
  guiState = APP_STATE;
}

void Watchy::_configModeCallback(WiFiManager *myWiFiManager) {
  display.setFullWindow();
  display.fillScreen(GxEPD_BLACK);
  display.setFont(&FreeMonoBold9pt7b);
  display.setTextColor(GxEPD_WHITE);
  display.setCursor(0, 30);
  display.println("Connect to");
  display.print("SSID: ");
  display.println(WIFI_AP_SSID);
  display.print("IP: ");
  display.println(WiFi.softAPIP());
	display.println("MAC address:");
	display.println(WiFi.softAPmacAddress().c_str());
  display.display(true); // full refresh
}

bool Watchy::connectWiFi() {

  String localSsid="";
  String localPwd="OPEN";
  int staFound = false;
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  int n = WiFi.scanNetworks();
  Serial.println("scan done");
  if (n == 0) 
  {
      Serial.println("no networks found");
  } 
  else 
  {
    Serial.print(n);
    Serial.println(" networks found");
    for (int i = 0; i < n; ++i) 
    {
      // Print SSID and RSSI for each network found
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i).c_str());
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));
      Serial.print(")");
      Serial.println((WiFi.encryptionType(i) == WIFI_AUTH_OPEN)?" ":"*");
      if(WiFi.encryptionType(i) == WIFI_AUTH_OPEN)
      {
        localSsid = WiFi.SSID(i);
        staFound = true;
        Serial.println("networks free open : "+ WiFi.SSID(i));
      }
      else
      {
        for (int j=0; j < WIFI_STA_NB; j++)
        {
          Serial.println("Compar "+ WiFi.SSID(i) + " & "+ settings.ssid[j].c_str());
          //if(settings.ssid[j].c_str()== WiFi.SSID(i).c_str())
          if(WiFi.SSID(i).compareTo(settings.ssid[j]) == 0)
          {
            localSsid = WiFi.SSID(i);
            localPwd = settings.pwd[j];
            staFound = true;
            Serial.println("networks Match found : "+ WiFi.SSID(i));
          }
        }
      }
      if(staFound == true)
      {
        int ret;
        staFound = false;
        if(WiFi.encryptionType(i) == WIFI_AUTH_OPEN)
        {
          ret = WiFi.begin(localSsid.c_str());
        }
        else
        {
          ret = WiFi.begin(localSsid.c_str(),localPwd.c_str());
        }
        

        if (ret == WL_CONNECT_FAILED) 
        { 
          WIFI_CONFIGURED = false;
          Serial.println("WL_CONNECT_FAILED");
        } 
        else 
        {
          if (WL_CONNECTED ==
              WiFi.waitForConnectResult()) { // attempt to connect for 10s
            WIFI_CONFIGURED = true;
            //i=n for ext from the for
            i = n;
          } else { // connection failed, time out
            WIFI_CONFIGURED = false;
            // turn off radios
            WiFi.mode(WIFI_OFF);
            btStop();
            Serial.println("WL_CONNECT_TIMEOUT");
            Serial.println(ret);
            
            return WIFI_CONFIGURED;
          }
        }
      }
      else
      {
        WIFI_CONFIGURED = false;
      }
      delay(10);
    }
  }
  
  return WIFI_CONFIGURED;
}

void Watchy::showScanWiFi() {

  int16_t x = 5;
  int16_t y = 10;
  int16_t x1, y1;
  uint16_t w, h;

  display.setFullWindow();
  display.fillScreen(DARKMODE ? GxEPD_BLACK : GxEPD_WHITE);
  display.setFont(&FreeMonoBold8pt7b);
  display.setTextColor(DARKMODE ? GxEPD_WHITE : GxEPD_BLACK);
  //Centre calendar word
  display.getTextBounds("SCAN WIFI",x,y,&x1,&y1,&w,&h);
  display.setCursor((DISPLAY_WIDTH-w)/2, y);
  display.println("SCAN WIFI");
  
  display.display(true);

  String localSsid="";
  String localPwd="OPEN";
  int staFound = false;
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  int n = WiFi.scanNetworks();
  display.setCursor(x, y+=20);
  display.println("scan done");
  if (n == 0) 
  {
    display.setCursor(x, y+=20);
    display.println("no networks found");
  } 
  else 
  {
    display.setCursor(x, y+=20);
    display.print(n);
    display.println(" networks found");
    for (int i = 0; i < n; ++i) 
    {
      // Print SSID and RSSI for each network found
      display.setCursor(x, y+=20);
      display.print(i + 1);
      display.print(":");
      display.print(WiFi.SSID(i).substring(0,19));
      //display.print(" (");
      //display.print(WiFi.RSSI(i));
      //display.print(")");
      display.println((WiFi.encryptionType(i) == WIFI_AUTH_OPEN)?"#":"*");
     
    }
  }
  display.display(true);
  WiFi.mode(WIFI_OFF);
  btStop();
}

void Watchy::showUpdateFW() {
  display.setFullWindow();
  display.fillScreen(GxEPD_BLACK);
  display.setFont(&FreeMonoBold9pt7b);
  display.setTextColor(GxEPD_WHITE);
  display.setCursor(0, 30);
  display.println("Please visit");
  display.println("watchy.sqfmi.com");
  display.println("with a Bluetooth");
  display.println("enabled device");
  display.println(" ");
  display.println("Press menu button");
  display.println("again when ready");
  display.println(" ");
  display.println("Keep USB powered");
  display.display(true); // full refresh

  guiState = FW_UPDATE_STATE;
}

void Watchy::updateFWBegin() {
  display.setFullWindow();
  display.fillScreen(GxEPD_BLACK);
  display.setFont(&FreeMonoBold9pt7b);
  display.setTextColor(GxEPD_WHITE);
  display.setCursor(0, 30);
  display.println("Bluetooth Started");
  display.println(" ");
  display.println("Watchy BLE OTA");
  display.println(" ");
  display.println("Waiting for");
  display.println("connection...");
  display.display(true); // full refresh

  BLE BT;
  BT.begin("Watchy BLE OTA");
  int prevStatus = -1;
  int currentStatus;

  while (1) {
    currentStatus = BT.updateStatus();
    if (prevStatus != currentStatus || prevStatus == 1) {
      if (currentStatus == 0) {
        display.setFullWindow();
        display.fillScreen(GxEPD_BLACK);
        display.setFont(&FreeMonoBold9pt7b);
        display.setTextColor(GxEPD_WHITE);
        display.setCursor(0, 30);
        display.println("BLE Connected!");
        display.println(" ");
        display.println("Waiting for");
        display.println("upload...");
        display.display(true); // full refresh
      }
      if (currentStatus == 1) {
        display.setFullWindow();
        display.fillScreen(GxEPD_BLACK);
        display.setFont(&FreeMonoBold9pt7b);
        display.setTextColor(GxEPD_WHITE);
        display.setCursor(0, 30);
        display.println("Downloading");
        display.println("firmware:");
        display.println(" ");
        display.print(BT.howManyBytes());
        display.println(" bytes");
        display.display(true); // partial refresh
      }
      if (currentStatus == 2) {
        display.setFullWindow();
        display.fillScreen(GxEPD_BLACK);
        display.setFont(&FreeMonoBold9pt7b);
        display.setTextColor(GxEPD_WHITE);
        display.setCursor(0, 30);
        display.println("Download");
        display.println("completed!");
        display.println(" ");
        display.println("Rebooting...");
        display.display(true); // full refresh

        delay(2000);
        esp_restart();
      }
      if (currentStatus == 4) {
        display.setFullWindow();
        display.fillScreen(GxEPD_BLACK);
        display.setFont(&FreeMonoBold9pt7b);
        display.setTextColor(GxEPD_WHITE);
        display.setCursor(0, 30);
        display.println("BLE Disconnected!");
        display.println(" ");
        display.println("exiting...");
        display.display(true); // full refresh
        delay(1000);
        break;
      }
      prevStatus = currentStatus;
    }
    delay(100);
  }

  // turn off radios
  WiFi.mode(WIFI_OFF);
  btStop();
  showMenu(menuIndex, false);
}

void Watchy::showSyncNTP() {
  display.setFullWindow();
  display.fillScreen(GxEPD_BLACK);
  display.setFont(&FreeMonoBold9pt7b);
  display.setTextColor(GxEPD_WHITE);
  display.setCursor(0, 30);
  display.println("Syncing NTP... ");
  display.print("GMT offset: ");
  display.println(gmtOffset);
  display.display(true); // full refresh
  if (connectWiFi()) {
    if (syncNTP()) {
      display.println("NTP Sync Success\n");
      display.println("Current Time Is:");

      RTC.read(currentTime);

      display.print(tmYearToCalendar(currentTime.Year));
      display.print("/");
      display.print(currentTime.Month);
      display.print("/");
      display.print(currentTime.Day);
      display.print(" - ");

      if (currentTime.Hour < 10) {
        display.print("0");
      }
      display.print(currentTime.Hour);
      display.print(":");
      if (currentTime.Minute < 10) {
        display.print("0");
      }
      display.println(currentTime.Minute);
    } else {
      display.println("NTP Sync Failed");
    }
  } else {
    display.println("WiFi Not Configured");
  }
  display.display(true); // full refresh
  delay(3000);
  showMenu(menuIndex, false);
}

bool Watchy::syncNTP() { // NTP sync - call after connecting to WiFi and
                         // remember to turn it back off
  return syncNTP(gmtOffset,
                 settings.ntpServer.c_str());
}

bool Watchy::syncNTP(long gmt) {
  return syncNTP(gmt, settings.ntpServer.c_str());
}

bool Watchy::syncNTP(long gmt, String ntpServer) {
  // NTP sync - call after connecting to
  // WiFi and remember to turn it back off
  WiFiUDP ntpUDP;
  NTPClient timeClient(ntpUDP, ntpServer.c_str(), gmt);
  timeClient.begin();
  if (!timeClient.forceUpdate()) {
    return false; // NTP sync failed
  }
  tmElements_t tm;
  breakTime((time_t)timeClient.getEpochTime(), tm);
  RTC.set(tm);
  return true;
}

int Watchy::startDayOfWeek(int y, int m, int d){
  //static int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    static int t[] = {6, 2, 1, 4, 6, 2, 4, 6, 3, 5, 1, 3};
  y -= m < 3;
  return (y +y/4 -y/100 + y/400 + t[m-1] + d)% 7; 
} 

String Watchy::dayToString(int _day)
{
  String day = "";
  if(_day<10)
  {
    day+=" ";
  }
  day+= String(_day); 
  day+=" ";
  return(day);
}

u_int16_t Watchy::getTextColor()
{
  return (DARKMODE ? GxEPD_WHITE : GxEPD_BLACK);
}

u_int16_t Watchy::getBackColor()
{
  return (DARKMODE ? GxEPD_BLACK : GxEPD_WHITE);
}

