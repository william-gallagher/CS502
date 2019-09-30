#include "timerQueue.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "protos.h"
#include "process.h"
#include "os_globals.h"
#include <unistd.h>

int CreateSuspendQueue(){
  suspend_queue_id = QCreate("SQueue");
  return suspend_queue_id;
}

int CreateMessageQueue(){
  message_queue_id = QCreate("MQueue");
  return message_queue_id;
}




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
  
  //Check for access to Ready Queue
  INT32 Suspend_Until_Locked = TRUE;
  INT32 Sucess_Failure = -1;

  
  READ_MODIFY(READY_LOCK, 1, Suspend_Until_Locked, &Sucess_Failure);

  if(Sucess_Failure == FALSE){
    aprintf("\n\nCould Not Obtain the Lock for the Ready Queue\n\n");
  }

  QInsert(ready_queue_id, Priority, (void *) rqe);

  // QInsertOnTail(ready_queue_id, (void *) rqe);
  // QPrint(ready_queue_id);
  
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
    
  // QPrint(ready_queue_id);
  
  READ_MODIFY(READY_LOCK, 0, Suspend_Until_Locked, &Sucess_Failure);

  if(Sucess_Failure == FALSE){
    aprintf("\n\nCould not give up Lock for Ready Lock\n\n");
  }

  return 1;
}










/*
Changes the priority of the process in the ready queue
*/
long ChangePriorityInReadyQueue(PROCESS_CONTROL_BLOCK* process, INT32 NewPriority){

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
  process->priority = NewPriority;
  QInsert(ready_queue_id, NewPriority, process->queue_ptr);
    
  // QPrint(ready_queue_id);
  
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
  // QPrint(ready_queue_id);
  
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
    if(Sucess_Failure == TRUE){
      //aprintf("\n\nGave up Lock for Ready Lock\n\n");
  }

    MEMORY_MAPPED_IO mmio;       
    mmio.Mode = Z502ReturnValue;
    mmio.Field1 = mmio.Field2 = mmio.Field3 = mmio.Field4;
    MEM_READ(Z502Clock, &mmio);
    long CurrentTime = mmio.Field1;

    //aprintf("Time of check %ld\n\n", CurrentTime);
  
  if(result == -1){
    return -1;
  }
  else{
    return 1;
  }
}






/*
Looks at the Timer Queue and returns the wakeup time of the next item on the queue. If there is nothing else on the Timer Queue NextWakeUp is set to -1. This function can be used to test for the existance of any items on the Timer Queue.
*/
void GetNextWakeUpTime(long* NextWakeUp){
  
  //Check for access to TIMER QUEUE
  INT32 Suspend_Until_Locked = TRUE;
  INT32 Sucess_Failure = -1; 

  READ_MODIFY(TIMER_LOCK, 1, Suspend_Until_Locked, &Sucess_Failure);

  if(Sucess_Failure == FALSE){
    aprintf("\n\nCould Not Obtain the Lock for the Timer Queue\n\n");
  }
  

  TQ_ELEMENT* tqe = QNextItemInfo(timer_queue_id);

  if((long)tqe != -1){
      (*NextWakeUp) = tqe->wakeup_time;
  }
  else{
    (*NextWakeUp) = -1;
  }   
  
  READ_MODIFY(TIMER_LOCK, 0, Suspend_Until_Locked, &Sucess_Failure);

  if(Sucess_Failure == FALSE){
    aprintf("\n\nCould not give up Timer Lock\n\n");
  }
}




/*
Create a TQ_ELEMENT with context and wakeup time and add to the timer queue
*/
void AddTimerToQueue(long context, long wakeup_time, void* PCB){

  //Check for access to TIMER QUEUE
  INT32 Suspend_Until_Locked = TRUE;
  INT32 Sucess_Failure = -1;

  TQ_ELEMENT* tqe = malloc(sizeof(TQ_ELEMENT));
  tqe->context = context;
  tqe->wakeup_time = wakeup_time;
  tqe->PID = ((PROCESS_CONTROL_BLOCK *)PCB)->idnum;
  tqe->PCB = PCB;

  READ_MODIFY(TIMER_LOCK, 1, Suspend_Until_Locked, &Sucess_Failure);

  if(Sucess_Failure == FALSE){
    aprintf("\n\nCould Not Obtain the Lock for the Timer Queue\n\n");
  }
  

  QInsert(timer_queue_id, (unsigned int)wakeup_time, (void*)tqe);
  // QPrint(timer_queue_id);
  
  READ_MODIFY(TIMER_LOCK, 0, Suspend_Until_Locked, &Sucess_Failure);

  if(Sucess_Failure == FALSE){
    aprintf("\n\nCould not give up Timer Lock\n\n");
  }
}


