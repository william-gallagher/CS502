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

//Operating System Includes
#include "process.h"
#include "timerQueue.h"
#include "readyQueue.h"
#include "diskQueue.h"
#include "messageBuffer.h"
#include "osGlobals.h"
#include "osSchedulePrinter.h"
#include "diskManagement.h"
#include "memoryManagement.h"



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

    //Check for errors
    if(mmio.Field4 != ERR_SUCCESS){
      aprintf("\n\nPROB WITH INTERRUPT\n\n");
    }
    while(mmio.Field4 == ERR_SUCCESS) {
      DeviceID = mmio.Field1;
      Status = mmio.Field2;

      //Print a message describing the Interrupt as long as the print
      //is enabled
      PrintInterrupt(DeviceID, Status);   
   
      if(DeviceID == TIMER_INTERRUPT){
 
	HandleTimerInterrupt();
      }
      else if(DeviceID >= DISK_INTERRUPT &&
	      DeviceID <= DISK_INTERRUPT + MAX_NUMBER_OF_DISKS -1){

	long DiskID = DeviceID - DISK_INTERRUPT; 
	HandleDiskInterrupt(DiskID);
	
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


  // Get cause of fault
  mmio.Field1 = mmio.Field2 = mmio.Field3 = 0;
  mmio.Mode = Z502GetInterruptInfo;
  MEM_READ(Z502InterruptDevice, &mmio);
  DeviceID = mmio.Field1;
  Status   = mmio.Field2;

  PrintFault(DeviceID, Status);
  // for(int i=0; i<100000; i++);
 
  //get the current page table
  PROCESS_CONTROL_BLOCK *CurrentPCB = GetCurrentPCB();

  INT16 *PageTable = (INT16 *)CurrentPCB->page_table;
  INT16 *ShadowPageTable = (INT16 *)CurrentPCB->shadow_page_table;
  INT16 Index = (INT16) mmio.Field2;

  //If Valid bit is set and we are here then the user program has
  //asked for an address that is not on a mod 4 boundary.
  //Just terminate the program.
  if(CheckValidBit(PageTable[Index]) == TRUE){

    aprintf("\n\nERROR: Bad Address. Terminate Program\n\n");
    long ReturnError;
    osTerminateProcess(-1, &ReturnError);  
  }

  //There are two possibilities. The valid bit is not set because the
  //logical page has never been used or because the page is backed by data
  //in the swap space.

  INT32 OnDisk = CheckOnDisk(Index, ShadowPageTable);

       
  GetPhysicalFrame(&PageTable[Index], CurrentPCB, Index);
  SetValidBit(&PageTable[Index]);

  if(OnDisk == TRUE){

    INT16 CacheLine = ShadowPageTable[Index] & 0x0FFF;

    char DataBuffer[16];

    // aprintf("In base.c. PID %d is reading for disk %d\n", CurrentPCB->idnum, SWAP_DISK);
     
    osDiskReadRequest(SWAP_DISK, CacheLine, (long)DataBuffer);
      
    INT16 Frame = (PageTable[Index] & 0x0FFF);
    Z502WritePhysicalMemory(Frame, DataBuffer);

    ShadowPageTable[Index] &= 0x7FFF;
   }
  osPrintMemoryState();
    
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

  short call_type;

  call_type = (short) SystemCallData->SystemCallNumber;
  
  long Arguments[MAX_NUMBER_ARGUMENTS];
  for (INT32 i = 0; i < SystemCallData->NumberOfArguments - 1; i++) {
    Arguments[i] = (long)SystemCallData->Argument[i];
  }

  //Print the state of the SVC while the print is enabled.
  PrintSVC(Arguments, call_type);

  long *TimeOfDay;
  char ProcessName[MAX_NAME_LENGTH];

  long NewPriority;
  long *PID;
  long *ReturnError;

  long TargetPID;
  long SourcePID;
  long PID2; //need better name
  
  long DiskID;
  long DiskSector;
  long DiskAddress;
  long SleepTime;
  char *MessageBuffer;
  long MessageLength;
  long MessRecLength;
  long *MessSendLength;
  long *SenderPID;

  char *FileName;
  long Inode;
  long Index;
  char *WriteBuffer;

  long StartingAddress;
  long PagesInSharedArea;
  char *AreaTag;
  long *OurSharedID;
  
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
      ReturnError = (long *)SystemCallData->Argument[1];
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
      osCreateProcess(name, start_address, priority, process_id, return_error);
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
      MessageBuffer = (char *)SystemCallData->Argument[1];
      MessageLength = (long)SystemCallData->Argument[2];
      ReturnError = (long *)SystemCallData->Argument[3];
      osSendMessage(TargetPID, MessageBuffer, MessageLength, ReturnError);
      break;

    case SYSNUM_RECEIVE_MESSAGE:

      SourcePID = (long)SystemCallData->Argument[0];
      MessageBuffer = (char *)SystemCallData->Argument[1];
      MessRecLength = (long)SystemCallData->Argument[2];
      MessSendLength = (long *)SystemCallData->Argument[3];
      SenderPID = (long *)SystemCallData->Argument[4];
      ReturnError = (long *)SystemCallData->Argument[5];
      osReceiveMessage(SourcePID, MessageBuffer, MessRecLength, MessSendLength, SenderPID, ReturnError);
      break;

    case SYSNUM_FORMAT:

      DiskID = (long)SystemCallData->Argument[0];
      ReturnError = (long *)SystemCallData->Argument[1];
      osFormatDisk(DiskID, ReturnError);
      break;

    case SYSNUM_OPEN_DIR:

      DiskID = (long)SystemCallData->Argument[0];
      FileName = (char *)SystemCallData->Argument[1];
      ReturnError = (long *)SystemCallData->Argument[2];
      osOpenDirectory(DiskID, FileName, ReturnError);
      break;

    case SYSNUM_CREATE_DIR:

      FileName = (char *)SystemCallData->Argument[0];
      ReturnError = (long *)SystemCallData->Argument[1];
      osCreateFile(FileName, ReturnError, 1);
      
      break;
      
    case SYSNUM_CREATE_FILE:

      FileName = (char *)SystemCallData->Argument[0];
      ReturnError = (long *)SystemCallData->Argument[1];
      osCreateFile(FileName, ReturnError, 0);
      
      break;

    case SYSNUM_OPEN_FILE:

      FileName = (char *)SystemCallData->Argument[0];
      long *InodePtr = (long *)SystemCallData->Argument[1];
      ReturnError = (long *)SystemCallData->Argument[2];
      osOpenFile(FileName, InodePtr, ReturnError);
      break;      

    case SYSNUM_WRITE_FILE:
      Inode = (long)SystemCallData->Argument[0];
      Index = (long)SystemCallData->Argument[1];
      WriteBuffer = (char *)SystemCallData->Argument[2];
      ReturnError = (long *)SystemCallData->Argument[3];
      osWriteFile(Inode, Index, WriteBuffer, ReturnError);
      break;

    case SYSNUM_READ_FILE:
      Inode = (long)SystemCallData->Argument[0];
      Index = (long)SystemCallData->Argument[1];
      WriteBuffer = (char *)SystemCallData->Argument[2];
      ReturnError = (long *)SystemCallData->Argument[3];
      osReadFile(Inode, Index, WriteBuffer, ReturnError);
      break;

    case SYSNUM_CLOSE_FILE:
      Inode = (long)SystemCallData->Argument[0];
      ReturnError = (long *)SystemCallData->Argument[1];
      osCloseFile(Inode, ReturnError); 
      break;

    case SYSNUM_DIR_CONTENTS:
      ReturnError = (long *)SystemCallData->Argument[0];
      osPrintCurrentDirContents(ReturnError);
      break;
    
    case SYSNUM_DEFINE_SHARED_AREA:
      StartingAddress = (long)SystemCallData->Argument[0];
      PagesInSharedArea = (long)SystemCallData->Argument[1];
      AreaTag = (char *)SystemCallData->Argument[2];
      OurSharedID = (long *)SystemCallData->Argument[3];
      ReturnError = (long *)SystemCallData->Argument[4];
      InitializeSharedArea(StartingAddress, PagesInSharedArea, AreaTag,
			   OurSharedID, ReturnError);
      
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

	    //set flag in OS to indicate multiprocessor mode
	    M = MULTI;
        }
    } else {
        aprintf("Simulation is running as a UniProcessor\n");
        aprintf("Add an 'M' to the command line to invoke multiprocessor operation.\n\n");

	M = UNI;
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
    InitializeProcessInfo();

    //Create the Frame Manager
    InitializeFrameManager();

    //Create the Queues and Buffers
    CreateTimerQueue();
    CreateReadyQueue();
    CreateMessageBuffer();

    NextFrame = 0;
    NextSwapLocation = 0x600;
    Cache = NULL;

    //for shared memory
    SharedIDCount = 0;

    for(int i=0; i<MAX_NUMBER_OF_DISKS; i++){
      CreateDiskQueue(i);
    }

    //Initialize Disk Queue Locks
    InitializeDiskLocks();

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
	  SetTestNumber(buffer);
	  SetPrintOptions(TestRunning);

	  //create the intial process running the code in test.c
	  osCreateProcess(argv[1], test, 1, &process_id, &return_error);

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

