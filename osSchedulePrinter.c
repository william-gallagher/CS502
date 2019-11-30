/*
osSchedulerPrinter.c

This file holds the functions that set up the State Printer. It also
holds the PrintSVC() and the PrintInterrupt() which print out the status
of the SVC and Interrupt when enabled.
*/

#include <string.h>
#include "syscalls.h"
#include "osGlobals.h"
#include "global.h"
#include "protos.h"
#include "process.h"
#include "timerQueue.h"
#include "osSchedulePrinter.h"

/*
This function fills the Ready Queue part of the Scheduler Printer.
It walks the Ready Queue and transfers the PIDs to the SP_INPUT struct.
*/
void FillReady(SP_INPUT_DATA *SPInput){

  INT16 ReadyCount = 0;
  INT32 Index = 0;
  RQ_ELEMENT *rqe;

  LockLocation(READY_LOCK);

  rqe = QWalk(ready_queue_id, Index);

  //Walk the Ready Queue until can't find anymore items
  while((long)rqe != -1){
    SPInput->ReadyProcessPIDs[Index] = (INT16)(rqe->PID);
 
    Index++;
    ReadyCount++;
    rqe = QWalk(ready_queue_id, Index);
  }
  UnlockLocation(READY_LOCK);
  SPInput->NumberOfReadyProcesses = ReadyCount;
}

/*
This function fills the Timer Queue part of the Scheduler Printer.
It walks the Timer Queue and transfers the PIDs to the SP_INPUT struct.
*/
void FillTimer(SP_INPUT_DATA *SPInput){

  INT16 TimerCount = 0;
  int Index = 0;
  TQ_ELEMENT *tqe;

  LockLocation(TIMER_LOCK);
  
  tqe = QWalk(timer_queue_id, Index);

  //Walk the Timer Queue until can't find anymore items
  while((long)tqe != -1){
    SPInput->TimerSuspendedProcessPIDs[Index] = (INT16)(tqe->PID);
    Index++;
    TimerCount++;
    tqe = QWalk(timer_queue_id, Index);
  }

  UnlockLocation(TIMER_LOCK);
  SPInput->NumberOfTimerSuspendedProcesses = TimerCount;
}

/*
This function fills the Disk Queue part of the Scheduler Printer given
by DiskID. It walks the Ready Queue and transfers the PIDs to the 
SP_INPUT struct.
*/
void FillDisk(SP_INPUT_DATA* SPInput, int DiskID, INT16* DiskCount){

  INT32 Index = 0;
  DQ_ELEMENT* dqe;

  LockLocation(DISK_LOCK[DiskID]);

  dqe = QWalk(disk_queue[DiskID], Index);

  //Walk the Disk Queue until can't find anymore items
  while((long)dqe != -1){
    SPInput->DiskSuspendedProcessPIDs[*DiskCount] = (INT16)(dqe->PID);
    Index++;
    (*DiskCount)++;
    dqe = QWalk(disk_queue[DiskID], Index);
  }

  UnlockLocation(DISK_LOCK[DiskID]);
}

/*
This function steps through the Disk Queues and adds each to the SP_
INPUT struct.
*/
void FillAllDisk(SP_INPUT_DATA *SPInput){

  INT16 DiskCount = 0;

  //check each disk queue in turn
  for(int i=0; i<MAX_NUMBER_OF_DISKS; i++){

    FillDisk(SPInput, i, &DiskCount);
  }

  SPInput->NumberOfDiskSuspendedProcesses = DiskCount;
}

