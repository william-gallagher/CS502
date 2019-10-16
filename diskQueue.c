/*
diskQueue.c

This file holds the functions that deal with the Disk Queues. Note that there is an individual disk queue for each disk. This file also includeshandling interrupts due to the disks.
*/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "protos.h"
#include "process.h"
#include "osGlobals.h"
#include "timerQueue.h" 
#include "readyQueue.h"
#include "diskQueue.h"

/*
This function creates the Queue for storing the processes that are using
or waiting to use the disk given by DiskNumber.
The first step is creating a unique name for the Disk Queue. This is 
DQUEUE_1 , DQUEUE_2 etc
*/
void CreateDiskQueue(INT32 DiskNumber){

  //Create String "DQUEUE_" + DiskNumber
  //This serves as the Queue Name
  char num[2];
  snprintf((char *)(&num), 2, "%d", DiskNumber);
  char queue_name[8];
  strcpy(queue_name, "DQUEUE_");
  strcat(queue_name, num);

  disk_queue[DiskNumber] = QCreate(queue_name);
  if(disk_queue[DiskNumber] == -1){
    aprintf("\n\nUnable to create Disk Queue!\n\n");
  }    
}

/*
This function initializes a lock for each individual Disk Queue.
Start with the DISK_LOCK_BASE and work up.
*/
void InitializeDiskLocks(){

  for(int i=0; i<MAX_NUMBER_OF_DISKS; i++){
    DISK_LOCK[i] = DISK_LOCK_BASE + i;  }
}

/*
Receives the id of a disk and checks to see if the disk is busy 
performing I/O operations. Returns DEVICE_IN_USE or DEVICE_FREE in 
address pointed to by status.
*/
void CheckDiskStatus(long DiskID, long* Status){

  MEMORY_MAPPED_IO mmio;
  mmio.Mode = Z502Status;
  mmio.Field1 = DiskID;
  mmio.Field2 = mmio.Field3 = mmio.Field4 = 0;

  MEM_READ(Z502Disk, &mmio);

  (*Status) = mmio.Field2;
}

/*
This function adds the DQ_ELEMENT to the Disk Queue given by DiskID. All
elements are added to the tail of the Disk Queue. This allows them to be
handled in the order that the requests are made.
*/
void AddToDiskQueue(long DiskID, DQ_ELEMENT *dqe){

  LockLocation(DISK_LOCK[DiskID]);
  QInsertOnTail(disk_queue[DiskID], (void *) dqe);
  UnlockLocation(DISK_LOCK[DiskID]);
}

/*
This function remove a DQ_ELEMENT from the head of the Disk Queue given
by DiskID. A pointer to the removed DQ_ELEMENT is returned.
*/
DQ_ELEMENT* RemoveFromDiskQueueHead(long DiskID){

  LockLocation(DISK_LOCK[DiskID]);
  DQ_ELEMENT* dqe = (DQ_ELEMENT *)QRemoveHead(disk_queue[DiskID]);
  UnlockLocation(DISK_LOCK[DiskID]);

  return dqe;
}

/*
Checks the Disk Queue given by DiskID for the existance of element.
This function does not remove the element if it exist. Returns -1 if no 
element exits
*/
DQ_ELEMENT* CheckDiskQueue(long DiskID){

  LockLocation(DISK_LOCK[DiskID]);
  DQ_ELEMENT* dqe = (DQ_ELEMENT *)QWalk(disk_queue[DiskID], 0);
  UnlockLocation(DISK_LOCK[DiskID]);

  return dqe;
}

