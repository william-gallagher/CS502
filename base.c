/************************************************************************

 This code forms the base of the operating system you will
 build.  It has only the barest rudiments of what you will
 eventually construct; yet it contains the interfaces that
 allow test.c and z502.c to be successfully built together.

 Revision History:
 1.0 August 1990
 1.1 December 1990: Portability attempted.
 1.3 July     1992: More Portability enhancements.
 Add call to SampleCode.
 1.4 December 1992: Limit (temporarily) printout in
 interrupt handler.  More portability.
 2.0 January  2000: A number of small changes.
 2.1 May      2001: Bug fixes and clear STAT_VECTOR
 2.2 July     2002: Make code appropriate for undergrads.
 Default program start is in test0.
 3.0 August   2004: Modified to support memory mapped IO
 3.1 August   2004: hardware interrupt runs on separate thread
 3.11 August  2004: Support for OS level locking
 4.0  July    2013: Major portions rewritten to support multiple threads
 4.20 Jan     2015: Thread safe code - prepare for multiprocessors
 4.51 August  2018: Minor bug fixes
 4.60 February2019: Test for the ability to alloc large amounts of memory
 ************************************************************************/

#include             "global.h"
#include             "syscalls.h"
#include             "protos.h"
#include             "string.h"
#include             <stdlib.h>
#include             <ctype.h>

//My includes
#include "process.h"
#include "timerQueue.h"
#include "readyQueue.h"
#include "diskQueue.h"
#include "messageBuffer.h"
#include "os_globals.h"
#include "os_Schedule_Printer.h"

long GetTestName(char* test_name);

//  This is a mapping of system call nmemonics with definitions

char *call_names[] = {"MemRead  ", "MemWrite ", "ReadMod  ", "GetTime  ",
        "Sleep    ", "GetPid   ", "Create   ", "TermProc ", "Suspend  ",
        "Resume   ", "ChPrior  ", "Send     ", "Receive  ", "PhyDskRd ",
        "PhyDskWrt", "DefShArea", "Format   ", "CheckDisk", "OpenDir  ",
        "OpenFile ", "CreaDir  ", "CreaFile ", "ReadFile ", "WriteFile",
        "CloseFile", "DirContnt", "DelDirect", "DelFile  " };

/************************************************************************
 INTERRUPT_HANDLER
 When the Z502 gets a hardware interrupt, it transfers control to
 this routine in the Operating System.
 NOTE WELL:  Just because the timer or the disk has interrupted, and
         therefore this code is executing, it does NOT mean the 
         action you requested was successful.
         For instance, if you give the timer a NEGATIVE time - it 
         doesn't know what to do.  It can only cause an interrupt
         here with an error.
         If you try to read a sector from a disk but that sector
         hasn't been written to, that disk will interrupt - the
         data isn't valid and it's telling you it was confused.
         YOU MUST READ THE ERROR STATUS ON THE INTERRUPT
************************************************************************/

void InterruptHandler(void) {
    INT32 DeviceID;
    INT32 Status;

    MEMORY_MAPPED_IO mmio;       // Enables communication with hardware
   
	
    // Get cause of interrupt
    mmio.Mode = Z502GetInterruptInfo;
    mmio.Field1 = mmio.Field2 = mmio.Field3 = mmio.Field4 = 0;
    MEM_READ(Z502InterruptDevice, &mmio);

    
    
    if(mmio.Field4 != ERR_SUCCESS){
      aprintf("\n\nPROB WITH INTERRUPT\n\n");
    }
    while(mmio.Field4 == ERR_SUCCESS) {
      DeviceID = mmio.Field1;
      Status = mmio.Field2;

      PrintInterrupt(DeviceID, Status);   
   
      // HAVE YOU CHECKED THAT THE INTERRUPTING DEVICE FINISHED WITHOUT ERROR?



      if(DeviceID == TIMER_INTERRUPT){
 
	HandleTimerInterrupt();
      }
      
      else if(DeviceID >= DISK_INTERRUPT &&
	      DeviceID <= DISK_INTERRUPT + MAX_NUMBER_OF_DISKS -1){

	long disk_id = DeviceID - DISK_INTERRUPT;
	DQ_ELEMENT* dqe = RemoveFromDiskQueueHead(disk_id);
	
	AddToReadyQueue(dqe->context, dqe->PID, dqe->PCB);
	free(dqe);
	//need to check for another element on the disk queue
	dqe = CheckDiskQueue(disk_id);
	if((long)dqe != -1){
	  
	
	  //don't need to check if disk is busy- can't be because we just used it
	  if(dqe->disk_action == READ_DISK){  	
	    mmio.Mode = Z502DiskRead;
	  }
	  else{
	    mmio.Mode = Z502DiskWrite;
	  }
	  mmio.Field1 = dqe->disk_id;
	  mmio.Field2 = dqe->disk_sector;
	  mmio.Field3 = dqe->disk_address;
	
	  MEM_WRITE(Z502Disk, &mmio);
	}
	
      }
      
      //see if there is another interrupt
      mmio.Mode = Z502GetInterruptInfo;
      mmio.Field1 = mmio.Field2 = mmio.Field3 = mmio.Field4 = 0;
      MEM_READ(Z502InterruptDevice, &mmio);
    }
    

}           // End of InterruptHandler

