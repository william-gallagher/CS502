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
    pcb = &PCB[i];
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
    pcb = &PCB[i];
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
void osPrintState(char* Action, long TargetPID, long CurrentPID){

  //Don't use Schedule Printer if we have reached the print limit
  if(SchedulerPrints <= 0) return;
  
  SP_INPUT_DATA SPInput;

  strcpy(SPInput.TargetAction, Action);

  SPInput.CurrentlyRunningPID = CurrentPID;
  SPInput.TargetPID = TargetPID;
  SPInput.NumberOfRunningProcesses = 0;
  //Maybe set like this until multi-processor
  //SPInput.RunningProcessPIDs[0] =  SPInput.CurrentlyRunningPID;

  FillReady(&SPInput);

  FillProcessSuspended(&SPInput);

  FillMessageSuspended(&SPInput);
  
 

  FillTimer(&SPInput);
  FillAllDisk(&SPInput);
  SPInput.NumberOfTerminatedProcesses = 0;
  
  SPPrintLine(&SPInput);

  //decrement the count for the Scheduler Printer
  SchedulerPrints--;
}


char *error_status[] = {"ERR_SUCCESS", "ERR_BAD_PARAM", "ERR_NO_PREVIOUS_WRITE", "ERR_ILLEGAL_ADDRESS", "ERR_DISK_IN_USE", "ERR_BAD_DEVICE_ID", "ERR_NO_DEVICE_FOUND", "DEVICE_IN_USE", "DEVICE_FREE"}; 


void PrintInterrupt(INT32 DeviceID, INT32 Status){

  //Don't print if we have reached the max number of prints for the test
  if(InterruptHandlerPrints <= 0) return;
  
  if(DeviceID == TIMER_INTERRUPT){
    
    aprintf("\nInterrupt Handler: Timer Interrupt\nTimer Status %s\n\n", error_status[Status]);
  }
  if(DeviceID>= DISK_INTERRUPT &&
     DeviceID <= DISK_INTERRUPT + MAX_NUMBER_OF_DISKS -1){
     
    aprintf("\nInterrupt Handler: Disk Interrupt\nHandling Disk %d with Status %s\n\n", DeviceID - DISK_INTERRUPT, error_status[Status]);
  }

  //decrement the count for the interrupt prints
  InterruptHandlerPrints--;
}




     
void PrintSVC(long Arguments[], INT32 call_type ){

  char buffer[MAX_NAME_LENGTH];

  //Don't print if we have reached the limit for prints
  if(SVCPrints <= 0) return;
       
  switch(call_type){
    
  case SYSNUM_GET_TIME_OF_DAY:

    aprintf("\n\tSVC handler: GetTime\n\n");
    break;

  case SYSNUM_SLEEP:

    aprintf("\n\tSVC handler: Sleep\n\tSleep time: %d\n\n", Arguments[0]);
    break;
 
  case SYSNUM_GET_PROCESS_ID:

    strcpy(buffer, (char *)Arguments[0]);
    if(strcmp(buffer, "") == 0){
      aprintf("\n\tSVC handler: GetPid\n\tGetting the Process ID for the current Process\n\n");
    }
    else{
      aprintf("\nSVC handler: GetPid\n\tGetting the Process ID for Process %s\n\n", buffer);
    }
    break;
  
  case SYSNUM_CREATE_PROCESS:
    aprintf("\n\tSVC handler: Create Process\n\tCreating a Process with name %s\n\n", (char *) Arguments[0]);
    break;

  case SYSNUM_TERMINATE_PROCESS:
    aprintf("\n\tSVC handler: Terminate Process\n\tTerminating Target PID %d\n\n", Arguments[0]);

    break;

  case SYSNUM_SUSPEND_PROCESS:
    aprintf("\n\tSVC handler: Suspend Process\n\tSuspending PID %d\n\n", Arguments[0]);
    break;

  case SYSNUM_RESUME_PROCESS:
    aprintf("\n\tSVC handler: Resume Process\n\tResuming PID %d\n\n", Arguments[0]);
    break;

  case SYSNUM_CHANGE_PRIORITY:
    aprintf("\n\tSVC handler: Change Priority\n\tChanging Priority of PID %d to %d\n\n", Arguments[0], Arguments[1]);
    break;

  case SYSNUM_SEND_MESSAGE:
    aprintf("\n\tSVC handler: Send Message\n\tSending a Message to PID %d\n\n", Arguments[0]);
    break;

  case SYSNUM_RECEIVE_MESSAGE:
    aprintf("\n\tSVC handler: Receive Message\n\tReceving a Message from PID %d\n\n", Arguments[0]);
    break;

  case SYSNUM_PHYSICAL_DISK_READ:
    aprintf("\n\tSVC handler: Disk Read\n\tReading from Disk %d\n\n", Arguments[0]);
    break;

  case SYSNUM_PHYSICAL_DISK_WRITE:
    aprintf("\n\tSVC handler: Disk Write\n\tWriting to Disk %d\n\n", Arguments[0]);
    break;

  case SYSNUM_CHECK_DISK:
    aprintf("\n\tSVC handler: Check Disk\n\tChecking data on Disk %d\n\n", Arguments[0]);
    break;

  default:
      aprintf("\n\tSVC handler: Call Not Recognized.\n\n");

  }

  //decrement the count of SVCPrints
  SVCPrints--;
}

