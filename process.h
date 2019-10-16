/*
process.h

This file holds the prototypes of the functions in process.c. They deal with the creation, suspension, resumption, termination and general management of processes for the OS.

*/


#ifndef PROCESS_H
#define PROCESS_H

#include "osGlobals.h"


void ProcessesState();
void InitializeProcessInfo();
void osSuspendProcess(long PID, long *ReturnError);
void osResumeProcess(long PID, long *ReturnError);
void osTerminateProcess(long TargetPID, long *ReturnError);
void GetProcessID(char ProcessName[], long *PID, long *ReturnError);
long GetCurrentPID();
long osGetCurrentContext();
void* GetCurrentPCB();
int CheckProcessCount();
void osCreateProcess(char Name[], long StartAddress, long Priority, long *PID, long *ReturnError);
INT32 CheckActiveProcess();
void TerminateProcess(long PID);
void TerminateChildren(long PID);
int CheckProcessName(char Name[]);
void* GetPCBContext(long Context);
void ChangeProcessState(long PID, INT32 NewState);
void GetProcessState(long PID, INT32 *State);
void osChangePriority(long PID, long NewPriority, long *ReturnError);
void* GetPCB(long PID);

#endif //PROCESS_H
