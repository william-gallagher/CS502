#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "timerQueue.h"
#include "readyQueue.h"
#include "process.h"
#include "protos.h"
#include "os_globals.h"


//need to remember to free at end somehow
//Also created a lock for each process
void CreatePRO_INFO(){
  PRO_INFO = malloc(sizeof(PROCESSES_INFORMATION));
  for(int i=0; i<MAXPROCESSES; i++){
    PRO_INFO->PCB[i].in_use = 0;
    PRO_INFO->PCB[i].LOCK = (PROC_LOCK_BASE + i);
  }
  
  PRO_INFO->nextid = 0;
}

//get the first available slot for a PCB
INT32 GetAvailableSlot(){
  for(INT32 i=0; i<MAXPROCESSES; i++){
    if(PRO_INFO->PCB[i].in_use != 1){
      return i;
    }
  }
  return -1;
}


long osCreateProcess(char* name, long context, INT32 parent, long Priority){


  //Need to get the next available slot.
  INT32 next_slot = GetAvailableSlot();
  PROCESS_CONTROL_BLOCK* new_pcb = &PRO_INFO->PCB[next_slot];


  new_pcb->idnum = PRO_INFO->nextid;
  PRO_INFO->nextid++; //eventually need a more sophisticated method for reusing pid's
  strcpy(new_pcb->name, name);
  new_pcb->priority = Priority;
  new_pcb->context = context;
  new_pcb->in_use = 1;
  new_pcb->parent = parent;
  new_pcb->state = RUNNING;
  
  return new_pcb->idnum;
}

/*Get the PID of the currently running process. This is accomplished by getting the context of the process through Memory Mapped IO Function Z50GetCurrentContext. This context is compared to the list of existing processes to determine which is running.
 */
long GetCurrentPID(){

  MEMORY_MAPPED_IO mmio;
  mmio.Mode = Z502GetCurrentContext;
  mmio.Field1 = mmio.Field2 = mmio.Field3 = mmio.Field4 = 0;
  MEM_READ(Z502Context, &mmio);

  if(mmio.Field4 != ERR_SUCCESS){
    aprintf("\nFailed to Get Current Context\n");
  }

  long context = mmio.Field1;

  for(int i=0; i<MAXPROCESSES; i++){
    if(PRO_INFO->PCB[i].in_use != 0){
      if(PRO_INFO->PCB[i].context == context){
	return PRO_INFO->PCB[i].idnum;
      }
    }
  }
  return -1; 
}

 
void GetProcessID(char ProcessName[], long *PID, long *ReturnError){

  //Requesting current process
  if(strcmp(ProcessName, "") == 0){
    (*PID) = GetCurrentPID();
    (*ReturnError) = ERR_SUCCESS;
  }
  else{
    for(int i=0; i<MAXPROCESSES; i++){
      if(PRO_INFO->PCB[i].in_use != 0){
	if(strcmp(ProcessName, PRO_INFO->PCB[i].name) == 0){
	  (*PID) = PRO_INFO->PCB[i].idnum;
	  (*ReturnError) = ERR_SUCCESS;
	  return;
	}
      }
    }
    (*ReturnError) = ERR_BAD_PARAM;
  }
}


/*This function accepts a context. It sets the pointer to the PCB of the PID.
 */
void* GetPCBContext(long context){

  PROCESS_CONTROL_BLOCK* process;
  
  for(int i=0; i<MAXPROCESSES; i++){
    if(PRO_INFO->PCB[i].in_use != 0){

      if(PRO_INFO->PCB[i].context == context){

	process = &PRO_INFO->PCB[i];
	return (void *) process;
      }
    }
  }
  process = NULL;
  aprintf("Could not find the proper PCB from that context\n");
  return (void *) process;
}



/*This function accepts a PID. It returns the pointer to the PCB of the PID.
 */
void* GetPCB(long PID){

  PROCESS_CONTROL_BLOCK* process;
  
  for(int i=0; i<MAXPROCESSES; i++){
    if(PRO_INFO->PCB[i].in_use != 0){
      //  printf("%d is in use \n\n", i);
      if(PRO_INFO->PCB[i].idnum == PID){
	//	printf("here at %d\n", i);
	process = &PRO_INFO->PCB[i];
	return (void*) process;
      }
    }
  }
  process = NULL;
  return process;

}

//Returns a pointer to the PCB of the currently executing process
void* GetCurrentPCB(){

  long PID = GetCurrentPID();
  void* current_PCB = GetPCB(PID);

  return current_PCB;
}

/*
Returns the context of the currently executing process by using the z502GetCurrentContext call
*/
long osGetCurrentContext(){
  
  MEMORY_MAPPED_IO mmio;
  mmio.Mode = Z502GetCurrentContext;
  mmio.Field1 = mmio.Field2 = mmio.Field3 = mmio.Field4 = 0;
  MEM_READ(Z502Context, &mmio);

  if(mmio.Field4 != ERR_SUCCESS){
    aprintf("\nFailed to Get Current Context\n");
  }

  long context = mmio.Field1;
  return context;
}
  