/************************************************************************
 FAULT_HANDLER
 The beginning of the OS502.  Used to receive hardware faults.
 ************************************************************************/

void FaultHandler(void) {
    INT32 DeviceID;
    INT32 Status;
    MEMORY_MAPPED_IO mmio;       // Enables communication with hardware

    static BOOL remove_this_from_your_fault_code = TRUE; 
    static INT32 how_many_fault_entries = 0; 

    // Get cause of fault
    mmio.Field1 = mmio.Field2 = mmio.Field3 = 0;
    mmio.Mode = Z502GetInterruptInfo;
    MEM_READ(Z502InterruptDevice, &mmio);
    DeviceID = mmio.Field1;
    Status   = mmio.Field2;

    // This causes a print of the first few faults - and then stops printing!
    // You can adjust this as you wish.  BUT this code as written here gives
    // an indication of what's happening but then stops printing for long tests
    // thus limiting the output.
    how_many_fault_entries++; 
    if (remove_this_from_your_fault_code && (how_many_fault_entries < 10)) {
           aprintf("FaultHandler: Found device ID %d with status %d\n",
                          (int) mmio.Field1, (int) mmio.Field2);
    }
    
} // End of FaultHandler

/************************************************************************
 SVC
 The beginning of the OS502.  Used to receive software interrupts.
 All system calls come to this point in the code and are to be
 handled by the student written code here.
 The variable do_print is designed to print out the data for the
 incoming calls, but does so only for the first ten calls.  This
 allows the user to see what's happening, but doesn't overwhelm
 with the amount of data.
 ************************************************************************/

void svc(SYSTEM_CALL_DATA *SystemCallData) {

  //just test the state printer
   osPrintState("Call SVC");

  short call_type;

  call_type = (short) SystemCallData->SystemCallNumber;
  
  long Arguments[MAX_NUMBER_ARGUMENTS];
  for (INT32 i = 0; i < SystemCallData->NumberOfArguments - 1; i++) {
    Arguments[i] = (long)SystemCallData->Argument[i];
  }

  PrintSVC(Arguments, call_type);

  long *TimeOfDay;
  char ProcessName[MAX_NAME_LENGTH];

  long NewPriority;
  long *PID;
  long *ReturnError;

  long TargetPID;
  long PID2; //need better name
  
  long DiskID;
  long DiskSector;
  long DiskAddress;
  long SleepTime;

    
    switch(call_type){
      
    case SYSNUM_GET_TIME_OF_DAY:
 
      TimeOfDay = (long *)SystemCallData->Argument[0];
      GetTimeOfDay(TimeOfDay);      
      break;

    case SYSNUM_GET_PROCESS_ID:

      strcpy(ProcessName, (char *)SystemCallData->Argument[0]);
      PID = (long *)SystemCallData->Argument[1];
      ReturnError = (long *)SystemCallData->Argument[2];
      GetProcessID(ProcessName, PID, ReturnError);
      break;

    case SYSNUM_SLEEP:
      
      SleepTime = (long)SystemCallData->Argument[0];
      StartTimer(SleepTime);
      break;
      
    case SYSNUM_TERMINATE_PROCESS:

      TargetPID = (long)SystemCallData->Argument[0];
      ReturnError = (long *)SystemCallData->Argument[2];
      osTerminateProcess(TargetPID, ReturnError);
      break;
      
    case SYSNUM_PHYSICAL_DISK_READ:

      DiskID = (long)SystemCallData->Argument[0];
      DiskSector = (long)SystemCallData->Argument[1];
      DiskAddress = (long)SystemCallData->Argument[2];
      osDiskReadRequest(DiskID, DiskSector, DiskAddress);
      break;

    case SYSNUM_PHYSICAL_DISK_WRITE:

      DiskID = (long)SystemCallData->Argument[0];
      DiskSector = (long)SystemCallData->Argument[1];
      DiskAddress = (long)SystemCallData->Argument[2];
      osDiskWriteRequest(DiskID, DiskSector, DiskAddress);
      break;
      
    case SYSNUM_CHECK_DISK:
      DiskID = (long)SystemCallData->Argument[0];
      DiskSector = (long)SystemCallData->Argument[1];

      osCheckDiskRequest(DiskID, DiskSector);
      break;


    case SYSNUM_CREATE_PROCESS:

      aprintf("");
      char* name = (char *)SystemCallData->Argument[0];
      long start_address = (long)SystemCallData->Argument[1];
      long priority = (long)SystemCallData->Argument[2];
      long* process_id = (long *)SystemCallData->Argument[3];
      long* return_error = (long *)SystemCallData->Argument[4];
      CreateProcess(name, start_address, priority, PRO_INFO->current, process_id, return_error);
      break;

    case SYSNUM_SUSPEND_PROCESS:

      PID2 = (long)SystemCallData->Argument[0];
      return_error = (long *)SystemCallData->Argument[1];
      osSuspendProcess(PID2, return_error);

      break;

    case SYSNUM_RESUME_PROCESS:

      PID2 = (long)SystemCallData->Argument[0];
      return_error = (long *)SystemCallData->Argument[1];
      osResumeProcess(PID2, return_error);

      break;

    case SYSNUM_CHANGE_PRIORITY:

      PID2 = (long)SystemCallData->Argument[0];
      NewPriority = (long)SystemCallData->Argument[1];
      ReturnError = (long *)SystemCallData->Argument[2];
      osChangePriority(PID2, NewPriority, ReturnError);
      
      break;

    case SYSNUM_SEND_MESSAGE:

      TargetPID = (long)SystemCallData->Argument[0];
      char *MessageBuffer = (char *)SystemCallData->Argument[1];
      long MessageLength = (long)SystemCallData->Argument[2];
      ReturnError = (long *)SystemCallData->Argument[3];
      osSendMessage(TargetPID, MessageBuffer, MessageLength, ReturnError);
      break;

    case SYSNUM_RECEIVE_MESSAGE:

      aprintf("");
      long SourcePID = (long)SystemCallData->Argument[0];
      MessageBuffer = (char *)SystemCallData->Argument[1];
      long MessRecLength = (long)SystemCallData->Argument[2];
      long* MessSendLength = (long *)SystemCallData->Argument[3];
      long *SenderPID = (long *)SystemCallData->Argument[4];
      ReturnError = (long *)SystemCallData->Argument[5];
      osReceiveMessage(SourcePID, MessageBuffer, MessRecLength, MessSendLength, SenderPID, ReturnError);
      break;
     
    }

}           // End of SVC

