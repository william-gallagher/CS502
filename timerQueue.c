#include "timerQueue.h"
#include "readyQueue.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "protos.h"
#include "process.h"
#include "os_globals.h"

//---------------------------------------------------------------------
/*
  Temporarily put Lock Functions here until I have a better spot for them

*/
/*
*/

void LockLocation(INT32 lock){
 
  INT32 SuspendUntilLocked = TRUE;
  INT32 ReturnError = -1;

  READ_MODIFY(lock, 1, SuspendUntilLocked, &ReturnError);

  if(ReturnError == FALSE){
    aprintf("\n\nCould Not Obtain the Lock# %d\n\n", lock);
  }
}

void UnlockLocation(INT32 lock){

  INT32 SuspendUntilLocked = TRUE;
  INT32 ReturnError = -1;

  READ_MODIFY(lock, 0, SuspendUntilLocked, &ReturnError);

  if(ReturnError == FALSE){
    aprintf("\n\nCould not give up Lock# %d\n\n", lock);
  }
}

void GetTimeOfDay(long *TimeOfDay){
  
  MEMORY_MAPPED_IO mmio;
  
  mmio.Mode = Z502ReturnValue;
  mmio.Field1 = mmio.Field2 = mmio.Field3 = mmio.Field4 = 0;
  MEM_READ(Z502Clock, &mmio);
  
  (*TimeOfDay) =  mmio.Field1;
}





/*
Looks at the Timer Queue and returns the wakeup time of the next item on the queue. If there is nothing else on the Timer Queue NextWakeUp is set to -1. This function can be used to test for the existance of any items on the Timer Queue.
*/
void GetNextWakeUpTime(long* NextWakeUp){
  
  LockLocation(TIMER_LOCK);
 
  TQ_ELEMENT* tqe = QNextItemInfo(timer_queue_id);

  if((long)tqe != -1){
      (*NextWakeUp) = tqe->wakeup_time;
  }
  else{
    (*NextWakeUp) = -1;
  }

  UnlockLocation(TIMER_LOCK);
 
}




/*
Create a TQ_ELEMENT with context and wakeup time and add to the timer queue
*/
void AddTimerToQueue(long context, long wakeup_time, void* PCB){

  TQ_ELEMENT* tqe = malloc(sizeof(TQ_ELEMENT));
  tqe->context = context;
  tqe->wakeup_time = wakeup_time;
  tqe->PID = ((PROCESS_CONTROL_BLOCK *)PCB)->idnum;
  tqe->PCB = PCB;

  
  LockLocation(TIMER_LOCK);

  QInsert(timer_queue_id, (unsigned int)wakeup_time, (void*)tqe);
 
  UnlockLocation(TIMER_LOCK);
}


/*
Remove a TQ_ELEMENT from the Timer Queue. Return the context stored in the TQ_ELEMENT
*/
TQ_ELEMENT* RemoveTimerFromQueue(){

  
  LockLocation(TIMER_LOCK);
  
  TQ_ELEMENT* tqe = (TQ_ELEMENT *)QRemoveHead(timer_queue_id);

  UnlockLocation(TIMER_LOCK);

  return tqe;
}
  


void StartTimer(long SleepTime){

  long CurrentTime;
  MEMORY_MAPPED_IO mmio; 

  long context = osGetCurrentContext();
  GetTimeOfDay(&CurrentTime);
  
  //set the wakeup time
  long wakeup_time = CurrentTime + SleepTime;

  //Check to see if Timer is busy
  long NextWakeUp;
  GetNextWakeUpTime(&NextWakeUp);

  //No more items on Timer Queue.
  if(NextWakeUp == -1){
     // Start the timer 
    mmio.Mode = Z502Start;
    mmio.Field1 = SleepTime;   
    mmio.Field2 = mmio.Field3 =mmio.Field4 = 0;
    MEM_WRITE(Z502Timer, &mmio);
  }
  else{
    if(NextWakeUp > wakeup_time){
      // Start the timer 
      mmio.Mode = Z502Start;
      mmio.Field1 = SleepTime;   
      mmio.Field2 = mmio.Field3 =mmio.Field4 = 0;
      MEM_WRITE(Z502Timer, &mmio);
    }
  }//don't need to restart if new wakeup time is after NextWakeUp

  //add tqe to the timer queue
  AddTimerToQueue(context, wakeup_time, GetCurrentPCB());

  //set process state to TIMER
  ChangeProcessState(GetCurrentPID(), TIMER);

 
  //check return of start timer
  if(mmio.Field4 != ERR_SUCCESS){
    printf("Error starting the timer\n\n");
  }

    dispatcher();
}

/*
Remove the first element of the Timer Queue. Check to see if the Timer needs to be reset. If there is another process with only a brief amount of time until its wake up time it is also put on the Ready Queue.
*/

void HandleTimerInterrupt(){

  MEMORY_MAPPED_IO mmio;
  INT32 RemoveAgain;

  do{

    TQ_ELEMENT* tq = RemoveTimerFromQueue();
    AddToReadyQueue(tq->context, tq->PID, tq->PCB);
 
    //Check for another timer on the Queue
    TQ_ELEMENT* next_timer = (TQ_ELEMENT*) QNextItemInfo(timer_queue_id);

    //no more processes on the Timer Queue
    if((long)next_timer == -1){
      RemoveAgain = FALSE;
    }
    //There is another process of Timer Queue
    else{
      //get the current time
      mmio.Mode = Z502ReturnValue;
      mmio.Field1 = mmio.Field2 = mmio.Field3 = mmio.Field4;
      MEM_READ(Z502Clock, &mmio);
      long CurrentTime = mmio.Field1;

      long WakeTime = next_timer->wakeup_time;
      aprintf("\n\nThe current time is %ld and the wakeup time is %ld\n\n", CurrentTime, WakeTime);

      if(WakeTime - CurrentTime > 10){
	//reset timer 
	mmio.Mode = Z502Start;
	mmio.Field1 = WakeTime - CurrentTime;   
	mmio.Field2 = mmio.Field3 = 0;
	MEM_WRITE(Z502Timer, &mmio);

	//check return of start timer
	if(mmio.Field4 != ERR_SUCCESS){
	  printf("Error starting the timer\n\n");
	}
	RemoveAgain = FALSE;
      }
      else{ //WakeTime - Current <= 10
	RemoveAgain = TRUE;
      }
    }
  }while(RemoveAgain == TRUE);
  
  
}

//really doesnt need an int return
//use functions in QueueManager.c to create a timer for storing 
int CreateTimerQueue(){
  timer_queue_id = QCreate("TQueue");
  return timer_queue_id;
}



