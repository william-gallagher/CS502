/*
readyQueue.c

This file holds the functions that deal with the Ready Queue. It also
includes the dispatcher. 
*/

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "protos.h"
#include "process.h"
#include "readyQueue.h"
#include "timerQueue.h" 
#include "osGlobals.h"
#include "osSchedulePrinter.h"

/*
This function creates the Ready Queue using the functions found in the
Queue Manager. 
*/
void CreateReadyQueue(){
  ready_queue_id = QCreate("RQueue");
  if(ready_queue_id == -1){  //Check for error
    aprintf("\n\nError: Unable to create Ready Queue!\n\n");
  }
}

/*
Create a RQ_ELEMENT with fill with Context, PID and a pointer to the 
Process Control Block. Then add the Ready Queue Element to the Ready
Queue. These elements are enqueued by priority.
*/
void AddToReadyQueue(long Context, long PID, void* PCB, INT32 PrintFlag){

  PROCESS_CONTROL_BLOCK* pcb = (PROCESS_CONTROL_BLOCK*) PCB;

  //Allocate and fill Ready Queue Element
  RQ_ELEMENT* rqe = malloc(sizeof(RQ_ELEMENT));
  rqe->context = Context;
  rqe->PID = PID;
  rqe->PCB = pcb;
  pcb->queue_ptr = (void *)rqe;
  
  long Priority = pcb->priority;

  //Insert in Ready Queue. Note that processes are enqueued by priority
  LockLocation(READY_LOCK);
  QInsert(ready_queue_id, Priority, (void *) rqe);
  UnlockLocation(READY_LOCK);
  ChangeProcessState(PID, READY);

  //There are some cases that it does not make sense to use the state printer
  //For example: When resuming a process we will print with "Resume" field
  //rather than "Ready"
  if(PrintFlag == TRUE){
      osPrintState("Ready", PID, GetCurrentPID());
  }
}


/*
Checks to see if the process pointed to by pcb is in the Ready Queue. 
If so the process is removed. A return of -1 indicates that the pcb has
not been found. 
*/
long RemoveFromReadyQueue(PROCESS_CONTROL_BLOCK* pcb){

  long Result;
  
  LockLocation(READY_LOCK);
  //Step through Ready Queue and look for pcb
  if((long)QRemoveItem(ready_queue_id, pcb->queue_ptr) == -1){
    Result = -1;
  }
  else{
    Result = 1;
  }
  UnlockLocation(READY_LOCK);
 
  return Result;
}


/*
This function changes the priority of the process pointed to by pcb.
It should be determined before calling this function that the process
is indeed in the Ready Queue.
First, the process is pulled out of the Ready Queue.
Next, Update its priority.
Then, Reinsert in Ready Queue to reflect the changed priority.
*/
long ChangePriorityInReadyQueue(PROCESS_CONTROL_BLOCK *pcb,
				INT32 NewPriority){
  
  LockLocation(READY_LOCK);

  if((long)QRemoveItem(ready_queue_id, pcb->queue_ptr) == -1){
    aprintf("\n\nError: Process not found in Ready Queue\n\n");
    return -1;
  }
  pcb->priority = NewPriority;
  QInsert(ready_queue_id, NewPriority, pcb->queue_ptr);

  UnlockLocation(READY_LOCK);

  return 1;
}

/*
Remove a RQ_ELEMENT from the head of the Ready Queue and returns a
pointer to it.
*/
RQ_ELEMENT* RemoveFromReadyQueueHead(){

  LockLocation(READY_LOCK);
  RQ_ELEMENT* rqe = (RQ_ELEMENT *)QRemoveHead(ready_queue_id);
  UnlockLocation(READY_LOCK);
  
  return rqe;
}

/*
Check the Ready Queue. Return -1 if there are no elements in the queue.
Otherwise return 1. This is useful for working with the dispatcher() 
function.
*/
long CheckReadyQueue(){

  LockLocation(READY_LOCK);
  long result = (long)QNextItemInfo(ready_queue_id);
  UnlockLocation(READY_LOCK);

  //Nothing on Ready Queue
  if(result == -1){
    return -1;
  }
  else{   //Something on Ready Queue
    return 1;
  }
}

/*
The function is used for the macro CALL to advance the simulation to the
next interrupt.
*/
void WasteTime(){
   //printf("");
}

/*
This function is the heart of the scheduler. It waits while there is 
nothing on the Ready Queue and wastes time until the next interrupt.
When something is added to the Ready Queue by the Interrupt Handler 
the dispatcher grabs it and starts the context.
*/
void dispatcher(){

MEMORY_MAPPED_IO mmio;
 
while(CheckReadyQueue() == -1){
CALL(WasteTime());
//We need a sleep so other thread can get LOCK
//Otherwise the Interrupt Handler can't get in to add a process
//from the Timer or Disk Queues.
//usleep(1);
 for(INT32 i=0; i<10000; i++);
}
long CurrentPID = GetCurrentPID();


RQ_ELEMENT* rqe = RemoveFromReadyQueueHead();
long Context = rqe->context;


osPrintState("Dispatch", rqe->PID, CurrentPID);
  
ChangeProcessState(rqe->PID, RUNNING);

  free(rqe);
  
  mmio.Mode = Z502StartContext;
  mmio.Field1 = Context;
  mmio.Field2 = START_NEW_CONTEXT_AND_SUSPEND;
  mmio.Field3 = mmio.Field4 = 0;
  MEM_WRITE(Z502Context, &mmio);     // Start up the context

  if(mmio.Field4 != ERR_SUCCESS){
    aprintf("\n\nError: in starting context in dispatcher\n\n");
  }
}