/************************************************************************
 osInit
 This is the first routine called after the simulation begins.  This
 is equivalent to boot code.  All the initial OS components can be
 defined and initialized here.
 ************************************************************************/

void osInit(int argc, char *argv[]) {
    // Every process will have a page table.  This will be used in
    // the second half of the project.  
    void *PageTable = (void *) calloc(2, NUMBER_VIRTUAL_PAGES);
    INT32 i;
    MEMORY_MAPPED_IO mmio;

    // Demonstrates how calling arguments are passed thru to here

    aprintf("Program called with %d arguments:", argc);
    for (i = 0; i < argc; i++)
        aprintf(" %s", argv[i]);
    aprintf("\n");
    aprintf("Calling with argument 'sample' executes the sample program.\n");

    // Here we check if a second argument is present on the command line.
    // If so, run in multiprocessor mode.  Note - sometimes people change
    // around where the "M" should go.  Allow for both possibilities
    if (argc > 2) {
        if ((strcmp(argv[1], "M") ==0) || (strcmp(argv[1], "m")==0)) {
            strcpy(argv[1], argv[2]);
            strcpy(argv[2],"M\0");
        }
        if ((strcmp(argv[2], "M") ==0) || (strcmp(argv[2], "m")==0)) {
            aprintf("Simulation is running as a MultProcessor\n\n");
            mmio.Mode = Z502SetProcessorNumber;
            mmio.Field1 = MAX_NUMBER_OF_PROCESSORS;
            mmio.Field2 = (long) 0;
            mmio.Field3 = (long) 0;
            mmio.Field4 = (long) 0;
            MEM_WRITE(Z502Processor, &mmio);   // Set the number of processors
        }
    } else {
        aprintf("Simulation is running as a UniProcessor\n");
        aprintf("Add an 'M' to the command line to invoke multiprocessor operation.\n\n");
    }

    //  Some students have complained that their code is unable to allocate
    //  memory.  Who knows what's going on, other than the compiler has some
    //  wacky switch being used.  We try to allocate memory here and stop
    //  dead if we're unable to do so.
    //  We're allocating and freeing 8 Meg - that should be more than
    //  enough to see if it works.
    void *Temporary = (void *) calloc( 8, 1024 * 1024);
    if ( Temporary == NULL )  {  // Not allocated
	    printf( "Unable to allocate memory in osInit.  Terminating simulation\n");
	    exit(0);
    }
    free(Temporary);
    //  Determine if the switch was set, and if so go to demo routine.
    //  We do this by Initializing and Starting a context that contains
    //     the address where the new test is run.
    //  Look at this carefully - this is an example of how you will start
    //     all of the other tests.

    //create the structures for the OS
    CreatePRO_INFO();

    
    //Timer Queue
    if(CreateTimerQueue() == -1){
      aprintf("\n\nUnable to create Timer Queue!\n\n");
    }
    //Disk Queues
    for(int i=0; i<MAX_NUMBER_OF_DISKS; i++){
      CreateDiskQueue(i);
    }

    //Initialize Disk Queue Locks
    InitializeDiskLocks();
    
    //Ready Queue
    if(CreateReadyQueue() == -1){
      aprintf("\n\nUnable to create Ready Queue!\n\n");
    }
       
    //Message Buffer
    if(CreateMessageQueue() == -1){
      aprintf("\n\nUnable to create Message Queue!\n\n");
    }

    
    if ((argc > 1) && (strcmp(argv[1], "sample") == 0)) {
        mmio.Mode = Z502InitializeContext;
        mmio.Field1 = 0;
        mmio.Field2 = (long) SampleCode;
        mmio.Field3 = (long) PageTable;

        MEM_WRITE(Z502Context, &mmio);   // Start of Make Context Sequence
        mmio.Mode = Z502StartContext;
        // Field1 contains the value of the context returned in the last call
        mmio.Field2 = START_NEW_CONTEXT_AND_SUSPEND;
        MEM_WRITE(Z502Context, &mmio);     // Start up the context

    } // End of handler for sample code - This routine should never return here

	if (argc > 1) {
	  long return_error;
	  long process_id;

	  char buffer[20];
	  strcpy(buffer, argv[1]);
	  long test = GetTestName(buffer);
	  CreateProcess(argv[1], test, 1, -1, &process_id, &return_error);


	  PRO_INFO->current = 0;//hackhackhack....

	  dispatcher();
	} 
	
    //  By default test0 runs if no arguments are given on the command line
    //  Creation and Switching of contexts should be done in a separate routine.
    //  This should be done by a "OsMakeProcess" routine, so that
    //  test0 runs on a process recognized by the operating system.

    mmio.Mode = Z502InitializeContext;
    mmio.Field1 = 0;
    mmio.Field2 = (long) test0;
    mmio.Field3 = (long) PageTable;

    MEM_WRITE(Z502Context, &mmio);   // Start this new Context Sequence
    mmio.Mode = Z502StartContext;
    // Field1 contains the value of the context returned in the last call
    // Suspends this current thread
    mmio.Field2 = START_NEW_CONTEXT_AND_SUSPEND;
    MEM_WRITE(Z502Context, &mmio);     // Start up the context

}                                               // End of osInit