/*
This function handles the Read Disk Service Call. It creates a
DQ_ELEMENT and adds it to the Disk Queue given by DiskID. If the disk
is not busy the read request is issued immediately. Otherwise the 
request is handled when the disk is free.
*/
void osDiskReadRequest(long DiskID, long DiskSector, long DiskAddress){

  MEMORY_MAPPED_IO mmio;
  PROCESS_CONTROL_BLOCK* curr_proc = GetCurrentPCB();
  DQ_ELEMENT *dq;
      
  //create struct for disk queue and fill with the relevant info
  dq = malloc(sizeof(DQ_ELEMENT));
  dq->context = curr_proc->context;
  dq->PID = curr_proc->idnum;
  dq->PCB = curr_proc;
  dq->disk_id = DiskID;
  dq->disk_sector = DiskSector;
  dq->disk_address = DiskAddress;
  dq->disk_action = READ_DISK;
    
  long status;
  CheckDiskStatus(DiskID, &status);

  //If the disk is free perform the Read.
  if(status == DEVICE_FREE){
 
    mmio.Mode = Z502DiskRead;
    mmio.Field1 = DiskID;
    mmio.Field2 = DiskSector;
    mmio.Field3 = DiskAddress;
    MEM_WRITE(Z502Disk, &mmio);   
  }

  //Add the process to the disk queue whether the disk is busy or not.
  AddToDiskQueue(DiskID, dq);
      
  //set process state to DISK
  ChangeProcessState(dq->PID, DISK);
  
  dispatcher();
}

/*
This function handles the Write Disk Service Call. It creates a
DQ_ELEMENT and adds it to the Disk Queue given by DiskID. If the disk
is not busy the write request is issued immediately. Otherwise the 
request is handled when the disk is free.
*/
void osDiskWriteRequest(long DiskID, long DiskSector, long DiskAddress){
      
  MEMORY_MAPPED_IO mmio;
  PROCESS_CONTROL_BLOCK* curr_proc = GetCurrentPCB();
  DQ_ELEMENT *dq;
  
  //create struct for disk queue and fill with the relevant info
  dq = malloc(sizeof(DQ_ELEMENT));
  dq->context = curr_proc->context;
  dq->PID = curr_proc->idnum;
  dq->PCB = curr_proc;
  dq->disk_id = DiskID;
  dq->disk_sector = DiskSector;
  dq->disk_address = DiskAddress;
  dq->disk_action = WRITE_DISK;
      
  long status;
  CheckDiskStatus(DiskID, &status);

  if(status == DEVICE_FREE){

    mmio.Mode = Z502DiskWrite;
    mmio.Field1 = DiskID;
    mmio.Field2 = DiskSector;
    mmio.Field3 = DiskAddress;
	
    MEM_WRITE(Z502Disk, &mmio);
  }

  //Add the process to the disk queue whether the disk is busy or not.
  AddToDiskQueue(DiskID, dq);
  
  dispatcher();
}

/*
This function uses the CheckDisk primitive to see the contents of a
disk. Note that it does not seem to cause an interrupt. It just gets 
the contents and returns.
*/
void osCheckDiskRequest(long DiskID, long DiskSector){

  long Status;
  MEMORY_MAPPED_IO mmio;
    
  CheckDiskStatus(DiskID, &Status);

  //If disk is free proceed with the Check
  if(Status == DEVICE_FREE){
	
    mmio.Mode = Z502CheckDisk;
    mmio.Field1 = DiskID;
    mmio.Field2 = mmio.Field3 = mmio.Field4 = 0;
	
    MEM_WRITE(Z502Disk, &mmio);
     
  }
  else{
    aprintf("\n\nERROR: Disk %ld is not free. Cannot Check Disk\n\n", DiskID);
  }
}


void HandleDiskInterrupt(long DiskID){

  MEMORY_MAPPED_IO mmio;
  
  DQ_ELEMENT* dqe = RemoveFromDiskQueueHead(DiskID);

  //Add process to Ready Queue to be resumed
  AddToReadyQueue(dqe->context, dqe->PID, dqe->PCB);
  free(dqe);
  
  //We need to check for another element on the disk queue that needs
  //service.
  dqe = CheckDiskQueue(DiskID);
  if((long)dqe != -1){
	
    //Note that we don't need to check if disk is busy.
    //It can't be because we just used it and haven't restarted.

    //Set read or write mode
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
