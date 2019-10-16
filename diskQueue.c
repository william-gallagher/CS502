#include "timerQueue.h" //for locks
#include "readyQueue.h" //for dispatcher
#include "diskQueue.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "protos.h"
#include "process.h"
#include "os_globals.h"

int CreateDiskQueue(int disk_number){
  char num[2];
  snprintf((char *)(&num), 2, "%d", disk_number);
  char queue_name[8];
  strcpy(queue_name, "DQUEUE_");
  strcat(queue_name, num);

  disk_queue[disk_number] = QCreate(queue_name);
  return 1;
}

void InitializeDiskLocks(){

  for(int i=0; i<MAX_NUMBER_OF_DISKS; i++){
    DISK_LOCK[i] = MEMORY_INTERLOCK_BASE + 2 + i;  }
}

/*
Receives the id of a disk and checks to see if the disk is busy performing I/O operations. Returns DEVICE_IN_USE or DEVICE_FREE in address pointed to by status.
*/
void CheckDiskStatus(long disk_id, long* status){

  MEMORY_MAPPED_IO mmio;
  mmio.Mode = Z502Status;
  mmio.Field1 = disk_id;
  mmio.Field2 = mmio.Field3 = mmio.Field4 = 0;

  MEM_READ(Z502Disk, &mmio);

  (*status) = mmio.Field2;
}

/*
Adds to Disk Queue. head_or_tail indicates whether to add to the head or tail of the Disk Queue.
*/
void AddToDiskQueue(long DiskID, DQ_ELEMENT* dqe){

  LockLocation(DISK_LOCK[DiskID]);

  QInsertOnTail(disk_queue[DiskID], (void *) dqe);
  
   UnlockLocation(DISK_LOCK[DiskID]);
  
}

/*
Remove a DQ_ELEMENT from the head of the Ready Queue. Return the DQ_ELEMENT
*/
DQ_ELEMENT* RemoveFromDiskQueueHead(long DiskID){

  LockLocation(DISK_LOCK[DiskID]);
  
  DQ_ELEMENT* dqe = (DQ_ELEMENT *)QRemoveHead(disk_queue[DiskID]);
  UnlockLocation(DISK_LOCK[DiskID]);

  return dqe;
}

/*
Checks the Disk Queue for the existance of another element. Does not remove the element. Returns -1 if no element exits
*/
DQ_ELEMENT* CheckDiskQueue(long DiskID){

  LockLocation(DISK_LOCK[DiskID]);
  
  DQ_ELEMENT* dqe = (DQ_ELEMENT *)QWalk(disk_queue[DiskID], 0);

  UnlockLocation(DISK_LOCK[DiskID]);

  return dqe;
}

void osDiskReadRequest(long DiskID, long DiskSector, long DiskAddress){

  MEMORY_MAPPED_IO mmio;
  PROCESS_CONTROL_BLOCK* curr_proc = GetCurrentPCB();
  DQ_ELEMENT *dq;
      
  //create struct for disk queue
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

  if(status == DEVICE_FREE){
 
    mmio.Mode = Z502DiskRead;
    mmio.Field1 = DiskID;
    mmio.Field2 = DiskSector;
    mmio.Field3 = DiskAddress;
	
    MEM_WRITE(Z502Disk, &mmio);
     
  }

  AddToDiskQueue(DiskID, dq);
      
  //set process state to DISK
  ChangeProcessState(dq->PID, DISK);
  
  dispatcher();
     
}

void osDiskWriteRequest(long DiskID, long DiskSector, long DiskAddress){
      
  MEMORY_MAPPED_IO mmio;
  PROCESS_CONTROL_BLOCK* curr_proc = (PROCESS_CONTROL_BLOCK *)GetCurrentPCB();
  DQ_ELEMENT *dq;
  long status;
  
  //create struct for disk queue
  dq = malloc(sizeof(DQ_ELEMENT));
  dq->context = curr_proc->context;
  dq->PID = curr_proc->idnum;
  dq->PCB = curr_proc;
      
  // curr_proc = &PRO_INFO->PCB[PRO_INFO->current];

  CheckDiskStatus(DiskID, &status);

  if(status == DEVICE_FREE){

    //this is where to store the information about the process
    disk_process[DiskID] = dq;
	
    mmio.Mode = Z502DiskWrite;
    mmio.Field1 = DiskID;
    mmio.Field2 = DiskSector;
    mmio.Field3 = DiskAddress;
	
    MEM_WRITE(Z502Disk, &mmio);
     
  }
  //  else{
	
    dq->disk_id = DiskID;
    dq->disk_sector = DiskSector;
    dq->disk_address = DiskAddress;
    AddToDiskQueue(DiskID, dq);
    //  }
    
  dispatcher();

}

void osCheckDiskRequest(long DiskID, long DiskSector){
  //curr_proc = &PRO_INFO->PCB[PRO_INFO->current];
  long Status;
  MEMORY_MAPPED_IO mmio;
    
  CheckDiskStatus(DiskID, &Status);

  if(Status == DEVICE_FREE){
	
    mmio.Mode = Z502CheckDisk;
    mmio.Field1 = DiskID;
    mmio.Field2 = mmio.Field3 = mmio.Field4 = 0;
	
    MEM_WRITE(Z502Disk, &mmio);
     
  }
  else{

    aprintf("\n\nDisk %ld is not free. Cannot Check Disk\n\n", DiskID);
  }
}
