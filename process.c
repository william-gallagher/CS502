/*
  process.c

  This file holds the functions that deal with the creation, suspension, resumption, termination and general management of processes for the OS.

*/

#include <string.h>
#include <stdlib.h>
#include "timerQueue.h"
#include "readyQueue.h"
#include "process.h"
#include "protos.h"
#include "osGlobals.h"
#include "osSchedulePrinter.h"

/*
  This function is called in OsInit. It sets the use flag to FREE and creates a LOCK that is associated with each PCB
*/
void InitializeProcessInfo(){
 
    for(int i=0; i<MAXPROCESSES; i++){
	PCB[i].in_use = FREE;
	PCB[i].LOCK = (PROC_LOCK_BASE + i);
    }

    nextid = 0;
}

/* 
   This function looks through the PCBs to find the first available PCBs.
   The index of the first PCB slot is returned. Otherwise -1 is returned 
   indicating there are no more PCBs available and the OS has reached its 
   limit for process creation.
*/
INT32 GetAvailablePCB(){
  
    for(int i=0; i<MAXPROCESSES; i++){
	if(PCB[i].in_use != IN_USE){
	    return i;
	}
    }
    return -1;  //Indicates no available PCB
}

/*
  This function does the work of finding the PCB for the new process. It
  fills the fields of the PCB and returns the PID of the new process.
*/
long FillPCB(char* Name, long Context, INT32 Parent, long Priority, void* PageTable){


    //First get an available PCB.
    INT32 next_slot = GetAvailablePCB();
    PROCESS_CONTROL_BLOCK* new_pcb = &PCB[next_slot];

    //Set PID to the next available ID. PIDs start at 0 and count up
    new_pcb->idnum = nextid;
    nextid++;
  
    strcpy(new_pcb->name, Name);
    new_pcb->priority = Priority;
    new_pcb->context = Context;
    new_pcb->in_use = IN_USE;
    new_pcb->parent = Parent;
    new_pcb->state = RUNNING;
    new_pcb->page_table = PageTable;
  
    return new_pcb->idnum;
}

/*
  Get the PID of the currently running process. This is accomplished by 
  getting the context of the process through Memory Mapped IO Function 
  Z50GetCurrentContext. This context is compared to the list of existing
  processes to determine which is running.
*/
long GetCurrentPID(){

    MEMORY_MAPPED_IO mmio;
    mmio.Mode = Z502GetCurrentContext;
    mmio.Field1 = mmio.Field2 = mmio.Field3 = mmio.Field4 = 0;
    MEM_READ(Z502Context, &mmio);

    if(mmio.Field4 != ERR_SUCCESS){
	aprintf("\n\nERROR: Failed to Get Current Context\n\n");
    }

    long context = mmio.Field1;

    for(int i=0; i<MAXPROCESSES; i++){
	if(PCB[i].in_use != FREE){
	    if(PCB[i].context == context){
		return PCB[i].idnum;
	    }
	}
    }
    return -1; 
}

/*
  This function accepts a string: ProcessName and searches through the 
  the active PCBs to find the PID that corresponds to the ProcessName.
  The PID is passed by reference back through PID and the result of the
  search is passed in ReturnError
*/
void GetProcessID(char ProcessName[], long *PID, long *ReturnError){

    //The string "" means that the current PID is requested
    if(strcmp(ProcessName, "") == 0){
	(*PID) = GetCurrentPID();
	(*ReturnError) = ERR_SUCCESS;
    }
    else{ //looking for a PID other than the current PID
	for(int i=0; i<MAXPROCESSES; i++){
	    if(PCB[i].in_use != FREE){
		if(strcmp(ProcessName, PCB[i].name) == 0){
		    (*PID) = PCB[i].idnum;
		    (*ReturnError) = ERR_SUCCESS;
		    return;
		}
	    }
	}
	//If we get this far then the process name does not exist.
	//Return an Error
	(*ReturnError) = ERR_BAD_PARAM;
    }
}


/*This function accepts a context. It searches throught the PCBs to find
  the process that has the given context. If the contest corresponds to a
  process a pointer to the PCB is returned. Otherwise a NULL pointer is 
  returned
*/
void* GetPCBContext(long context){

    PROCESS_CONTROL_BLOCK* process;
  
    for(int i=0; i<MAXPROCESSES; i++){
	if(PCB[i].in_use != FREE){

	    if(PCB[i].context == context){
		process = &PCB[i];
		return (void *) process; //sucessfully found the process
	    }
	}
    }
    process = NULL;
    aprintf("\n\nError:Could not find PCB from that context\n\n");
    return (void *) process; //return NULL pointer
}

