#include "timerQueue.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "protos.h"
#include "process.h"
#include "os_globals.h"

int CreateSuspendQueue(){
  suspend_queue_id = QCreate("SQueue");
  return suspend_queue_id;
}






int CreateReadyQueue(){
  ready_queue_id = QCreate("RQueue");
  return ready_queue_id;
}

/*
Create a RQ_ELEMENT with context and add to the Ready Queue
*/
void AddToReadyQueue(long context){

  PROCESS_CONTROL_BLOCK* process = GetPCBContext(context);
  
  //Check for access to Ready Queue
  INT32 Suspend_Until_Locked = TRUE;
  INT32 Sucess_Failure = -1;

  READ_MODIFY(READY_LOCK, 1, Suspend_Until_Locked, &Sucess_Failure);

  if(Sucess_Failure == FALSE){
    aprintf("\n\nCould Not Obtain the Lock for the Ready Queue\n\n");
  }
  
  RQ_ELEMENT* rqe = malloc(sizeof(RQ_ELEMENT));
  rqe->context = context;
  process->queue_ptr = (void *)rqe;

  QInsertOnTail(ready_queue_id, (void *) rqe);
  QPrint(ready_queue_id);
  
  READ_MODIFY(READY_LOCK, 0, Suspend_Until_Locked, &Sucess_Failure);

  if(Sucess_Failure == FALSE){
    aprintf("\n\n Failure to give up Ready Lock\n\n");
  }
}


/*
Checks to see if the process is in the Ready Queue. If so the process is removed.
*/
long RemoveFromReadyQueue(PROCESS_CONTROL_BLOCK* process){

  //Check for access to Ready Queue
  INT32 Suspend_Until_Locked = TRUE;
  INT32 Sucess_Failure = -1;

  READ_MODIFY(READY_LOCK, 1, Suspend_Until_Locked, &Sucess_Failure);

  if(Sucess_Failure == FALSE){
    aprintf("\n\nCould Not Obtain the Lock for the Ready Lock\n\n");
  }
  
  if((long)QRemoveItem(ready_queue_id, process->queue_ptr) == -1){
    aprintf("\nProcess not found in Ready Queue\n\n");
  }
  else{
    aprintf("\nProcess removed form Ready Queue\n\n");
  }    
    
  QPrint(ready_queue_id);
  
  READ_MODIFY(READY_LOCK, 0, Suspend_Until_Locked, &Sucess_Failure);

  if(Sucess_Failure == FALSE){
    aprintf("\n\nCould not give up Lock for Ready Lock\n\n");
  }

  return 1;
}



/*
Remove a RQ_ELEMENT from the head of the Ready Queue. Return the context stored in the RQ_ELEMENT
*/
long RemoveFromReadyQueueHead(){

  //Check for access to Ready Queue
  INT32 Suspend_Until_Locked = TRUE;
  INT32 Sucess_Failure = -1;

  READ_MODIFY(READY_LOCK, 1, Suspend_Until_Locked, &Sucess_Failure);

  if(Sucess_Failure == FALSE){
    aprintf("\n\nCould Not Obtain the Lock for the Ready Lock\n\n");
  }
  
  RQ_ELEMENT* rqe = (RQ_ELEMENT *)QRemoveHead(ready_queue_id);
  QPrint(ready_queue_id);
  
  READ_MODIFY(READY_LOCK, 0, Suspend_Until_Locked, &Sucess_Failure);

  if(Sucess_Failure == FALSE){
    aprintf("\n\nCould not give up Lock for Ready Lock\n\n");
  }

  return rqe->context;
}

