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
    aprintf("\nWelcome to the IH\n\n");
    long CurrentTime;

    	mmio.Mode = Z502ReturnValue;
	mmio.Field1 = mmio.Field2 = mmio.Field3 = mmio.Field4;
	//	MEM_READ(Z502Clock, &mmio);
	// CurrentTime = mmio.Field1;

	//aprintf("Current time at the beginnning of the ih is %ld\n", CurrentTime);
	
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
      /*
        aprintf( "The InterruptDevice call in the InterruptHandler has failed.\n");
        aprintf("The DeviceId and Status that were returned are not valid.\n");
	}
      */
      // HAVE YOU CHECKED THAT THE INTERRUPTING DEVICE FINISHED WITHOUT ERROR?



      aprintf("InterruptHandler: Found device ID %d with status %d\n",
	      DeviceID, Status);


      if(DeviceID == TIMER_INTERRUPT){
	aprintf("must handle the timer interrupt\n\n");
	    //get the current time
	mmio.Mode = Z502ReturnValue;
	mmio.Field1 = mmio.Field2 = mmio.Field3 = mmio.Field4;
	//	MEM_READ(Z502Clock, &mmio);
        CurrentTime = mmio.Field1;

	//	aprintf("Current time is %ld in ih\n", CurrentTime);
	HandleTimerInterrupt();
      }
      else if(DeviceID >= DISK_INTERRUPT &&
	      DeviceID <= DISK_INTERRUPT + MAX_NUMBER_OF_DISKS -1){

	long disk_id = DeviceID - DISK_INTERRUPT;
	aprintf("\n\n\nWe have a disk interrupt %d\n\n\n", disk_id);
	DQ_ELEMENT* dqe = disk_process[disk_id];
	
	AddToReadyQueue(dqe->context, dqe->PID, dqe->PCB);
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
  osPrintState();

  short call_type;
  static short do_print = 10;
  short i;
  MEMORY_MAPPED_IO mmio;

  call_type = (short) SystemCallData->SystemCallNumber;
  if (do_print > 0) {
    aprintf("SVC handler: %s\n", call_names[call_type]);
    for (i = 0; i < SystemCallData->NumberOfArguments - 1; i++) {
      //Value = (long)*SystemCallData->Argument[i];
      aprintf("Arg %d: Contents = (Decimal) %8ld,  (Hex) %8lX\n", i,
	      (unsigned long) SystemCallData->Argument[i],
	      (unsigned long) SystemCallData->Argument[i]);
    }
    do_print--;
  }
  
  long disk_id;
  long disk_sector;
  long disk_address;
  long sleep_time;
  DQ_ELEMENT* dq;
  PROCESS_CONTROL_BLOCK* curr_proc;
    
    switch(call_type){
      
    case SYSNUM_GET_TIME_OF_DAY:
      mmio.Mode = Z502ReturnValue;
      mmio.Field1 = mmio.Field2 = mmio.Field3 = 0;
      MEM_READ(Z502Clock, &mmio);
      *(long *)SystemCallData->Argument[0] = mmio.Field1;
      break;

      //added in for test 1
    case SYSNUM_GET_PROCESS_ID:
      getProcessID(SystemCallData);
      break;

      //added in for test 2
    case SYSNUM_SLEEP:

      //printf("\n\n\nHere at SYSCALL Sleep\n\n\n");
      
      sleep_time = (long)SystemCallData->Argument[0];
      StartTimer(sleep_time);
      break;
      
    case SYSNUM_TERMINATE_PROCESS:

      aprintf("");
      long PID = GetCurrentPID();
      
      if((long)SystemCallData->Argument[0] == -1){
	printf("Terminate Current process!\n");

	TerminateProcess(PID);
	mmio.Mode = Z502Action;
	mmio.Field1 = mmio.Field2 = mmio.Field3 = 0;
	MEM_WRITE(Z502Halt, &mmio);
      }
      if((long)SystemCallData->Argument[0] == -2){
	printf("Terminate current process and all children\n\n");
	TerminateChildren(PID);
	TerminateProcess(PID);
	//
	//-----------! Can't halt if there are running processes
	//
	mmio.Mode = Z502Action;
	mmio.Field1 = mmio.Field2 = mmio.Field3 = 0;
	MEM_WRITE(Z502Halt, &mmio);
      }
      

      *(long *)SystemCallData->Argument[1] = 0;  //set error message
      break;
      
    case SYSNUM_PHYSICAL_DISK_READ:

      disk_id = (long)SystemCallData->Argument[0];
      disk_sector = (long)SystemCallData->Argument[1];
      disk_address = (long)SystemCallData->Argument[2];



      //curr_proc = &PRO_INFO->PCB[PRO_INFO->current];
      curr_proc = GetCurrentPCB();
      
      //create struct for disk queue
      dq = malloc(sizeof(DQ_ELEMENT));
      dq->context = curr_proc->context;
      dq->PID = curr_proc->idnum;
      dq->PCB = curr_proc;
    
      long status;
      CheckDiskStatus(disk_id, &status);

      if(status == DEVICE_FREE){
	aprintf("\nDisk %d is free\n\n", disk_id);
	aprintf("Proceeding with the read\n");

	//this is where to store the information about the process
	disk_process[disk_id] = dq;
	
	mmio.Mode = Z502DiskRead;
	mmio.Field1 = disk_id;
	mmio.Field2 = disk_sector;
	mmio.Field3 = disk_address;
	
	MEM_WRITE(Z502Disk, &mmio);
     
      }
      else{
	aprintf("\nDevice not free for read\n\n");

	dq->disk_id = disk_id;
	dq->disk_sector = disk_sector;
	dq->disk_address = disk_address;
	AddToDiskQueue(disk_id, dq);
      }

       //set process state to DISK
      ChangeProcessState(GetCurrentPID(), DISK);
  
      dispatcher();
     
       break;
       
    case SYSNUM_PHYSICAL_DISK_WRITE:

      disk_id = (long)SystemCallData->Argument[0];
      disk_sector = (long)SystemCallData->Argument[1];
      disk_address = (long)SystemCallData->Argument[2];

     //curr_proc = &PRO_INFO->PCB[PRO_INFO->current];
      curr_proc = GetCurrentPCB();
      
      //create struct for disk queue
      dq = malloc(sizeof(DQ_ELEMENT));
      dq->context = curr_proc->context;
      dq->PID = curr_proc->idnum;
      dq->PCB = curr_proc;
      
      curr_proc = &PRO_INFO->PCB[PRO_INFO->current];

      CheckDiskStatus(disk_id, &status);

      if(status == DEVICE_FREE){
	aprintf("\nDisk %d is free\n\n", disk_id);
	aprintf("Proceeding with the write\n");

	//this is where to store the information about the process
	disk_process[disk_id] = dq;
	
	mmio.Mode = Z502DiskWrite;
	mmio.Field1 = disk_id;
	mmio.Field2 = disk_sector;
	mmio.Field3 = disk_address;
	
	MEM_WRITE(Z502Disk, &mmio);
     
      }
      else{
	aprintf("\nDevice not free\n\n");

	dq->disk_id = disk_id;
	dq->disk_sector = disk_sector;
	dq->disk_address = disk_address;
	AddToDiskQueue(disk_id, dq);
      }

      dispatcher();

   
       break;
           
    case SYSNUM_CHECK_DISK:
      disk_id = (long)SystemCallData->Argument[0];
      disk_sector = (long)SystemCallData->Argument[1];
       curr_proc = &PRO_INFO->PCB[PRO_INFO->current];

      CheckDiskStatus(disk_id, &status);

      if(status == DEVICE_FREE){
	aprintf("\nDisk %d is free\n\n", disk_id);
	aprintf("Proceeding with the check\n");

	//this is where to store the information about the process
	//	disk_process[disk_id] = curr_proc->context;
	
	mmio.Mode = Z502CheckDisk;
	mmio.Field1 = disk_id;
	mmio.Field2 = mmio.Field3 = 0;
	
	MEM_WRITE(Z502Disk, &mmio);
     
      }
      else{
	aprintf("\nDevice not free\n\n");
	//create struct for disk queue
	dq = malloc(sizeof(DQ_ELEMENT));
	dq->context = curr_proc->context;
	dq->disk_id = disk_id;
	dq->disk_sector = disk_sector;
	//dq->disk_address = disk_address;
	AddToDiskQueue(disk_id, dq);
      }

      //dispatcher();  appears that no interrupt is thrown for CheckDisk
      //browsing z502 code seems to confirm

      break;


    case SYSNUM_CREATE_PROCESS:

	 printf("\n\n\ncreate a process here\n\n\n");
	 char* name = (char *)SystemCallData->Argument[0];
	 long start_address = (long)SystemCallData->Argument[1];
	 long priority = (long)SystemCallData->Argument[2];
	 long* process_id = (long *)SystemCallData->Argument[3];
	 long* return_error = (long *)SystemCallData->Argument[4];
	 CreateProcess(name, start_address, priority, PRO_INFO->current, process_id, return_error);
      break;

    case SYSNUM_SUSPEND_PROCESS:

      printf("\n\nSuspend Process\n\n");
      PID = (long)SystemCallData->Argument[0];
      return_error = (long *)SystemCallData->Argument[1];
      osSuspendProcess(PID, return_error);

      break;

    case SYSNUM_RESUME_PROCESS:

      printf("\n\nResume Process\n\n");
      PID = (long)SystemCallData->Argument[0];
      return_error = (long *)SystemCallData->Argument[1];
      osResumeProcess(PID, return_error);

      break;
     
       
    default:
      aprintf("\n\n\nNo SysCall number!!!\n\n\n");
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

    //test print
    ProcessesState();
    
    //Timer Queue
    if(CreateTimerQueue() == -1){
      printf("\n\nUnable to create Timer Queue!\n\n");
    }
    //Disk Queues
    for(int i=0; i<MAX_NUMBER_OF_DISKS; i++){
      CreateDiskQueue(i);
    }

    //Initialize Disk Queue Locks
    InitializeDiskLocks();
    
    //Ready Queue
    if(CreateReadyQueue() == -1){
      printf("\n\nUnable to create Ready Queue!\n\n");
    }

    //Suspended Queue
    if(CreateSuspendQueue() == -1){
      printf("\n\nUnable to create Suspended Queue!\n\n");
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
	  ProcessesState();

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

  if(strcmp("sample", test_name) == 0)
    return (long)(SampleCode);

  if(strcmp("test0", test_name) == 0)
    return (long)(test0);
     
  if(strcmp("test1", test_name) == 0)
    return (long)(test1);
    
  if(strcmp("test2", test_name) == 0)
    return (long)(test2);
  
  if(strcmp("test3", test_name) == 0)
    return (long)(test3);
  
  if(strcmp("test4", test_name) == 0)
    return (long)(test4);

  if(strcmp("test5", test_name) == 0)
    return (long)(test5);
     
  if(strcmp("test6", test_name) == 0)
    return (long)(test6);
    
  if(strcmp("test7", test_name) == 0)
    return (long)(test7);
  
  if(strcmp("test8", test_name) == 0)
    return (long)(test8);
  
  if(strcmp("test9", test_name) == 0)
    return (long)(test9);
  
  if(strcmp("test10", test_name) == 0)
    return (long)(test10);
  
  if(strcmp("test11", test_name) == 0)
    return (long)(test11);
  
  if(strcmp("test12", test_name) == 0)
    return (long)(test12);
  
  return 0;
}
