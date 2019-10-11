/*
This file holds all the support functions for the State Printer
*/
#include "syscalls.h"
#include "os_globals.h"
#include "global.h"
#include "protos.h"
#include "process.h"
#include "timerQueue.h"
#include <string.h>
#include "os_Schedule_Printer.h"

/*
Fill the Ready Queue parts of the schedule printer
*/
void FillReady(SP_INPUT_DATA* SPInput){

  INT16 ReadyCount = 0;
  INT32 Index = 0;
  RQ_ELEMENT* rqe;

  LockLocation(READY_LOCK);

  rqe = QWalk(ready_queue_id, Index);
  
  while((long)rqe != -1){
    SPInput->ReadyProcessPIDs[Index] = (INT16)(rqe->PID);
 
    Index++;
    ReadyCount++;
    rqe = QWalk(ready_queue_id, Index);
  }
  UnlockLocation(READY_LOCK);

  SPInput->NumberOfReadyProcesses = ReadyCount;
}

void FillTimer(SP_INPUT_DATA* SPInput){

  INT16 TimerCount = 0;
  int Index = 0;
  TQ_ELEMENT* tqe;

  LockLocation(TIMER_LOCK);
  
  tqe = QWalk(timer_queue_id, Index);
  
  while((long)tqe != -1){
    SPInput->TimerSuspendedProcessPIDs[Index] = (INT16)(tqe->PID);
    Index++;
    TimerCount++;
    tqe = QWalk(timer_queue_id, Index);
  }

  UnlockLocation(TIMER_LOCK);

  SPInput->NumberOfTimerSuspendedProcesses = TimerCount;
}

void FillDisk(SP_INPUT_DATA* SPInput, int DiskID, INT16* DiskCount){

  int Index = 0;
  DQ_ELEMENT* dqe;
    

  LockLocation(DISK_LOCK[DiskID]);

  dqe = QWalk(disk_queue[DiskID], Index);
  
  while((long)dqe != -1){
    SPInput->DiskSuspendedProcessPIDs[Index] = (INT16)(dqe->PID);
    Index++;
    (*DiskCount)++;
    dqe = QWalk(disk_queue[DiskID], Index);
  }

  UnlockLocation(DISK_LOCK[DiskID]);
}

void FillAllDisk(SP_INPUT_DATA* SPInput){

  INT16 DiskCount = 0;

  //check each disk queue in turn
  for(int i=0; i<MAX_NUMBER_OF_DISKS; i++){

    FillDisk(SPInput, i, &DiskCount);
  }

  SPInput->NumberOfDiskSuspendedProcesses = DiskCount;
}

/*
Add the Suspended Process PIDs to the State Printer Struct
*/
void FillProcessSuspended(SP_INPUT_DATA* SPInput){

  INT16 SuspendedCount = 0;
  PROCESS_CONTROL_BLOCK* pcb;
  int Index = 0;

  for(int i=0; i<MAXPROCESSES; i++){
    pcb = &PRO_INFO->PCB[i];
    if(pcb->in_use == 1){
      if(pcb->state == SUSPENDED){
	SuspendedCount++;
	SPInput->ProcSuspendedProcessPIDs[Index] = (INT16)(pcb->idnum);

	Index++;
      }
    }
  }
  SPInput->NumberOfProcSuspendedProcesses = SuspendedCount;
}

/*
Add the Message Suspended Process PIDs to the State Printer Struct
*/
void FillMessageSuspended(SP_INPUT_DATA* SPInput){

  INT16 MessageSuspendedCount = 0;
  PROCESS_CONTROL_BLOCK* pcb;
  int Index = 0;

  for(int i=0; i<MAXPROCESSES; i++){
    pcb = &PRO_INFO->PCB[i];
    if(pcb->in_use == 1){
      if(pcb->state == SUSPENDED_WAITING_FOR_MESSAGE){
	MessageSuspendedCount++;
	SPInput->MessageSuspendedProcessPIDs[Index] = (INT16)(pcb->idnum);

	Index++;
      }
    }
  }
  SPInput->NumberOfMessageSuspendedProcesses = MessageSuspendedCount;
}