long GetTestName(char* test_name){

  printf("%s is the test name\n", test_name);

  if(strcmp("sample", test_name) == 0){
    return (long)(SampleCode);
  }
  if(strcmp("test0", test_name) == 0){
    TestRunning = 0;
    return (long)(test0);
  }
  if(strcmp("test1", test_name) == 0){
    TestRunning = 1;
    return (long)(test1);
  }    
  if(strcmp("test2", test_name) == 0){
    TestRunning = 2;
    return (long)(test2);
  }
  if(strcmp("test3", test_name) == 0){
    TestRunning = 3;
    return (long)(test3);
  }
  if(strcmp("test4", test_name) == 0){
    TestRunning = 4;
    return (long)(test4);
  }
  if(strcmp("test5", test_name) == 0){
    TestRunning = 5;
    return (long)(test5);
  }
  if(strcmp("test6", test_name) == 0){
    TestRunning = 6;
    return (long)(test6);
  }    
  if(strcmp("test7", test_name) == 0){
    TestRunning = 7;
    return (long)(test7);
  }
  if(strcmp("test8", test_name) == 0){
    TestRunning = 8;
    return (long)(test8);
  }
  if(strcmp("test9", test_name) == 0){
    TestRunning = 9;
    return (long)(test9);
  }
  if(strcmp("test10", test_name) == 0){
    TestRunning = 10;
    return (long)(test10);
  }
  if(strcmp("test11", test_name) == 0){
    TestRunning = 11;
    return (long)(test11);
  }
  if(strcmp("test12", test_name) == 0){
    TestRunning = 12;
    return (long)(test12);
  }
  if(strcmp("test13", test_name) == 0){
    TestRunning = 13;
    return (long)(test13);
  }
  if(strcmp("test14", test_name) == 0){
    TestRunning = 14;
    return (long)(test14);
  }
  return 0;
}