/*
Check the Ready Queue. Return -1 if there are no elements in the queue. Otherwise return 1.
*/
long CheckReadyQueue(){

  //Check for access to Ready Queue
  INT32 Suspend_Until_Locked = TRUE;
  INT32 Sucess_Failure = -1;

  READ_MODIFY(READY_LOCK, 1, Suspend_Until_Locked, &Sucess_Failure);

  if(Sucess_Failure == FALSE){
    aprintf("\n\nCould Not Obtain the Lock for the Ready Lock\n\n");
  }
  
  long result = (long)QNextItemInfo(ready_queue_id);
  
  READ_MODIFY(READY_LOCK, 0, Suspend_Until_Locked, &Sucess_Failure);

  if(Sucess_Failure == FALSE){
    aprintf("\n\nCould not give up Lock for Ready Lock\n\n");
  }

  if(result == -1){
    return -1;
  }
  else{
    return 1;
  }
}

















/*
Create a TQ_ELEMENT with context and wakeup time and add to the timer queue
*/
void AddTimerToQueue(long context, long wakeup_time){

  //Check for access to TIMER QUEUE
  INT32 Suspend_Until_Locked = TRUE;
  INT32 Sucess_Failure = -1;

  READ_MODIFY(TIMER_LOCK, 1, Suspend_Until_Locked, &Sucess_Failure);

  if(Sucess_Failure == FALSE){
    aprintf("\n\nCould Not Obtain the Lock for the Timer Queue\n\n");
  }
  
  TQ_ELEMENT* tqe = malloc(sizeof(TQ_ELEMENT));
  tqe->context = context;
  tqe->wakeup_time = wakeup_time;

  QInsert(timer_queue_id, (unsigned int)wakeup_time, (void*)tqe);
  QPrint(timer_queue_id);
  
  READ_MODIFY(TIMER_LOCK, 0, Suspend_Until_Locked, &Sucess_Failure);

  if(Sucess_Failure == TRUE){
    aprintf("\n\n Sucess in giving up memory\n\n");
  }
}


/*
Remove a TQ_ELEMENT from the Timer Queue. Return the context stored in the TQ_ELEMENT
*/
long RemoveTimerFromQueue(){

  //Check for access to TIMER QUEUE
  INT32 Suspend_Until_Locked = TRUE;
  INT32 Sucess_Failure = -1;

  READ_MODIFY(TIMER_LOCK, 1, Suspend_Until_Locked, &Sucess_Failure);

  if(Sucess_Failure == FALSE){
    aprintf("\n\nCould Not Obtain the Lock for the Timer Queue\n\n");
  }
  
  TQ_ELEMENT* tqe = (TQ_ELEMENT *)QRemoveHead(timer_queue_id);
  QPrint(timer_queue_id);
  
  READ_MODIFY(TIMER_LOCK, 0, Suspend_Until_Locked, &Sucess_Failure);

  if(Sucess_Failure == TRUE){
    aprintf("\n\n Sucess in giving up memory\n\n");
  }

  return tqe->context;
}
  

//can't figure out how call works right now
void dispatcher(){

  MEMORY_MAPPED_IO mmio;

  if(CheckReadyQueue() == -1){
  //if((long)QNextItemInfo(ready_queue_id) == -1){
    // aprintf("\n\nNothing on the Queue!\n\n");
  // CALL();//advances simulation to next interrupt
  
    //idle process
    mmio.Mode = Z502Action;
    mmio.Field1 = mmio.Field2 = mmio.Field3 = 0;
    MEM_WRITE(Z502Idle, &mmio);
  }

  //seems like have to add a small delay for interrupt to finish before 
  while(CheckReadyQueue() == -1){}

   //RQ_ELEMENT* rqe = (RQ_ELEMENT *)QRemoveHead(ready_queue_id);
   long context = RemoveFromReadyQueueHead();

  mmio.Mode = Z502ReturnValue;
  mmio.Field1 = mmio.Field2 = mmio.Field3 = 0;
  MEM_READ(Z502Clock, &mmio);
  aprintf("In the dispatcher at  %ld\n\n", mmio.Field1);

  
  mmio.Mode = Z502StartContext;
  mmio.Field1 = context;
  mmio.Field2 = START_NEW_CONTEXT_AND_SUSPEND;
  mmio.Field3 = mmio.Field4 = 0;
  MEM_WRITE(Z502Context, &mmio);     // Start up the context

  
}