/*
Remove a TQ_ELEMENT from the Timer Queue. Return the context stored in the TQ_ELEMENT
*/
TQ_ELEMENT* RemoveTimerFromQueue(){

  //Check for access to TIMER QUEUE
  INT32 Suspend_Until_Locked = TRUE;
  INT32 Sucess_Failure = -1;

  READ_MODIFY(TIMER_LOCK, 1, Suspend_Until_Locked, &Sucess_Failure);

  if(Sucess_Failure == FALSE){
    aprintf("\n\nCould Not Obtain the Lock for the Timer Queue\n\n");
  }
  
  TQ_ELEMENT* tqe = (TQ_ELEMENT *)QRemoveHead(timer_queue_id);
  // QPrint(timer_queue_id);
  
  READ_MODIFY(TIMER_LOCK, 0, Suspend_Until_Locked, &Sucess_Failure);

  if(Sucess_Failure == FALSE){
    aprintf("\n\nCould not give up Timer Lock\n\n");
  }

  return tqe;
}
  

void WasteTime(){
   printf("");
}

//can't figure out how call works right now
void dispatcher(){

  MEMORY_MAPPED_IO mmio;
  /* 

     //the old way of sleeping before implementing CALL

  if(CheckReadyQueue() == -1){
  if((long)QNextItemInfo(ready_queue_id) == -1){
    // aprintf("\n\nNothing on the Queue!\n\n");
  // CALL();//advances simulation to next interrupt
  
    //idle process
    mmio.Mode = Z502Action;
    mmio.Field1 = mmio.Field2 = mmio.Field3 = 0;
    MEM_WRITE(Z502Idle, &mmio);
  }

  //seems like have to add a small delay for interrupt to finish before 
  while(CheckReadyQueue() == -1){}
  */
  
  while(CheckReadyQueue() == -1){
     CALL(WasteTime());
     usleep(1);//need a sleep so other thread can get LOCK
  }
  
  long context = RemoveFromReadyQueueHead();

  //need PID
  // ChangeProcessState( ,RUNNING)
  
  mmio.Mode = Z502StartContext;
  mmio.Field1 = context;
  mmio.Field2 = START_NEW_CONTEXT_AND_SUSPEND;
  mmio.Field3 = mmio.Field4 = 0;
  MEM_WRITE(Z502Context, &mmio);     // Start up the context

  if(mmio.Field4 != ERR_SUCCESS){
    aprintf("\nError in starting context in dispatcher\n");
  }

  
}