void SetTestNumber(char TestName[]){

  if(strcmp("sample", TestName) == 0){
    TestRunning = -1;
  }
  if(strcmp("test0", TestName) == 0){
    TestRunning = 0;
  }
  if(strcmp("test1", TestName) == 0){
    TestRunning = 1;
  }    
  if(strcmp("test2", TestName) == 0){
    TestRunning = 2;
  }
  if(strcmp("test3", TestName) == 0){
    TestRunning = 3;
  }
  if(strcmp("test4", TestName) == 0){
    TestRunning = 4;
  }
  if(strcmp("test5", TestName) == 0){
    TestRunning = 5;
  }
  if(strcmp("test6", TestName) == 0){
    TestRunning = 6;
  }    
  if(strcmp("test7", TestName) == 0){
    TestRunning = 7;
  }
  if(strcmp("test8", TestName) == 0){
    TestRunning = 8;
  }
  if(strcmp("test9", TestName) == 0){
    TestRunning = 9;
  }
  if(strcmp("test10", TestName) == 0){
    TestRunning = 10;
  }
  if(strcmp("test11", TestName) == 0){
    TestRunning = 11;
  }
  if(strcmp("test12", TestName) == 0){
    TestRunning = 12;
  }
  if(strcmp("test13", TestName) == 0){
    TestRunning = 13;
  }
  if(strcmp("test14", TestName) == 0){
    TestRunning = 14;
  }
}
#define MAX_INT ((UINT32)~0 >> 1)

void SetPrintOptions(INT32 TestRunning) {

  switch(TestRunning){

  case -1: //Running sample()
  case 0:
  case 1:
  case 2:
    SVCPrints = MAX_INT;
    InterruptHandlerPrints = MAX_INT;
    SchedulerPrints = 0;
    MemoryPrints = 0;
    break;
  case 3:
  case 4:
  case 5:
  case 6:
  case 7:
  case 8:
  case 9:
  case 10:
    SVCPrints = 10;
    InterruptHandlerPrints = 10;
    SchedulerPrints = MAX_INT;
    MemoryPrints = 0;
    break;
  case 11:
  case 12:
  case 13:
  case 14:
    SVCPrints = 10;
    InterruptHandlerPrints = 10;
    SchedulerPrints = 100;
    MemoryPrints = 0;
    break;
  default:
    aprintf("\n\nTest Number Not Recognized\n\n");    
  }
}
      
