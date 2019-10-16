#ifndef A_H1
#define A_H1

#include "os_globals.h"

//Include file for everything to do with processes


//Test function that prints the states of all the processes
void ProcessesState();
//need to remember to free at end somehow
void InitializeProcessInfo();
//long osCreateProcess(char* name, long context, INT32 parent, long Priority);
void osSuspendProcess(long PID, long* return_error);
void osResumeProcess(long PID, long* return_error);
void osTerminateProcess(long TargetPID, long *ReturnError);
void GetProcessID(char ProcessName[], long *PID, long *ReturnError);
long GetCurrentPID();
long osGetCurrentContext();
void* GetCurrentPCB();
/*This function checks to see if MAX_PROCESSES has been reached. If the OS has reached its limit 0 is returned. Otherwise 1 is returned. The function steps through the PRO_INFO to see if there is an open slot.
 */
int CheckProcessCount();
void osCreateProcess(char* name, long start_address, long priority, long* process_id, long* return_error);
INT32 CheckActiveProcess();
void TerminateProcess(long process_id);
void TerminateChildren(long process_id);
int CheckProcessName(char* name);
void* GetPCBContext(long context);
void ChangeProcessState(long PID, INT32 new_state);
void GetProcessState(long PID, INT32* state);
void osChangePriority(long PID, long NewPriority, long* ReturnError);
void* GetPCB(long PID);

#endif //end my guard
