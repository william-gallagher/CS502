#include "os_globals.h"

//Include file for everything to do with processes


//Test function that prints the states of all the processes
void ProcessesState();


//need to remember to free at end somehow
  void CreatePRO_INFO();
  
long osCreateProcess(char* name, long context, INT32 parent);

void osSuspendProcess(long PID, long* return_error);

void osResumeProcess(long PID, long* return_error);

void getProcessID(SYSTEM_CALL_DATA* scd);

long GetCurrentPID();

long osGetCurrentContext();

void* GetCurrentPCB();

int CheckProcessName(char* name);

/*This function checks to see if MAX_PROCESSES has been reached. If the OS has reached its limit 0 is returned. Otherwise 1 is returned. The function steps through the PRO_INFO to see if there is an open slot.
 */
int CheckProcessCount();

//need a getcontext function

void CreateProcess(char* name, long start_address, long priority, INT32 parent, long* process_id, long* return_error);

void TerminateProcess(long process_id);


void TerminateChildren(long process_id);
  
int CheckProcessName(char* name);

void* GetPCBContext(long context);

void ChangeProcessState(long PID, INT32 new_state);

void GetProcessState(long PID, INT32* state);