void StartTimer(long sleep_time){

  long CurrentTime;
  MEMORY_MAPPED_IO mmio; 

  long context = osGetCurrentContext();
  
  aprintf("here is the context getting added to the queue %lx\n",context);
  aprintf("sleeping %ld time units\n\n", sleep_time);

  //get the current time
  mmio.Mode = Z502ReturnValue;
  mmio.Field1 = mmio.Field2 = mmio.Field3 = mmio.Field4;
  MEM_READ(Z502Clock, &mmio);
  CurrentTime = mmio.Field1;
  aprintf("Here is current time %ld\n", CurrentTime);

  //set the wakeup time
  long wakeup_time = CurrentTime + sleep_time;


  //Check to see if Timer is busy
  long NextWakeUp;
  GetNextWakeUpTime(&NextWakeUp);

  //No more items on Timer Queue.
  if(NextWakeUp == -1){
     // Start the timer 
    mmio.Mode = Z502Start;
    mmio.Field1 = sleep_time;   
    mmio.Field2 = mmio.Field3 =mmio.Field4 = 0;
    MEM_WRITE(Z502Timer, &mmio);
  }
  else{
    if(NextWakeUp > wakeup_time){
      // Start the timer 
      mmio.Mode = Z502Start;
      mmio.Field1 = sleep_time;   
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

int CreateDiskQueue(int disk_number){
  char num[2];
  snprintf((char *)(&num), 2, "%d", disk_number);
  char queue_name[8];
  strcpy(queue_name, "DQUEUE_");
  strcat(queue_name, num);
  //  printf("Here is the name: %s\n\n", queue_name);
  disk_queue[disk_number] = QCreate(queue_name);
  return 1;
}

void InitializeDiskLocks(){

  for(int i=0; i<MAX_NUMBER_OF_DISKS; i++){
    DISK_LOCK[i] = MEMORY_INTERLOCK_BASE + 2 + i;  }
}

/*
Receives the id of a disk and checks to see if the disk is busy performing I/O operations. Returns DEVICE_IN_USE or DEVICE_FREE in address pointed to by status.
*/
void CheckDiskStatus(long disk_id, long* status){

  MEMORY_MAPPED_IO mmio;
  mmio.Mode = Z502Status;
  mmio.Field1 = disk_id;
  mmio.Field2 = mmio.Field3 = mmio.Field4 = 0;

  MEM_READ(Z502Disk, &mmio);

  (*status) = mmio.Field2;
}

/*
Adds to Disk Queue. head_or_tail indicates whether to add to the head or tail of the Disk Queue.
*/
void AddToDiskQueue(long disk_id, DQ_ELEMENT* dqe){

   
  //Check for access to Disk Queue
  INT32 Suspend_Until_Locked = TRUE;
  INT32 Sucess_Failure = -1;

  READ_MODIFY(DISK_LOCK[disk_id], 1, Suspend_Until_Locked, &Sucess_Failure);

  if(Sucess_Failure == FALSE){
    aprintf("\n\nCould Not Obtain the Lock for Disk Queue %ld\n\n", disk_id);
  }
 

    QInsertOnTail(disk_queue[disk_id], (void *) dqe);
    //QPrint(disk_queue[disk_id]);
 


  READ_MODIFY(DISK_LOCK[disk_id], 0, Suspend_Until_Locked, &Sucess_Failure);

  if(Sucess_Failure == FALSE){
    aprintf("\n\n Failure to give up the Lock for Disk Queue\n\n");
  }

}



//======================================================================
/*

Message Buffer Functions

*/




/*
Counts the number of messages in the Message Buffer
*/
long CountMessagesInBuffer(){

  long MessageCount = 0;
  long Index = 0;
  MQ_ELEMENT *mqe;
  
  INT32 Suspend_Until_Locked = TRUE;
  INT32 Sucess_Failure = -1;

  READ_MODIFY(MESSAGE_LOCK, 1, Suspend_Until_Locked, &Sucess_Failure);

  if(Sucess_Failure == FALSE){
    aprintf("\n\nCould Not Obtain the Lock for Message Buffer %ld\n\n");
  }

  mqe = QWalk(message_queue_id, Index);
  
  while((long)mqe != -1){
    Index++;
    MessageCount++;
    mqe = QWalk(message_queue_id, Index);
  }

  READ_MODIFY(MESSAGE_LOCK, 0, Suspend_Until_Locked, &Sucess_Failure);

  if(Sucess_Failure == FALSE){
    aprintf("\n\n Failure to give up the Lock for Message Buffer\n\n");
  }

  return MessageCount;
}


void AddToMessageBuffer(MQ_ELEMENT* mqe){
  
  INT32 Suspend_Until_Locked = TRUE;
  INT32 Sucess_Failure = -1;

  READ_MODIFY(MESSAGE_LOCK, 1, Suspend_Until_Locked, &Sucess_Failure);

  if(Sucess_Failure == FALSE){
    aprintf("\n\nCould Not Obtain the Lock for Message Buffer %ld\n\n");
  }

  QInsertOnTail(message_queue_id, mqe);

  READ_MODIFY(MESSAGE_LOCK, 0, Suspend_Until_Locked, &Sucess_Failure);

  if(Sucess_Failure == FALSE){
    aprintf("\n\n Failure to give up the Lock for Message Buffer\n\n");
  }
}

MQ_ELEMENT* GetMessageFromBuffer(long SourcePID, long CurrentPID){

  aprintf("In GetMessage %ld is the SenderPID that I am looking for\n", SourcePID);
  
  MQ_ELEMENT* mqe;
  int Index = 0;

  INT32 Suspend_Until_Locked = TRUE;
  INT32 Sucess_Failure = -1;

  READ_MODIFY(MESSAGE_LOCK, 1, Suspend_Until_Locked, &Sucess_Failure);

  if(Sucess_Failure == FALSE){
    aprintf("\n\nCould Not Obtain the Lock for the Message Buffer\n\n");
  }

  QPrint(message_queue_id);
  
  mqe = QWalk(message_queue_id, Index);
  
  while((long)mqe != -1){
    aprintf("Here is the source pid %ld\nhere is the target pid %ld\n\n", mqe->sender_pid,  mqe->target_pid);
    if((mqe->target_pid == CurrentPID && mqe->sender_pid == SourcePID)||
       ((mqe->target_pid == -1 || mqe->target_pid == CurrentPID)  && SourcePID == -1)){

      mqe = QRemoveItem(message_queue_id, (void *)mqe);
      aprintf("Found a message! %ld is the PID \n\n", mqe->target_pid);
      break;
    }
    Index++;
    mqe = QWalk(message_queue_id, Index);
  }

  READ_MODIFY(MESSAGE_LOCK, 0, Suspend_Until_Locked, &Sucess_Failure);

  if(Sucess_Failure == FALSE){
    aprintf("\n\n Failure to give up Message Lock\n\n");
  }
  return mqe;

}
  


void osSendMessage(long TargetPID, char* MessageBuffer, long MessageLength, long* ReturnError){

  PROCESS_CONTROL_BLOCK *process;
  
    //check to see if process exists
  if(TargetPID != -1){
    process = GetPCB(TargetPID);
    if(process == NULL){
      aprintf("Cannot send message to process of a PID that DNE! PID %ld\n\n", TargetPID);
      (*ReturnError) = 1;
      return;
    }
  }

  //Make sure MessageLength is in the right range
  if(MessageLength < 0 || MessageLength > MAX_MESSAGE_LENGTH){
    aprintf("Message Length out of range!\n\n");
    (*ReturnError) = 1;
    return;
  }

  //Check to make sure there is room in the Message Buffer
  if(CountMessagesInBuffer() >= MAX_MESSAGES_IN_BUFFER){
    aprintf("Message Buffer Full!\n\n");
    (*ReturnError) = 1;
    return;
  }

  MQ_ELEMENT *mqe = malloc(sizeof(MQ_ELEMENT));
  mqe->target_pid = TargetPID;
  strcpy(mqe->message, MessageBuffer);
  mqe->message_length = MessageLength;
  mqe->sender_pid = GetCurrentPID();
  AddToMessageBuffer(mqe);

  //check to see what the state of the target process is
  if(TargetPID != -1){
    INT32 State;
    long ReturnErrorResume;
  
    GetProcessState(TargetPID, &State);
    if(State == SUSPENDED){
      osResumeProcess(TargetPID, &ReturnErrorResume);
      if(ReturnErrorResume == 1){
	aprintf("Failure to Resume Process in osSendMessage\n\n");
      }
      ChangeProcessState(TargetPID, READY);
    }

    //set flag in PCB to let process know it has a message waiting
    process->waiting_for_message = TRUE;
  }

  //return success
  (*ReturnError) = 0;
}

void osReceiveMessage(long SourcePID, char* MessageBuffer,
		      long MessRecLength, long* MessSendLength,
		      long* SenderPID, long* ReturnError){

  long CurrentPID = GetCurrentPID();
  
  //check to see if process exists. -1 always allowed.
  if(SourcePID != -1){
    
    PROCESS_CONTROL_BLOCK* process = GetPCB(SourcePID);;

    if(process == NULL){
      aprintf("Cannot receive message from process that DNE!\n\n");
      (*ReturnError) = 1;
      return;
    }
  }

  //Make sure MessageLength is in the right range
  if(MessRecLength < 0 || MessRecLength > MAX_MESSAGE_LENGTH){
    aprintf("Can't Receive Message Length out of range!\n\n");
    (*ReturnError) = 1;
    return;
  }

  //check Message Buffer for message. If it there is none suspend process
  aprintf("Before Check\n\n");
  MQ_ELEMENT *mqe = GetMessageFromBuffer(SourcePID, CurrentPID);
  aprintf("Returned from checking for message\n\n");
  
  if((long)mqe == -1){
    ChangeProcessState(CurrentPID, SUSPENDED);
    dispatcher();
    mqe = GetMessageFromBuffer(SourcePID, CurrentPID);
  }

  strcpy(MessageBuffer, mqe->message);
  (*MessSendLength) = mqe->message_length;
  (*SenderPID) = mqe->sender_pid;
  (*ReturnError) = 0;
}
