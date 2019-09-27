//for use with SystemCallData struct
#include "syscalls.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "timerQueue.h"
#include "process.h"
#include "protos.h"
#include "os_globals.h"


 
//Test function that prints the states of all the processes
void ProcessesState(){
  for(int i=0; i<MAXPROCESSES; i++){
    if(PRO_INFO->PCB[i].in_use == 0){
      // printf("PRO_INFO slot %d is not in use\n", i);
    }
    else{
      // printf("\tPRO_INFO slot %d is in use\n\t%ld is the id\n\t%s is the name\n\twith parent %d\n\n", i, PRO_INFO->PCB[i].idnum, PRO_INFO->PCB[i].name,PRO_INFO->PCB[i].parent);
    }
  }
}

//need to remember to free at end somehow
//Also created a lock for each process
void CreatePRO_INFO(){
  PRO_INFO = malloc(sizeof(PROCESSES_INFORMATION));
  for(int i=0; i<MAXPROCESSES; i++){
    PRO_INFO->PCB[i].in_use = 0;
    PRO_INFO->PCB[i].LOCK = (PROC_LOCK_BASE + i);
    //aprintf("%x is lock for process %d\n", PRO_INFO->PCB[i].LOCK, i);
  }
  
  PRO_INFO->nextid = 0;
}

long osCreateProcess(char* name, long context, INT32 parent){
  
  PROCESS_CONTROL_BLOCK* new_pcb = &PRO_INFO->PCB[PRO_INFO->nextid];
  new_pcb->idnum = PRO_INFO->nextid;
  PRO_INFO->nextid++; //eventually need a more sophisticated method for reusing pid's
  strcpy(new_pcb->name, name);
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


//change the parameter passed here 
void getProcessID(SYSTEM_CALL_DATA* scd){

  char process_name[100];
  strcpy(process_name, (char*)scd->Argument[0]);

  //Requesting current process
  if(strcmp(process_name, "") == 0){
    *(long *)scd->Argument[1] = GetCurrentPID();
    *(long *)scd->Argument[2] = 0;//no error
  }
  else{
    for(int i=0; i<MAXPROCESSES; i++){
      if(PRO_INFO->PCB[i].in_use != 0){
	if(strcmp(process_name, PRO_INFO->PCB[i].name) == 0){
	  *(long *)scd->Argument[1] = PRO_INFO->PCB[i].idnum;
	  *(long *)scd->Argument[2] = 0;//no error
	  return;
	}
      }
    }
    *(long *)scd->Argument[2] = 1;//return error
    // printf("Requested something other than current process\nNot implemented yet!\n\n");
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
	return (void*) process;
      }
    }
  }
  process = NULL;
  aprintf("Could not find the proper PCB from that context\n");
  return (void*) process;
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
  GetProcessState(PID, &process_state);
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
  /*
  //Check to see if process is the current process
  if(GetCurrentPID() == PID){
    aprintf("Cannot suspend current process!\n\n");
    (*return_error) = 1;
    return;
  }
  */
  
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
      ProcessesState();
    }
  }
}

int CheckProcessCount(){

  for(int i=0; i<MAXPROCESSES; i++){
    if(PRO_INFO->PCB[i].in_use == 0){
      return 1;
    }
  }
  return 0;
}

void CreateProcess(char* name, long start_address, long priority, INT32 parent, long* process_id, long* return_error){

  long context;
  long PID;
  MEMORY_MAPPED_IO mmio;

  // Every process will have a page table.  This will be used in
  // the second half of the project.  
  void *PageTable = (void *) calloc(2, NUMBER_VIRTUAL_PAGES);

  if(CheckProcessCount() == 0){
    printf("\n\nReached Max number of Processes!\n\n");
    *return_error = 1;  //set error message
    return;
  } 

  //check for proper priority. Not 100% sure what improper priority is
  if(priority <= 0){
    printf("\n\nImproper Priority\n\n");
    *return_error = 1;  //set error message
    return;
  }
  if(CheckProcessName(name) == 0){
    printf("\n\nDuplicate Name\n\n");
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
  printf("\n\nhere is the new context %lx\n\n", context);
  PID = osCreateProcess(name, context, parent);
  
  
  *process_id = PID;  //set error message  
  printf("Here is the name of the process %s\n\n", name);

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
  INT32 Suspend_Until_Locked = TRUE;
  INT32 Sucess_Failure = -1;

  READ_MODIFY(LOCK, 1, Suspend_Until_Locked, &Sucess_Failure);
  if(Sucess_Failure == FALSE){
    aprintf("\n\nCould Not Obtain the Lock to change process state\n\n");
  }

  PCB->state = new_state;

    
  READ_MODIFY(LOCK, 0, Suspend_Until_Locked, &Sucess_Failure);
  if(Sucess_Failure == FALSE){
    aprintf("\n\nCould not give up the lock to change process state\n\n");
  }
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
  INT32 Suspend_Until_Locked = TRUE;
  INT32 Sucess_Failure = -1;

  READ_MODIFY(LOCK, 1, Suspend_Until_Locked, &Sucess_Failure);
  if(Sucess_Failure == FALSE){
    aprintf("\n\nCould Not Obtain the Lock for get process state\n\n");
  }

  (*state) = PCB->state;
    
  READ_MODIFY(LOCK, 0, Suspend_Until_Locked, &Sucess_Failure);
  if(Sucess_Failure == FALSE){
    aprintf("\n\nCould not give up the lock for get process state\n\n");
  }
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

