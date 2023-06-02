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


#pragma once

#include <HTTPClient.h>

#define PROJECT_LIST_LEN    20
#define PROJECT_NAME_LEN    20

#define TASK_LIST_LEN    	  20
#define TASK_NAME_LEN       20


enum retError{Success = 0,ErrNoConnection,ErrListToSmall};

typedef struct todoistProjectList {
      String name;
      String Id;
      } ;

class Todoist
{
  public:
    // attributes
    const String todoistUrl = "https://api.todoist.com/rest/v2/";
    const String todoistTaskDirectory = "tasks";
    const String todoistProjectDirectory = "projects";
    
    

    // constructor
    Todoist(String apiKey);
	
    //Public Function
    void getTaskByProject(String projectId); // get task on specifique project
	void getTaskByFilter(String Filter); // get task on specifique Filter
	void getProjectList(todoistProjectList _projetlist[]); // get list of Project
	
    
  private:
	//Private Fonction
    void _blabla(uint8_t a, uint8_t b);

};