/*
  This function accepts a PID. It returns the pointer to the proper PCB
  associated with the PID if it exists. Otherwise it returns a NULL
  pointer.
*/
void* GetPCB(long PID){

    PROCESS_CONTROL_BLOCK* process;

    //step through PCBs to find a PCB with given PID
    for(int i=0; i<MAXPROCESSES; i++){
	if(PCB[i].in_use != FREE){
	    if(PCB[i].idnum == PID){
		process = &PCB[i];
		return (void *) process;
	    }
	}
    }
    process = NULL;
    return process;
}

/*
  This function returns a pointer to the PCB of the currently running 
  process.
*/
void* GetCurrentPCB(){

    long PID = GetCurrentPID();
    void* current_PCB = GetPCB(PID);

    return current_PCB;
}

/*
  This funtion returns the context of the currently executing process by
  using the z502GetCurrentContext call. 
*/
long osGetCurrentContext(){
  
    MEMORY_MAPPED_IO mmio;
    mmio.Mode = Z502GetCurrentContext;
    mmio.Field1 = mmio.Field2 = mmio.Field3 = mmio.Field4 = 0;
    MEM_READ(Z502Context, &mmio);

    if(mmio.Field4 != ERR_SUCCESS){
	aprintf("\n\nError: Failed to Get Current Context\n\n");
    }

    long context = mmio.Field1;
    return context;
}

/*
  This function handles the Suspend Process System call. It suspends the 
  process given by PID. 
  Note that it is illegal to suspend the currently running process!!!!
  This is a design decision for this OS.
  Sucess or Failure is noted by  ReturnError.
*/
void osSuspendProcess(long PID, long* ReturnError){

    //Check to see if process is the current process
    if(GetCurrentPID() == PID){
	aprintf("\n\nError: Cannot suspend the current process!\n\n");
	(*ReturnError) = ERR_BAD_PARAM;
	return;
    }

    //Check to see if process exists. Return Failure if process DNE
    PROCESS_CONTROL_BLOCK* process = GetPCB(PID);
    if(process == NULL){
	//aprintf("\n\nError: Cannot suspend the process of a PID that DNE!\n\n");
	(*ReturnError) = 1;
	return;
    }

    //We need to know the state of the process to determine how to
    //proceed with suspend
    INT32 process_state;
    GetProcessState(PID, &process_state);
 
    if(process_state == SUSPENDED){
	//aprintf("Process is already suspended!\n\n");
	(*ReturnError) = ERR_BAD_PARAM;
	return;
    }
  
    //If the process is on the TIMER Queue we change the state to
    //WAITING_TO_SUSPEND_TIMER. This indicates that process will go
    //to the SUSPENDED state after it comes off the TIMER Queue. 
    else if(process_state == TIMER){
	//aprintf("Process on Timer Queue! Suspend when done\n\n!");
	ChangeProcessState(PID, WAITING_TO_SUSPEND_TIMER);
    }
  
    //If the process is on the DISK Queue we change the state to
    //WAITING_TO_SUSPEND_DISK. This indicates that process will go
    //to the SUSPENDED state after it comes off the DISK Queue. 
    else if(process_state == DISK){
	//aprintf("Process on Disk Queue! Suspend when done\n\n!");
	ChangeProcessState(PID, WAITING_TO_SUSPEND_DISK);
    }
  
    //Process is already waiting to be suspended when it finishes on the
    //TIMER or DISK Queues. There is no need for further action.
    else if(process_state == WAITING_TO_SUSPEND_TIMER ||
	    process_state == WAITING_TO_SUSPEND_DISK){
	//aprintf("Process is already waiting to be suspended\n\n");
    }
    else{

	ChangeProcessState(PID, SUSPENDED);

	//Pull PCB from READY Queue
	if(RemoveFromReadyQueue(process) != -1){
	    (*ReturnError) = ERR_SUCCESS;
	    osPrintState("Suspend", PID, GetCurrentPID());
	}
	else{
	    (*ReturnError) = ERR_BAD_PARAM;
	}
	return;
    }    
}

