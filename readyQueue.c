#include "readyQueue.h"
#include "timerQueue.h" //for locks for now
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "protos.h"
#include "process.h"
#include "os_globals.h"
#include <unistd.h>
#include "os_Schedule_Printer.h"

int CreateReadyQueue(){
  ready_queue_id = QCreate("RQueue");
  return ready_queue_id;
}

/*
Create a RQ_ELEMENT with context and add to the Ready Queue
*/
void AddToReadyQueue(long context, long PID, void* PCB){

  PROCESS_CONTROL_BLOCK* process = (PROCESS_CONTROL_BLOCK*) PCB;

  RQ_ELEMENT* rqe = malloc(sizeof(RQ_ELEMENT));
  rqe->context = context;
  rqe->PID = PID;
  rqe->PCB = process;
  process->queue_ptr = (void *)rqe; //keep?

  
  long Priority = process->priority;
 
  LockLocation(READY_LOCK);

  QInsert(ready_queue_id, Priority, (void *) rqe);

  UnlockLocation(READY_LOCK);
  //osPrintState("Add RQ", PID);
 
}


/*
Checks to see if the process is in the Ready Queue. If so the process is removed.
*/
long RemoveFromReadyQueue(PROCESS_CONTROL_BLOCK* process){

  
  LockLocation(READY_LOCK);
  
  if((long)QRemoveItem(ready_queue_id, process->queue_ptr) == -1){
    aprintf("\nProcess not found in Ready Queue\n\n");
  }
  else{
    aprintf("\nProcess removed form Ready Queue\n\n");
  }
  
  UnlockLocation(READY_LOCK);
 
  return 1;
}


/*
Changes the priority of the process in the ready queue
*/
long ChangePriorityInReadyQueue(PROCESS_CONTROL_BLOCK* process, INT32 NewPriority){
  
  LockLocation(READY_LOCK);
  
  if((long)QRemoveItem(ready_queue_id, process->queue_ptr) == -1){
    aprintf("\nProcess not found in Ready Queue\n\n");
  }
  process->priority = NewPriority;
  QInsert(ready_queue_id, NewPriority, process->queue_ptr);
  
  UnlockLocation(READY_LOCK);

  return 1;
}






/*
Remove a RQ_ELEMENT from the head of the Ready Queue. Return the context stored in the RQ_ELEMENT
*/
RQ_ELEMENT* RemoveFromReadyQueueHead(){

  
  LockLocation(READY_LOCK);
  
  RQ_ELEMENT* rqe = (RQ_ELEMENT *)QRemoveHead(ready_queue_id);
   
  UnlockLocation(READY_LOCK);
  
  return rqe;
}

/*
Check the Ready Queue. Return -1 if there are no elements in the queue. Otherwise return 1.
*/
long CheckReadyQueue(){

 
  LockLocation(READY_LOCK);
  
  long result = (long)QNextItemInfo(ready_queue_id);
  
  UnlockLocation(READY_LOCK);

 
  if(result == -1){
    return -1;
  }
  else{
    return 1;
  }
}

void WasteTime(){
   printf("");
}

//can't figure out how call works right now
void dispatcher(){

  MEMORY_MAPPED_IO mmio;
 
  
  while(CheckReadyQueue() == -1){
     CALL(WasteTime());
     usleep(1);//need a sleep so other thread can get LOCK
  }
  
  RQ_ELEMENT* rqe = RemoveFromReadyQueueHead();
  long Context = rqe->context;

  osPrintState("Dispatch", rqe->PID, rqe->PID);
  
  //need PID
  // ChangeProcessState( ,RUNNING)

  free(rqe);
  
  mmio.Mode = Z502StartContext;
  mmio.Field1 = Context;
  mmio.Field2 = START_NEW_CONTEXT_AND_SUSPEND;
  mmio.Field3 = mmio.Field4 = 0;
  MEM_WRITE(Z502Context, &mmio);     // Start up the context

  if(mmio.Field4 != ERR_SUCCESS){
    aprintf("\nError in starting context in dispatcher\n");
  }
}


