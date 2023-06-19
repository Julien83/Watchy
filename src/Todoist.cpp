// Todoist Library for ESP32.
//

//
// Author: Julien Junique
//
// Version: see library.properties
//
// Library: https://github.com/julien83/Todoist
// Website of Todoist: https://todoist.com/
//Documention of Todoist API REST: https://developer.todoist.com/rest/v2/#overview 


#include "Todoist.h"

String apiKey = "";


//init todoist api
Todoist::Todoist(String _apiKey) 
{
   apiKey = _apiKey;
}

// get list of Project
/*uint8_t Todoist::getProjectList(todoistProjectList _projetlist[]) 
{
  uint8_t ret = Success;

  HTTPClient http; // Use Todoist API for live data if WiFi is connected
  http.setConnectTimeout(2000); // 2 second max timeout
  
  String todoistQueryURL = todoistUrl + todoistProjectDirectory;

  http.begin(todoistQueryURL.c_str());
  http.addHeader("Authorization",("Bearer "+ _apiKey));
  int httpResponseCode = http.GET();

  if (httpResponseCode == 200) 
  {
    String payload             = http.getString();
    JSONVar responseObject     = JSON.parse(payload);

    int nbProject = responseObject.length();
    if(nbProject> PROJECT_LIST_LEN)
    {
      ret = ErrListToSmall;
      nbProject = PROJECT_LIST_LEN;
    }
    
    //record project
    for(int index=0;index<nbProject ; index++)
    {
      //todoistProjectList projectList[nbProject];
      //projectList[index].name = responseObject[index]["content"];
      //projectList[index].Id= responseObject[index]["id"];
      _projetlist[index].name = responseObject[index]["content"];
      _projetlist[index].Id= responseObject[index]["id"];
    }
  } 
  else
  {
    ret = ErrNoConnection;
  }
  http.end();
  return(ret); 
}*/