/*
  This function handles the Resume Process System call. It resumes the 
  process given by PID. 
  Sucess or Failure is noted by  ReturnError.
*/
void osResumeProcess(long PID, long *ReturnError){
  
    //check to see if process exists. Cannot resume a PCB that DNE.
    PROCESS_CONTROL_BLOCK* process = GetPCB(PID);
    if(process == NULL){
	//aprintf("\n\nError:Cannot resume the process of a PID that DNE!\n\n");
	(*ReturnError) = ERR_BAD_PARAM;
	return;
    }

    //Having established that PID is a valid PID we need the state of the
    //Process.
    INT32 process_state;
    GetProcessState(PID, &process_state);

    //No need for action
    if(process_state == RUNNING || process_state == READY ||
       process_state == TIMER || process_state == DISK){
    
	//aprintf("\n\nError: Process Does not need to be resumed\n\n");
	(*ReturnError) = ERR_BAD_PARAM;
	return;
    }
    //The process is in the Timer Queue but has been suspended previously.
    //Now change its state back to TIMER. It is still in the TIMER Queue!
    //There is no need to reinsert in the TIMER Queue!
    else if(process_state == WAITING_TO_SUSPEND_TIMER){

	ChangeProcessState(PID, TIMER);
    }
    //The process is in the Disk Queue but has been suspended previously.
    //Now change its state back to DISK. It is still in the DISK Queue!
    //There is no need to reinsert in the DISK Queue!
    else if(process_state == WAITING_TO_SUSPEND_DISK){

	ChangeProcessState(PID, DISK);
    }
    else{

	//Change state from SUSPEND to READY. Put in READY Queue.
	ChangeProcessState(PID, READY);
	AddToReadyQueue(process->context, PID, process, FALSE);
	(*ReturnError) = ERR_SUCCESS;
    }
    osPrintState("Resume", PID, GetCurrentPID());
   
}

/*
  This function checks for any active processes. This includes Running,
  Suspended, Waiting for Disk/Timer etc. 
  If there are none it returns FALSE.
  If there is a process that is ACTIVE return TRUE.
  This is useful for knowing whether to end simulation in a Terminate
  Process Call
*/
INT32 CheckActiveProcess(){

    //Try to find an active process
    for(int i=0; i<MAXPROCESSES; i++){
	if(PCB[i].in_use == IN_USE){
	    return TRUE;
	}
    }
    return FALSE;
}


/*
  This function checks to see if the OS has reached the MAX_PROCESSES 
  limit.
  If there is still space for another process return TRUE.
  If the OS has reached it max for processes return FALSE.
*/
int CheckProcessCount(){

    for(int i=0; i<MAXPROCESSES; i++){
	if(PCB[i].in_use == FREE){
	    return TRUE;
	}
    }
    return FALSE;
}

/*
  This function deletes the contents of the PCB given by the PID.
*/
void DeletePCB(long PID){

    PROCESS_CONTROL_BLOCK* pcb = GetPCB(PID);
    
    if(pcb == NULL){
	printf("\n\nError: Process not found!\n\n");
	return;
    }

    //Cleanup PCB for reuse
    pcb->idnum = -1;
    pcb->in_use = FREE;
    pcb->priority = 0;
    strcpy(pcb->name, "");
}

/*
  In this function any children of ParentPID are deleted
*/
void TerminateChildren(long ParentPID){

    for(int i=0; i<MAXPROCESSES; i++){

	if(PCB[i].in_use != 0 &&
	   PCB[i].parent == ParentPID){

	    DeletePCB(PCB[i].idnum);
	}
    }
}

/*
  Creates a new context with the given StartAddress and PageTable.
  The newly created Context is returned.
*/
long GetNewContext(long StartAddress, void *PageTable){

    long Context;
    MEMORY_MAPPED_IO mmio;
    mmio.Mode = Z502InitializeContext;
    mmio.Field1 = 0;
    mmio.Field2 = StartAddress;
    mmio.Field3 = (long) PageTable;

    MEM_WRITE(Z502Context, &mmio); 

    Context = mmio.Field1;//Field1 returns the new context

    return Context;
}

/*
  This function does the heavy lifting for creating a process. 
  It checks to make sure that there is an available PCB
  It checks to make sure the priority of the new process is valid
  It checks to make sure the name of the new process is not a duplicate

  The result of the creation of the process is passed in the field
  ReturnError.
*/
void osCreateProcess(char Name[], long StartAddress, long Priority, long *PID, long *ReturnError){

    long context;

    // Every process will have a page table.  This will be used in
    // the second half of the project.  
    void *PageTable = (void *) calloc(2, NUMBER_VIRTUAL_PAGES);

    if(CheckProcessCount() == FALSE){
	//aprintf("\n\nError: Reached Max number of Processes\n\n");
	(*ReturnError) = ERR_BAD_PARAM;  //set error message
	return;
    } 

    //Check for proper priority. Anything less than zero is an improper
    //priority
    if(Priority <= 0){
	//aprintf("\n\nError: Improper Priority\n\n");
	(*ReturnError) = ERR_BAD_PARAM;  //set error message
	return;
    }
    //Look for any duplicate names
    if(CheckProcessName(Name) == 0){
	//aprintf("\n\nError: Duplicate Name\n\n");
	(*ReturnError) = ERR_BAD_PARAM;  //set error message
	return;
    }

    //Ask Hardware for new Context
    context = GetNewContext(StartAddress, PageTable);

    long CurrentPID = GetCurrentPID();
    (*PID) = FillPCB(Name, context, CurrentPID, Priority, PageTable);

    osPrintState("Create", *PID, CurrentPID);
  
    //set error field to 0 to indicate no error in process creation
    (*ReturnError) = ERR_SUCCESS;  //set error message

    ChangeProcessState(*PID, READY);
    AddToReadyQueue(context, *PID, GetPCB(*PID), TRUE);
}