/*
   SPLineSetup(OutputLine, "SUS-MSG:",
    		Input->NumberOfMessageSuspendedProcesses,
    		Input->MessageSuspendedProcessPIDs);

	 NEED to ADD WAITING FOR MESSAGE
*/
void osPrintState(char* Action){

  SP_INPUT_DATA SPInput;

  strcpy(SPInput.TargetAction, Action);

  SPInput.CurrentlyRunningPID = GetCurrentPID();
  SPInput.TargetPID = 2;
  SPInput.NumberOfRunningProcesses = 1;
  SPInput.RunningProcessPIDs[0] =  SPInput.CurrentlyRunningPID;

  FillReady(&SPInput);

  FillProcessSuspended(&SPInput);

  FillMessageSuspended(&SPInput);
  
 

  FillTimer(&SPInput);
  FillAllDisk(&SPInput);
  SPInput.NumberOfTerminatedProcesses = 0;
  
  SPPrintLine(&SPInput);

}


char *error_status[] = {"ERR_SUCCESS", "ERR_BAD_PARAM", "ERR_NO_PREVIOUS_WRITE", "ERR_ILLEGAL_ADDRESS", "ERR_DISK_IN_USE", "ERR_BAD_DEVICE_ID", "ERR_NO_DEVICE_FOUND", "DEVICE_IN_USE", "DEVICE_FREE"}; 


void PrintInterrupt(INT32 DeviceID, INT32 Status){

  if(DeviceID == TIMER_INTERRUPT){
    
    aprintf("\nInterrupt Handler: Timer Interrupt\nTimer Status %s\n\n", error_status[Status]);
  }
  if(DeviceID>= DISK_INTERRUPT &&
     DeviceID <= DISK_INTERRUPT + MAX_NUMBER_OF_DISKS -1){
     
    aprintf("\nInterrupt Handler: Disk Interrupt\nHandling Disk %d with Status %s\n\n", DeviceID - DISK_INTERRUPT, error_status[Status]);
  }
}




     
void PrintSVC(long Arguments[], INT32 call_type ){

  char buffer[MAX_NAME_LENGTH];
      
  switch(call_type){
    
  case SYSNUM_GET_TIME_OF_DAY:

    aprintf("\nSVC handler: GetTime\n\n");
    break;

  case SYSNUM_SLEEP:

    aprintf("\nSVC handler: Sleep\nSleep time %d\n\n", Arguments[0]);
    break;
 
  case SYSNUM_GET_PROCESS_ID:

    strcpy(buffer, (char *)Arguments[0]);
    if(strcmp(buffer, "") == 0){
      aprintf("\nSVC handler: GetPid\nGetting the Process ID for the current Process\n\n");
    }
    else{
      aprintf("\nSVC handler: GetPid\nGetting the Process ID for Process %s\n\n", buffer);
    }
    break;
  
  case SYSNUM_CREATE_PROCESS:
    aprintf("\nSVC handler: Create Process\nCreating a Process with name %s\n\n", (char *) Arguments[0]);
    break;

  case SYSNUM_TERMINATE_PROCESS:
    aprintf("\nSVC handler: Terminate Process\nTerminating Target PID %d\n\n", Arguments[0]);

    break;

  case SYSNUM_SUSPEND_PROCESS:
    aprintf("\nSVC handler: Suspend Process\nSuspending PID %d\n\n", Arguments[0]);
    break;

  case SYSNUM_RESUME_PROCESS:
    aprintf("\nSVC handler: Resume Process\nResuming PID %d\n\n", Arguments[0]);
    break;

  case SYSNUM_CHANGE_PRIORITY:
    aprintf("\nSVC handler: Change Priority\nChanging Priority of PID %d to %d\n\n", Arguments[0], Arguments[1]);
    break;

  case SYSNUM_SEND_MESSAGE:
    aprintf("\nSVC handler: Send Message\nSending a Message to PID %d\n\n", Arguments[0]);
    break;

  case SYSNUM_RECEIVE_MESSAGE:
    aprintf("\nSVC handler: Receive Message\nReceving a Message from PID %d\n\n", Arguments[0]);
    break;

  case SYSNUM_PHYSICAL_DISK_READ:
    aprintf("\nSVC handler: Disk Read\nReading from Disk %d\n\n", Arguments[0]);
    break;

  case SYSNUM_PHYSICAL_DISK_WRITE:
    aprintf("\nSVC handler: Disk Write\nWriting to Disk %d\n\n", Arguments[0]);
    break;

  case SYSNUM_CHECK_DISK:
    aprintf("\nSVC handler: Check Disk\nChecking data on Disk %d\n\n", Arguments[0]);
    break;

  default:
      aprintf("\nSVC handler: Call Not Recognized.\n\n");

  }
}