/*
This function fills the Process Suspended part of the Scheduler Printer.
It steps through the PCBs and looks at the SUSPENDED flag and transfers
them to the SP_INPUT struct.
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
This function fills the Message Suspended part of the Scheduler Printer.
It steps through the PCBs and looks at the SUSPENDED flag and transfers
them to the SP_INPUT struct.
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
This is the function that fills and calls the State Printer.
Note that the SchedulerPrints tracks the number of times the Schedule
Printer has been called.
*/
void osPrintState(char* Action, long TargetPID, long CurrentPID){

  //Don't use Schedule Printer if we have reached the print limit
  if(SchedulerPrints <= 0) return;
  
  SP_INPUT_DATA SPInput;

  //Fill the Action field
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

/*
This function prints out the memory state by using the Memory Printer.
*/
void osPrintMemoryState(){

  //Don't use Memory Printer if we have reached the print limit
  if(MemoryPrints <= 0) return;
  
  MP_INPUT_DATA MPInput;
  INT32 FrameData;
  MP_FRAME_DATA *Data;
  INT16 PID;
  INT16 LogicalPage;
  PROCESS_CONTROL_BLOCK *pcb;
  INT16 *PageTable;
  
  INT16 State;
  
  for(INT32 i=0; i<NUMBER_PHYSICAL_PAGES; i++){
    FrameData = FrameManager[i];
    Data = &MPInput.frames[i];

    //test to see if frame is in use.
    if((FrameData & 0xFF000000) == 0){
      Data->InUse = FALSE;
    }
    else{
      Data->InUse = TRUE;
    }

    //Get PID of process using the Frame
    PID = (FrameData>>16) & 0x00FF;
    Data->Pid = PID;

    //Get Logical Page
    LogicalPage = (FrameData & 0x0000FFFF);
    Data->LogicalPage = LogicalPage;

    //Get the state of the Page.
    pcb = GetPCB(PID);
    PageTable = pcb->page_table;

    //check valid bit
    if((PageTable[LogicalPage] & PTBL_VALID_BIT) != 0){
      State = FRAME_VALID;
    }
    //Check Modified Bit
    if((PageTable[LogicalPage] & PTBL_MODIFIED_BIT) != 0){
      State = State + FRAME_MODIFIED;
    }
     //Check Referenced Bit
    if((PageTable[LogicalPage] & PTBL_REFERENCED_BIT) != 0){
      State = State + FRAME_REFERENCED;
    }
    
    Data->State = State;
    
  }

  MPPrintLine(&MPInput);
  MemoryPrints--;
}



//Enable pretty output by printing strings corresponding to error status
char *error_status[] = {"ERR_SUCCESS", "ERR_BAD_PARAM", "ERR_NO_PREVIOUS_WRITE", "ERR_ILLEGAL_ADDRESS", "ERR_DISK_IN_USE", "ERR_BAD_DEVICE_ID", "ERR_NO_DEVICE_FOUND", "DEVICE_IN_USE", "DEVICE_FREE"}; 

/*
This function formats and prints the status of the interrupt handler when
enabled by the number of InterruptHandlerPrints.
*/
void PrintInterrupt(INT32 DeviceID, INT32 Status){

  //Don't print if we have reached the max number of prints for the test
  if(InterruptHandlerPrints <= 0) return;
  
  if(DeviceID == TIMER_INTERRUPT){
    
    aprintf("\n\tInterrupt Handler: Timer Interrupt\n\tTimer Status %s\n\n", error_status[Status]);
  }
  if(DeviceID>= DISK_INTERRUPT &&
     DeviceID <= DISK_INTERRUPT + MAX_NUMBER_OF_DISKS -1){
     
    aprintf("\n\tInterrupt Handler: Disk Interrupt\n\tHandling Disk %d with Status %s\n\n", DeviceID - DISK_INTERRUPT, error_status[Status]);
  }

  //Decrement the count for the interrupt prints
  InterruptHandlerPrints--;
}

/*
This function formats and prints the status of SCV when enabled by the 
number of SVCPrints.
*/
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
      aprintf("\n\tSVC handler: GetPid\n\tGetting the Process ID for Process %s\n\n", buffer);
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

  case SYSNUM_FORMAT:
    aprintf("\n\tSVC handler: Format Disk\n\tFormatting Disk %d\n\n", Arguments[0]);
    break;

  case SYSNUM_OPEN_DIR:
    aprintf("\n\tSVC handler: Open Directory\n\tOpening '%s'\n\n", Arguments[1]);
    break;

  case SYSNUM_CREATE_DIR:
   aprintf("\n\tSVC handler: Create Directory\n\tCreating Dir '%s'\n\n", Arguments[0]);
   break;

  case SYSNUM_CREATE_FILE:
    aprintf("\n\tSVC handler: Create File\n\tCreating File '%s'\n\n", Arguments[0]);
    break;

  case SYSNUM_OPEN_FILE:
    aprintf("\n\tSVC handler: Open File\n\tOpening File '%s'\n\n", Arguments[0]);
    break;

  case SYSNUM_WRITE_FILE:

    aprintf("\n\tSVC handler: Write File\n\tWriting Logical Block %x to Inode %x\n\n", Arguments[1], Arguments[0]);    
    break;

  case SYSNUM_READ_FILE:

    aprintf("\n\tSVC handler: Read File\n\tReading Logical Block %x to Inode %x\n\n", Arguments[1], Arguments[0]);    
    break;

  case SYSNUM_CLOSE_FILE:

    aprintf("\n\tSVC handler: Close File\n\tClosing File given by Inode %x\n\n", Arguments[0]);    
    break;

  case SYSNUM_DEFINE_SHARED_AREA:
    
    aprintf("\n\tSVC handler: Define Shared Area\n\tShared area begins at address %x and is %d pages long\n\n", Arguments[0], Arguments[1]);    
    break;
  default:
      aprintf("\n\tSVC handler: Call Not Recognized.\n\n");

  }

  //decrement the count of SVCPrints
  SVCPrints--;
}

