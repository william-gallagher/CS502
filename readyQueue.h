/*
readyQueue.h

This file is the include file for everything to do with the ready queue.
It also contains the proto type for the dispatcher.

*/

#ifndef READY_QUEUE_H
#define READY_QUEUE_H

#include "global.h"
#include "osGlobals.h"


void dispatcher();
void CreateReadyQueue();
long RemoveFromReadyQueue(PROCESS_CONTROL_BLOCK *pcb);
long ChangePriorityInReadyQueue(PROCESS_CONTROL_BLOCK *pcb,
				INT32 NewPriority);
void AddToReadyQueue(long Context, long PID, void *PCB, INT32 PrintFlag);

#endif //READY_QUEUE_H