void osSuspendProcess(long PID, long* return_error){

  //Check to see if process is the current process
  if(GetCurrentPID() == PID){
    aprintf("Cannot suspend the current process!\n\n");
    (*return_error) = 1;
    return;
  }

  //check to see if process exists
  PROCESS_CONTROL_BLOCK* process = GetPCB(PID);
  if(process == NULL){
    aprintf("Cannot suspend the process of a PID that DNE!\n\n");
    (*return_error) = 1;
    return;
  }

  INT32 process_state;
 
  if(process_state == SUSPENDED){
    aprintf("Process is already suspended!\n\n");
    (*return_error) = 1;
    return;
  }
  else if(process_state == TIMER){
    aprintf("Process on Timer Queue! Suspend when done\n\n!");
    ChangeProcessState(PID, WAITING_TO_SUSPEND_TIMER);
  }
  else if(process_state == DISK){
    aprintf("Process on Disk Queue! Suspend when done\n\n!");
    ChangeProcessState(PID, WAITING_TO_SUSPEND_DISK);
  }
  else if(process_state == WAITING_TO_SUSPEND_TIMER ||
	  process_state == WAITING_TO_SUSPEND_DISK){
    aprintf("Process is already waiting to be suspended\n\n");
  }
  else{
    aprintf("Suspending process %ld\n\n", PID);
    ChangeProcessState(PID, SUSPENDED);
    if(RemoveFromReadyQueue(process) != -1){
      (*return_error) = 0;
      aprintf("Removing from Ready Q\n");
      QPrint(ready_queue_id);
    }
    else{
      (*return_error) = 1;
    }
    return;

  }    
}




void osResumeProcess(long PID, long* return_error){
 
  
  //check to see if process exists
  PROCESS_CONTROL_BLOCK* process = GetPCB(PID);
  if(process == NULL){
    aprintf("Cannot resume the process of a PID that DNE!\n\n");
    (*return_error) = 1;
    return;
  }

   INT32 process_state;
  GetProcessState(PID, &process_state);
  

  if(process_state == RUNNING || process_state == READY ||
     process_state == TIMER || process_state == DISK){
    
    aprintf("Process Does not need to be resumed\n\n");
    (*return_error) = 1;
    return;
  }
  else if(process_state == WAITING_TO_SUSPEND_TIMER){
    aprintf("Returning state to Timer\n\n");
    ChangeProcessState(PID, TIMER);
  }
  else if(process_state == WAITING_TO_SUSPEND_DISK){
    aprintf("Returning state to disk\n\n");
    ChangeProcessState(PID, DISK);
  }
  else{
    aprintf("Resuming process %ld\n\n", PID);
    ChangeProcessState(PID, READY);
    AddToReadyQueue(process->context, PID, process);
    QPrint(ready_queue_id);
    (*return_error) = 0;
    return;
  }
   
}

/*
Simply checks for any active processes. This includes Running, Suspended, Waiting for Disk/Timer etc. 
If there are none return FALSE.
If there is a process return TRUE.
This is useful for knowing whether to end simulation in a Terminate Process Call
*/
INT32 CheckActiveProcess(){

  //Try to find an active process
  for(int i=0; i<MAXPROCESSES; i++){
    if(PRO_INFO->PCB[i].in_use == 1){
      return TRUE;
    }
  }
  return FALSE;
}


/*seems to be close to duplicate of CheckActiveProcesses()
used in CreateProcess()
*/
int CheckProcessCount(){

  for(int i=0; i<MAXPROCESSES; i++){
    if(PRO_INFO->PCB[i].in_use == 0){
      return 1;
    }
  }
  return 0;
}


//Not sure how to handle children of process
//returning errors
void TerminateProcess(long process_id){

  int i = 0;
  //find process with proper pid
  for(i=0; i<MAXPROCESSES; i++){
    if(PRO_INFO->PCB[i].idnum == process_id){
      break;
    }
  }
  if(i == MAXPROCESSES){
    printf("Process not found!!!\n\n\n");
    return;
  }
  PRO_INFO->PCB[i].idnum = -1;
  PRO_INFO->PCB[i].in_use = 0;
  PRO_INFO->PCB[i].priority = 0;
  strcpy(PRO_INFO->PCB[i].name, "");

  

  return;
}

/*
Terminate the children of process with process_id
*/
void TerminateChildren(long process_id){

  for(int i=0; i<MAXPROCESSES; i++){

    if(PRO_INFO->PCB[i].in_use != 0 &&
       PRO_INFO->PCB[i].parent == process_id){

      TerminateProcess(PRO_INFO->PCB[i].idnum);
    }
  }
}



