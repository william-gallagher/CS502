//Include file for everything to do with processes

//for use with SystemCallData struct
#include "syscalls.h"
#include "string.h"

//Should there be a max number of processes????
#define MAXPROCESSES 100



//name has limit of 100 for now. Seems like a hack
typedef struct{
  INT32 idnum;
  long context;
  char name[100];
} PROCESS_INFO;

typedef struct{
  INT32 nextid;
  INT32 current;
  PROCESS_INFO processes[MAXPROCESSES];
} PROCESS_CONTROL_BLOCK;

//make this outside a function for now to allow everyone to use
PROCESS_CONTROL_BLOCK* PCB;

//need to remember to free at end somehow
PROCESS_CONTROL_BLOCK* CreatePCB(){
  PCB = malloc(sizeof(PROCESS_CONTROL_BLOCK));

  PCB->nextid = 1;
  return PCB;
}
  
void osCreateProcess(char* name, long context){
  
  PROCESS_INFO* new_process = &PCB->processes[PCB->nextid];
  new_process->idnum = PCB->nextid;
  strcpy(new_process->name, name);
  new_process->context = context;
  PCB->current = new_process->idnum;
}

void getProcessID(SYSTEM_CALL_DATA* scd){

  char process_name[100];
  strcpy(process_name, (char*)scd->Argument[0]);
  printf("%s is the process name\n\n", process_name);
  printf("%d is the current process\n", PCB->current);

  //Requesting current process
  if(strcmp(process_name, "") == 0){
    *(long *)scd->Argument[1] = PCB->current;
    *(long *)scd->Argument[2] = 0;//no error
  }
  else{
    printf("Requested something other than current process\nNot implemented yet!\n\n");
  }

}