/*
  This function receives the name of new process and checks the names of all existing processes. It returns 1 if the new name is unique and returns 0 if the new name is not unique
*/
int CheckProcessName(char* name){

    PROCESS_CONTROL_BLOCK* pi;
  
    for(int i=0; i<MAXPROCESSES; i++){
	if(&PCB[i] != NULL){
	    pi = &PCB[i];
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

    PROCESS_CONTROL_BLOCK* pcb =  GetPCB(PID);

    if(pcb == NULL){
	aprintf("Could not retrieve PCB for PID %ld\n", PID);
    }

    INT32 LOCK = pcb->LOCK;

    LockLocation(LOCK);
    (*state) = pcb->state;
    UnlockLocation(LOCK); 
}  

/*
  This function handles priority changes. First it does some error checking
  to ensure the process exists.
  The process with that will see the priority change is given by PID
  The new priority is given in NewPriority
  A PID of -1 indicates change the priority of the current process.

  If the process that we wish to change the priority of is in the READY 
  Queue it must be repositioned. If the process is in the DISK or TIMER
  Queues simply changing the priority in the pcb is sufficient.
*/
void osChangePriority(long PID, long NewPriority, long *ReturnError){

    PROCESS_CONTROL_BLOCK* pcb;
  
    //Change Priority of currently running process
    if(PID == -1){
	pcb = GetCurrentPCB();
	pcb->priority = NewPriority;
	(*ReturnError) = ERR_SUCCESS;
	osPrintState("Chg Pri", pcb->idnum, pcb->idnum);
	return;
    }

    //check to see if process exists
    pcb = GetPCB(PID);
    if(pcb == NULL){
	aprintf("\n\nError: Cannot change the priority of a process that DNE!\n\n");
	(*ReturnError) = ERR_BAD_PARAM;
	return;
    }

    //find the state of the process
    INT32 ProcessState;
    GetProcessState(PID, &ProcessState);
    if(ProcessState == READY){

	ChangePriorityInReadyQueue(pcb, NewPriority);
    }
    else{
	pcb->priority = NewPriority;
    }
    (*ReturnError) = ERR_SUCCESS;
    osPrintState("Chg pri", pcb->idnum, GetCurrentPID());
}

/*
  This function handles the termination of a process given by the TargetPID
  A TargetPID of -1 indicates that the current PID is to be terminated.
  A TargetPID of -2 indicates that the current PID and all its children
  are to be terminated.
  If, after terminating the given process, there are no more active 
  processes the simulation is terminated.
*/
void osTerminateProcess(long TargetPID, long *ReturnError){

    MEMORY_MAPPED_IO mmio;
    long PID = GetCurrentPID();

    if(TargetPID == -1 || TargetPID == -2){ //Delete the current process
	if(TargetPID == -1){

	    DeletePCB(PID);
	}
	else{ //TargetPID == -2. Delete current process and all children
	    TerminateChildren(PID);
	    DeletePCB(PID);
	}

	//If there are no more active processes end the simulation
	if(CheckActiveProcess() == FALSE){
	    mmio.Mode = Z502Action;
	    mmio.Field1 = mmio.Field2 = mmio.Field3 = 0;
	    MEM_WRITE(Z502Halt, &mmio);
	}
	else{   //If there is another active process continue simulation

	    if(TargetPID == -1){
		TargetPID = PID;
	    }
	    
	    osPrintState("Term", TargetPID, PID);
	    dispatcher();
	}
    }
    else{ //Delete a process that is not the current process
	//First have to remove READY Queue if in the READY state
	INT32 ProcessState;
	GetProcessState(TargetPID, &ProcessState);
	if(ProcessState == READY){
	    RemoveFromReadyQueue(GetPCB(TargetPID));
	}
	DeletePCB(TargetPID);
    }  
 
    (*ReturnError) = ERR_SUCCESS;  //set error message
    osPrintState("Term", TargetPID, PID);
}
