//Include file for everything to do with the timer queue

#include "global.h"
#include "os_globals.h"

int CreateReadyQueue();

//can't figure out how call works right now
void dispatcher();

long RemoveFromReadyQueue(PROCESS_CONTROL_BLOCK* process);

void AddToReadyQueue(long context, long PID, void* PCB);

void StartTimer(long sleep_time);

void HandleTimerInterrupt();

//really doesnt need an int return
//use functions in QueueManager.c to create a timer for storing 
int CreateTimerQueue();

int CreateSuspendQueue();

//Does this have to have int return????
int CreateDiskQueue(int disk_number);

void InitializeDiskLocks();

void AddToDiskQueue(long disk_id, DQ_ELEMENT* dqe);


void CheckDiskStatus(long disk_id, long* status);
