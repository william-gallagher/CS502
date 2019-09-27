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

/*
Fill the Ready Queue parts of the schedule printer
*/
void FillReady(SP_INPUT_DATA* SPInput){

  INT16 ReadyCount = 0;
  int Index = 0;

  //add lock
  RQ_ELEMENT* rqe;

  INT32 Suspend_Until_Locked = TRUE;
  INT32 Sucess_Failure = -1;

  READ_MODIFY(READY_LOCK, 1, Suspend_Until_Locked, &Sucess_Failure);

  if(Sucess_Failure == FALSE){
    aprintf("\n\nCould Not Obtain the Lock for the Ready Queue\n\n");
  }

  rqe = QWalk(ready_queue_id, Index);

  
  while((long)rqe != -1){
    SPInput->ReadyProcessPIDs[Index] = (INT16)(rqe->PID);
 
    Index++;
    ReadyCount++;
    rqe = QWalk(ready_queue_id, Index);
  }

  READ_MODIFY(READY_LOCK, 0, Suspend_Until_Locked, &Sucess_Failure);

  if(Sucess_Failure == FALSE){
    aprintf("\n\n Failure to give up Ready Lock\n\n");
  }

  SPInput->NumberOfReadyProcesses = ReadyCount;
}

void FillTimer(SP_INPUT_DATA* SPInput){

  INT16 TimerCount = 0;
  int Index = 0;

  //add lock
  TQ_ELEMENT* tqe;

  INT32 Suspend_Until_Locked = TRUE;
  INT32 Sucess_Failure = -1;

  READ_MODIFY(TIMER_LOCK, 1, Suspend_Until_Locked, &Sucess_Failure);

  if(Sucess_Failure == FALSE){
    aprintf("\n\nCould Not Obtain the Lock for the Timer Queue\n\n");
  }

  
  tqe = QWalk(timer_queue_id, Index);
  
  while((long)tqe != -1){
    SPInput->TimerSuspendedProcessPIDs[Index] = (INT16)(tqe->PID);
    Index++;
    TimerCount++;
    tqe = QWalk(timer_queue_id, Index);
  }

  READ_MODIFY(TIMER_LOCK, 0, Suspend_Until_Locked, &Sucess_Failure);

  if(Sucess_Failure == FALSE){
    aprintf("\n\n Failure to give up Ready Lock\n\n");
  }

  SPInput->NumberOfTimerSuspendedProcesses = TimerCount;
}

void FillDisk(SP_INPUT_DATA* SPInput, int DiskNumber, INT16* DiskCount){

  int Index = 0;
  DQ_ELEMENT* dqe;
    
  INT32 Suspend_Until_Locked = TRUE;
  INT32 Sucess_Failure = -1;

  READ_MODIFY(DISK_LOCK[DiskNumber], 1, Suspend_Until_Locked, &Sucess_Failure);

  if(Sucess_Failure == FALSE){
    aprintf("\n\nCould Not Obtain the Lock for Disk Queue %ld\n\n", DiskNumber);
  }
    
  dqe = QWalk(disk_queue[DiskNumber], Index);
  
  while((long)dqe != -1){
    SPInput->DiskSuspendedProcessPIDs[Index] = (INT16)(dqe->PID);
    Index++;
    (*DiskCount)++;
    dqe = QWalk(disk_queue[DiskNumber], Index);
  }

  READ_MODIFY(DISK_LOCK[DiskNumber], 0, Suspend_Until_Locked, &Sucess_Failure);

  if(Sucess_Failure == FALSE){
    aprintf("\n\n Failure to give up the Lock for Disk Queue\n\n");
  }
}

void FillAllDisk(SP_INPUT_DATA* SPInput){

  INT16 DiskCount = 0;

  //check each disk queue in turn
  for(int i=0; i<MAX_NUMBER_OF_DISKS; i++){

    FillDisk(SPInput, i, &DiskCount);
  }

  //return lock
  SPInput->NumberOfDiskSuspendedProcesses = DiskCount;
}

/*
Add the Suspended Process PIDs to the State Printer Struct
*/
void FillSuspended(SP_INPUT_DATA* SPInput){

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

void osPrintState(char* Action){

  SP_INPUT_DATA SPInput;

  strcpy(SPInput.TargetAction, Action);

  SPInput.CurrentlyRunningPID = GetCurrentPID();
  SPInput.TargetPID = 2;
  SPInput.NumberOfRunningProcesses = 1;
  SPInput.RunningProcessPIDs[0] =  SPInput.CurrentlyRunningPID;
  // SPInput.NumberOfReadyProcesses = 0;
  FillReady(&SPInput);
  // SPInput.NumberOfProcSuspendedProcesses = 0;
  FillSuspended(&SPInput);
  SPInput.NumberOfMessageSuspendedProcesses = 0;
  // SPInput.NumberOfTimerSuspendedProcesses = 0;
   FillTimer(&SPInput);
   // SPInput.NumberOfDiskSuspendedProcesses = 0;
   FillAllDisk(&SPInput);
  SPInput.NumberOfTerminatedProcesses = 0;
  
  SPPrintLine(&SPInput);

}
