#ifndef SETTINGS_H
#define SETTINGS_H

//https://api.todoist.com/rest/v2/projects?token=3d95d801351220fb297643a380aeb4c69bf328c2

//Todoist Settings
#define TODOIST_TOKEN           "3d95d801351220fb297643a380aeb4c69bf328c2"
//#define TODOIST_URL             "https://api.todoist.com/rest/v2/tasks/?project_id=2151759266"
#define TODOIST_URL             "https://api.todoist.com/rest/v2/tasks?filter=today"



//Weather Settings
#define CITY_ID                 "2983732" //Rians 83560 https://openweathermap.org/current#cityid
#define OPENWEATHERMAP_APIKEY   "f058fe1cad2afe8e2ddc5d063a64cecb" //use your own API key :)
#define OPENWEATHERMAP_URL      "http://api.openweathermap.org/data/2.5/weather?id=" //open weather api
#define TEMP_UNIT               "metric" //metric = Celsius , imperial = Fahrenheit
#define TEMP_LANG               "en"
#define WEATHER_UPDATE_INTERVAL 30 //must be greater than 5, measured in minutes
#define USE_INTERNAL_TEMP       false
//NTP Settings
#define NTP_SERVER              "pool.ntp.org"
#define GMT_OFFSET_SEC          3600 * +2 //Paris GMT+2 will be overwritten by weather data

#define WIFI_STA_SSID_1         "AP_TEST"
#define WIFI_STA_PWD_1          "1234test"
#define WIFI_STA_SSID_2         "AP2000"
#define WIFI_STA_PWD_2          "1234test"
#define WIFI_STA_SSID_3         "JJ_WIFI"
#define WIFI_STA_PWD_3          "lunadebelair"
#define WIFI_STA_SSID_4         "SFR_E9F8"
#define WIFI_STA_PWD_4          "lunadebelair"




watchySettings settings{
    .cityID = CITY_ID,
    .weatherAPIKey = OPENWEATHERMAP_APIKEY,
    .weatherURL = OPENWEATHERMAP_URL,
    .weatherUnit = TEMP_UNIT,
    .weatherLang = TEMP_LANG,
    .weatherUpdateInterval = WEATHER_UPDATE_INTERVAL,
    .useInternalTemperature = USE_INTERNAL_TEMP,
    .ntpServer = NTP_SERVER,
    .gmtOffset = GMT_OFFSET_SEC,
    .vibrateOClock = true,
    .vibrateHalfOClock = true,
    .ssid = {WIFI_STA_SSID_1,WIFI_STA_SSID_2,WIFI_STA_SSID_3,WIFI_STA_SSID_4},
    .pwd = {WIFI_STA_PWD_1,WIFI_STA_PWD_2,WIFI_STA_PWD_3,WIFI_STA_SSID_4},
    .todoistToken = TODOIST_TOKEN,
    .todoistUrl = TODOIST_URL,
};

#endif
