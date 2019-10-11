//Include file for everything to do with the timer queue

#include "global.h"
#include "os_globals.h"


void LockLocation(INT32 lock);
void UnlockLocation(INT32 lock);
void GetTimeOfDay(long *TimeOfDay);

void StartTimer(long SleepTime);

void HandleTimerInterrupt();

//really doesnt need an int return
//use functions in QueueManager.c to create a timer for storing 
int CreateTimerQueue();
