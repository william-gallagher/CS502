//for use with SystemCallData struct
#include "syscalls.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "timerQueue.h"
#include "process.h"
#include "protos.h"



//Test function that prints the states of all the processes
void ProcessesState(){
  for(int i=0; i<MAXPROCESSES; i++){
    if(PRO_INFO->PCB[i].in_use == 0){
      printf("PRO_INFO slot %d is not in use\n", i);
    }
    else{
      printf("\tPRO_INFO slot %d is in use\n\t%ld is the id\n\t%s is the name\n\twith parent %d\n\n", i, PRO_INFO->PCB[i].idnum, PRO_INFO->PCB[i].name,PRO_INFO->PCB[i].parent);
    }
  }
}

//need to remember to free at end somehow
void CreatePRO_INFO(){
  PRO_INFO = malloc(sizeof(PROCESSES_INFORMATION));
  for(int i=0; i<MAXPROCESSES; i++){
    PRO_INFO->PCB[i].in_use = 0;
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
  new_pcb->suspended = 0;
  
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

/*This function accepts a pointer to a Process Control Block and an PID. It sets the pointer to the PCB of the PID.
 */
PROCESS_CONTROL_BLOCK* GetPCB(long PID){

  PROCESS_CONTROL_BLOCK* process;
  
  for(int i=0; i<MAXPROCESSES; i++){
    if(PRO_INFO->PCB[i].in_use != 0){
      printf("%d is in use \n\n", i);
      if(PRO_INFO->PCB[i].idnum == PID){
	printf("here at %d\n", i);
	process = &PRO_INFO->PCB[i];
	return process;
      }
    }
  }
  process = NULL;
  return process;

}

void osSuspendProcess(long PID, long* return_error){

  //Check to see if process is the current process
  if(GetCurrentPID() == PID){
    aprintf("Cannot suspend current process!\n\n");
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
  else{
    if(process->suspended == 1){
      aprintf("Process is already suspended!\n\n");
      (*return_error) = 1;
      return;
    }
    else{
      aprintf("Suspending process %ld\n\n", PID);
      process->suspended = 1;
      (*return_error) = 0;
      return;
    }
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
  else{
    if(process->suspended == 0){
      aprintf("Process is already running!\n\n");
      (*return_error) = 1;
      return;
    }
    else{
      aprintf("Resuming process %ld\n\n", PID);
      process->suspended = 0;
      (*return_error) = 0;
      return;
    }
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
  long idnum;
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
  idnum = osCreateProcess(name, context, parent);
  
  
  *process_id = idnum;  //set error message  
  printf("Here is the name of the process %s\n\n", name);

  //set error field to 0 to indicate no error in process creation
  *return_error = 0;  //set error message

  //add new process to ready queue
  // RQ_ELEMENT* rqe = malloc(sizeof(RQ_ELEMENT));
  //rqe->context =  context; 
  //QInsertOnTail(ready_queue_id, (void *) rqe);
  AddToReadyQueue(context);
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