/*

*/
void PrintFault(INT32 DeviceID, INT32 Status){

  if(FaultHandlerPrints <= 0) return;

  aprintf("FaultHandler: Found device ID %d with status %d\n", DeviceID,
	  Status);

  FaultHandlerPrints--;
}


/*
A simple function that converts the test name input in the command line
to an integer. Note that sample is given as -1
*/
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
  if(strcmp("test21", TestName) == 0){
    TestRunning = 21;
  }
  if(strcmp("test22", TestName) == 0){
    TestRunning = 22;
  }
  if(strcmp("test23", TestName) == 0){
    TestRunning = 23;
  }
  if(strcmp("test24", TestName) == 0){
    TestRunning = 24;
  }
  if(strcmp("test25", TestName) == 0){
    TestRunning = 25;
  }
  if(strcmp("test26", TestName) == 0){
    TestRunning = 26;
  }
  if(strcmp("test27", TestName) == 0){
    TestRunning = 27;
  }
  if(strcmp("test28", TestName) == 0){
    TestRunning = 28;
  }
  if(strcmp("test29", TestName) == 0){
    TestRunning = 29;
  }
  if(strcmp("test41", TestName) == 0){
    TestRunning = 41;
  }
  if(strcmp("test42", TestName) == 0){
    TestRunning = 42;
  }
  if(strcmp("test43", TestName) == 0){
    TestRunning = 43;
  }
  if(strcmp("test44", TestName) == 0){
    TestRunning = 44;
  }
    if(strcmp("test45", TestName) == 0){
    TestRunning = 45;
  }
  if(strcmp("test46", TestName) == 0){
    TestRunning = 46;
  }
  if(strcmp("test47", TestName) == 0){
    TestRunning = 47;
  }
  if(strcmp("test48", TestName) == 0){
    TestRunning = 48;
  }
  
}

/*
This function sets the print options depending on which test is running.
*/
void SetPrintOptions(INT32 TestRunning) {

  switch(TestRunning){

  case -1: //Running sample()
  case 0:
  case 1:
  case 2:
    SVCPrints = MAX_INT;
    InterruptHandlerPrints = MAX_INT;
    FaultHandlerPrints = 0;
    SchedulerPrints = 0;
    MemoryPrints = 0;
    break;
  case 3:
  case 4:
    SVCPrints = 10;
    InterruptHandlerPrints = 10;
    FaultHandlerPrints = 0;
    SchedulerPrints = MAX_INT;
    MemoryPrints = 0;
    break;
  case 5:
  case 6:
    SVCPrints = 20;
    InterruptHandlerPrints = 10;
    FaultHandlerPrints = 0;
    SchedulerPrints = MAX_INT;
    MemoryPrints = 0;
    break;
  case 7:
    SVCPrints = 30;
    InterruptHandlerPrints = 10;
    FaultHandlerPrints = 0;
    SchedulerPrints = MAX_INT;
    MemoryPrints = 0;
    break;
  case 8:
  case 9:
    SVCPrints = 20;
    InterruptHandlerPrints = 10;
    FaultHandlerPrints = 0;
    SchedulerPrints = MAX_INT;
    MemoryPrints = 0;
    break;
  case 10:
    SVCPrints = 10;
    InterruptHandlerPrints = 10;
    FaultHandlerPrints = 0;
    SchedulerPrints = MAX_INT;
    MemoryPrints = 0;
    break;
  case 11:
  case 12:
  case 13:
  case 14:
    SVCPrints = 20;
    InterruptHandlerPrints = 10;
    SchedulerPrints = 100;
    MemoryPrints = 0;
    break;
  case 21:
  case 22:
    SVCPrints = MAX_INT;
    InterruptHandlerPrints = MAX_INT;
    FaultHandlerPrints = MAX_INT;
    SchedulerPrints = 0;
    MemoryPrints = 0;
    break;
  case 23:
    SVCPrints = 10;
    InterruptHandlerPrints = 10;
    FaultHandlerPrints = 10;
    SchedulerPrints = MAX_INT;
    MemoryPrints = 0;
  case 24:
  case 25:
  case 26:
  case 27:
  case 28:
    SVCPrints = 10;
    InterruptHandlerPrints = 10;
    FaultHandlerPrints = 10;
    SchedulerPrints = 100;
    MemoryPrints = 0;
    break;
  case 41:
  case 42:
    SVCPrints = MAX_INT;
    InterruptHandlerPrints = MAX_INT;
    FaultHandlerPrints = MAX_INT;
    SchedulerPrints = 0;
    MemoryPrints = MAX_INT;
    break;
  case 43:
     SVCPrints = 10;
    InterruptHandlerPrints = 10;
    FaultHandlerPrints = 10;
    SchedulerPrints = 0;
    MemoryPrints = MAX_INT;
    break;
  case 44:
  case 45:
  case 46:
  case 47:
  case 48:
    SVCPrints = 10;
    InterruptHandlerPrints = 10;
    FaultHandlerPrints = 10;
    SchedulerPrints = 0;
    MemoryPrints = 100;
    break;    
  default:
    aprintf("\n\nTest Number Not Recognized\n\n");    
  }
}