void CreateProcess(char* name, long start_address, long Priority, INT32 parent, long* process_id, long* return_error){

  long context;
  long PID;
  MEMORY_MAPPED_IO mmio;

  // Every process will have a page table.  This will be used in
  // the second half of the project.  
  void *PageTable = (void *) calloc(2, NUMBER_VIRTUAL_PAGES);

  if(CheckProcessCount() == 0){
    aprintf("\n\nReached Max number of Processes!\n\n");
    *return_error = 1;  //set error message
    return;
  } 

  //check for proper priority. Not 100% sure what improper priority is
  if(Priority <= 0){
    aprintf("\n\nImproper Priority\n\n");
    *return_error = 1;  //set error message
    return;
  }
  if(CheckProcessName(name) == 0){
    aprintf("\n\nDuplicate Name\n\n");
    *return_error = 1;  //set error message
    return;
  }

  //create a context for process
  mmio.Mode = Z502InitializeContext;
  mmio.Field1 = 0;
  mmio.Field2 = start_address;
  mmio.Field3 = (long) PageTable;

  MEM_WRITE(Z502Context, &mmio);   // Start of Make Context Sequence

  context = mmio.Field1;//Field1 returns the context from initialize context
  PID = osCreateProcess(name, context, parent, Priority);
  
  
  *process_id = PID;  //set error message  

  //set error field to 0 to indicate no error in process creation
  *return_error = 0;  //set error message

  ChangeProcessState(PID, READY);
  AddToReadyQueue(context, PID, GetPCB(PID));
  // ProcessesState();
}


/*
This function receives the name of new process and checks the names of all existing processes. It returns 1 if the new name is unique and returns 0 if the new name is not unique
*/
int CheckProcessName(char* name){

  PROCESS_CONTROL_BLOCK* pi;
  
  for(int i=0; i<MAXPROCESSES; i++){
    if(&PRO_INFO->PCB[i] != NULL){
      pi = &PRO_INFO->PCB[i];
      if(strcmp(pi->name, name) == 0){
	return 0;
      }
    }
  }
  return 1;
}



/*
Changes the state of the process given 
*/
void ChangeProcessState(long PID, INT32 new_state){

  PROCESS_CONTROL_BLOCK* PCB =  GetPCB(PID);

  if(PCB == NULL){
    aprintf("Could not retrieve PCB for PID %ld\n", PID);
  }

  INT32 LOCK = PCB->LOCK;

  LockLocation(LOCK);

  PCB->state = new_state;

  UnlockLocation(LOCK);
}  

/*
Get the state of the process given 
*/
void GetProcessState(long PID, INT32* state){

  PROCESS_CONTROL_BLOCK* PCB =  GetPCB(PID);

  if(PCB == NULL){
    aprintf("Could not retrieve PCB for PID %ld\n", PID);
  }

  INT32 LOCK = PCB->LOCK;

  LockLocation(LOCK);

  (*state) = PCB->state;
    
  UnlockLocation(LOCK); 
}  

void osChangePriority(long PID, long NewPriority, long* ReturnError){

  PROCESS_CONTROL_BLOCK* process;
  
  //Change Priority of currently running process
  if(PID == -1){
    process = GetCurrentPCB();
    process->priority = NewPriority;
    (*ReturnError) = 0;
    return;
  }

   //check to see if process exists
  process = GetPCB(PID);
  if(process == NULL){
    aprintf("Cannot change the priority of a process that DNE!\n\n");
    (*ReturnError) = 1;
    return;
  }

  //find the state of the process
  INT32 ProcessState;
  GetProcessState(PID, &ProcessState);
  if(ProcessState == READY){
    aprintf("Process is on the ready queue!\n\n");
    ChangePriorityInReadyQueue(process, NewPriority);
    (*ReturnError) = 1;
    return;
  }
  else{
    process->priority = NewPriority;
    (*ReturnError) = 1;
  }
}

void osTerminateProcess(long TargetPID, long *ReturnError){

  MEMORY_MAPPED_IO mmio;
  long PID = GetCurrentPID();

  if(TargetPID == -1 || TargetPID == -2){
    if(TargetPID == -1){
      aprintf("Terminate Current process!\n");

      TerminateProcess(PID);

    }
    else{ //TargetPID == -2)
      aprintf("Terminate current process and all children\n\n");
      TerminateChildren(PID);
      TerminateProcess(PID);
    }

    if(CheckActiveProcess() == FALSE){
      mmio.Mode = Z502Action;
      mmio.Field1 = mmio.Field2 = mmio.Field3 = 0;
      MEM_WRITE(Z502Halt, &mmio);
    }
    else{
      dispatcher();
    }
  }
  else{
    aprintf("Terminate Process %ld. This is not the current process!\n\n",TargetPID);
    TerminateProcess(TargetPID);
  }  
 
  (*ReturnError) = ERR_SUCCESS;  //set error message
}
