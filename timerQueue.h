/*
timerQueue.h

This file is the include file for everything to do with the timer queue.
*/

#ifndef TIMER_QUEUE_H
#define TIMER_QUEUE_H

#include "global.h"
#include "osGlobals.h"


void LockLocation(INT32 lock);
void UnlockLocation(INT32 lock);
void GetTimeOfDay(long *TimeOfDay);
void StartTimer(long SleepTime);
void HandleTimerInterrupt();
void CreateTimerQueue();

#endif //TIMER_QUEUE_H