/*
This Function takes the input from the command line and determines 
which test is to be run.
*/
long GetTestName(char* test_name){

  if(strcmp("sample", test_name) == 0){
    return (long)(SampleCode);
  }
  if(strcmp("test0", test_name) == 0){
    return (long)(test0);
  }
  if(strcmp("test1", test_name) == 0){
    return (long)(test1);
  }    
  if(strcmp("test2", test_name) == 0){
    return (long)(test2);
  }
  if(strcmp("test3", test_name) == 0){
    return (long)(test3);
  }
  if(strcmp("test4", test_name) == 0){
    return (long)(test4);
  }
  if(strcmp("test5", test_name) == 0){
    return (long)(test5);
  }
  if(strcmp("test6", test_name) == 0){
    return (long)(test6);
  }    
  if(strcmp("test7", test_name) == 0){
    return (long)(test7);
  }
  if(strcmp("test8", test_name) == 0){
    return (long)(test8);
  }
  if(strcmp("test9", test_name) == 0){
    return (long)(test9);
  }
  if(strcmp("test10", test_name) == 0){
    return (long)(test10);
  }
  if(strcmp("test11", test_name) == 0){
    return (long)(test11);
  }
  if(strcmp("test12", test_name) == 0){
    return (long)(test12);
  }
  if(strcmp("test13", test_name) == 0){
    return (long)(test13);
  }
  if(strcmp("test14", test_name) == 0){
    return (long)(test14);
  }
  if(strcmp("test21", test_name) == 0){
    return (long)(test21);
  }
 if(strcmp("test22", test_name) == 0){
    return (long)(test22);
  }
  if(strcmp("test23", test_name) == 0){
    return (long)(test23);
  }
  if(strcmp("test24", test_name) == 0){
    return (long)(test24);
  }
  if(strcmp("test25", test_name) == 0){
    return (long)(test25);
  }
  if(strcmp("test26", test_name) == 0){
    return (long)(test26);
  }
  if(strcmp("test27", test_name) == 0){
    return (long)(test27);
  }
  if(strcmp("test28", test_name) == 0){
    return (long)(test28);
  }
  if(strcmp("test29", test_name) == 0){
    return (long)(test29);
  }
  if(strcmp("test41", test_name) == 0){
    return (long)(test41);
  }
  if(strcmp("test42", test_name) == 0){
    return (long)(test42);
  }
  if(strcmp("test43", test_name) == 0){
    return (long)(test43);
  }
  if(strcmp("test44", test_name) == 0){
    return (long)(test44);
  }
  if(strcmp("test45", test_name) == 0){
    return (long)(test45);
  }
  if(strcmp("test46", test_name) == 0){
    return (long)(test46);
  }
  if(strcmp("test47", test_name) == 0){
    return (long)(test47);
  }
  if(strcmp("test48", test_name) == 0){
    return (long)(test48);
  }
   
  return 0;
}