void StartTimer(long sleep_time){

  long CurrentTime;
  MEMORY_MAPPED_IO mmio; 

  //get context of process
  mmio.Mode = Z502GetCurrentContext;
  mmio.Field1 = mmio.Field2 = mmio.Field3 = mmio.Field4 = 0;
  MEM_READ(Z502Context, &mmio);
  
  long context = mmio.Field1;
  
  printf("here is the context getting added to the queue %lx\n",context);
  printf("sleeping %ld time units\n\n", sleep_time);

  //get the current time
  mmio.Mode = Z502ReturnValue;
  mmio.Field1 = mmio.Field2 = mmio.Field3 = mmio.Field4;
  MEM_READ(Z502Clock, &mmio);
  CurrentTime = mmio.Field1;
  aprintf("Here is current time %ld\n", CurrentTime);

  //set the wakeup time
  long wakeup_time = CurrentTime + sleep_time;
  
  //add tqe to the timer queue
  AddTimerToQueue(context, wakeup_time);

  // Start the timer 
  mmio.Mode = Z502Start;
  mmio.Field1 = sleep_time;   
  mmio.Field2 = mmio.Field3 = 0;
  MEM_WRITE(Z502Timer, &mmio);

  //check return of start timer
  if(mmio.Field4 != ERR_SUCCESS){
    printf("Error starting the timer\n\n");
  }

    dispatcher();
}

void HandleTimerInterrupt(){

  MEMORY_MAPPED_IO mmio;
  // TQ_ELEMENT* tqe = (TQ_ELEMENT *)QRemoveHead(timer_queue_id);

  long context = RemoveTimerFromQueue();
  
  printf("Here is the context of the process removed from the Timer Queue %lx\n", context);
  /*
  //Add to the ready queue
  RQ_ELEMENT* rqe = malloc(sizeof(RQ_ELEMENT));
  rqe->context =  context; 
  QInsertOnTail(ready_queue_id, (void *) rqe);
  QPrint(ready_queue_id);
  */
  AddToReadyQueue(context);
  
  //reset timer if another timer is on the queue
  TQ_ELEMENT* next_timer = (TQ_ELEMENT*) QNextItemInfo(timer_queue_id);
  if((long)next_timer != -1){
    //get the current time
    mmio.Mode = Z502ReturnValue;
    mmio.Field1 = mmio.Field2 = mmio.Field3 = mmio.Field4;
    MEM_READ(Z502Clock, &mmio);
    long CurrentTime = mmio.Field1;

    long WakeTime = next_timer->wakeup_time;
    aprintf("\n\nThe current time is %ld and the wakeup time is %ld\n\n", CurrentTime, WakeTime);

    //reset timer 
    mmio.Mode = Z502Start;
    mmio.Field1 = WakeTime - CurrentTime;   
    mmio.Field2 = mmio.Field3 = 0;
    MEM_WRITE(Z502Timer, &mmio);

    //check return of start timer
    if(mmio.Field4 != ERR_SUCCESS){
      printf("Error starting the timer\n\n");
    }
    else{
      printf("\nNo more timers on Q\n");
    }

    
  }
  
}

//really doesnt need an int return
//use functions in QueueManager.c to create a timer for storing 
int CreateTimerQueue(){
  timer_queue_id = QCreate("TQueue");
  return timer_queue_id;
}

int CreateDiskQueue(int disk_number){
  char num[2];
  snprintf((char *)(&num), 2, "%d", disk_number);
  char queue_name[8];
  strcpy(queue_name, "DQUEUE_");
  strcat(queue_name, num);
  printf("Here is the name: %s\n\n", queue_name);
  disk_queue[disk_number] = QCreate(queue_name);
  return 1;
}

void InitializeDiskLocks(){

  for(int i=0; i<MAX_NUMBER_OF_DISKS; i++){
    DISK_LOCK[i] = MEMORY_INTERLOCK_BASE + 2 + i;
  }
}
