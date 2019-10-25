/*
readyQueue.c

This file holds the functions that deal with the Ready Queue. It also
includes the dispatcher. 
*/

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "protos.h"
#include "process.h"
#include "readyQueue.h"
#include "timerQueue.h" 
#include "osGlobals.h"
#include "osSchedulePrinter.h"
#include "diskManagement.h"
#include "diskQueue.h"

void SetBit(unsigned char *CharPtr, INT32 position){
  (*CharPtr) |= (0x80 >> position);
}

void InitializeBitMap(unsigned char BitArray[16][16]){
  for(int i=0; i<16; i++){
    for(int j=0; j<16; j++){
      BitArray[i][j] = 0;
    }
  }
}

void SetBitInBitMap(INT32 DiskSector, unsigned char BitArray[16][16],
		    INT32 BitModified[16]){

  INT32 row = DiskSector/128;
  DiskSector = DiskSector - row*128;

  BitModified[row] = TRUE;

  INT32 column = DiskSector/8;
  DiskSector = DiskSector - column*8;

  SetBit(&BitArray[row][column], DiskSector);
}

/*
Right now assumes only writing to disk
*/
DQ_ELEMENT* CreateDiskQueueElement(long DiskID, long Sector,
				   long Address, long Context, long PID,
				   void *PCB){

  DQ_ELEMENT *dqe = malloc(sizeof(DQ_ELEMENT));
  dqe->disk_action = WRITE_DISK;
  dqe->disk_id = DiskID;
  dqe->disk_sector = Sector;
  dqe->disk_address = Address;
  dqe->context = Context;
  dqe->PID = PID;
  dqe->PCB = PCB;			   

  return dqe;
}


void osFormatDisk(long DiskID, long *ReturnError){

  if(DiskID < 0 || DiskID >= MAX_NUMBER_OF_DISKS){
    aprintf("\n\nDisk Error is not in the proper range\n\n");
  }
  aprintf("\n\nHere in Format Disk\n\n");

  //get the Current Process information
  long CurrentPID = GetCurrentPID();
  long CurrentContext = osGetCurrentContext();
  void* CurrentPCB = GetCurrentPCB();

  
  //Practice with setting bits
  //assuming chars are 8 bits.. If not we will have problems
  unsigned char BitArray[16][16];

  INT32 BitModified[16];
  for(INT32 i=0; i<0x16; i++){
    BitModified[i] = FALSE;
  }
  
  InitializeBitMap(BitArray);

  
  unsigned char DiskSector[16];
  DiskSector[0] = 0x5A; //Magic Number 'Z'
  DiskSector[1] = DiskID; //set DiskID
  //set Bitmap Size to 12. No real reason why.
  DiskSector[2] = 0x00;  //LSB of Disk Length
  //set root dir header size to 1
  DiskSector[3] = 0x08;  //MSB of Disk Length
  //set swap space to 8. No real reason why.
  DiskSector[4] = 0x04; //Bitmap Size is 0x10. 4*4
  DiskSector[5] = 0x80; //Set Swap Size to 0x200
  DiskSector[6] = 0x01; //LSB of Bitmap Location
  DiskSector[7] = 0x00; //MSB of Bitmap Location
  DiskSector[8] = 0x11; //LSB of Root Dir Location. Start after Bitmap
  DiskSector[9] = 0x00;  //MSB of Root Dir Location.
  DiskSector[10] = 0x00; //LSB of Swap Space. Starts after Root Dir Loc
  DiskSector[11] = 0x06; //MSB of Swap Space
  DiskSector[12] = 0x00; //Reserved
  DiskSector[13] = 0x00; //Reserved
  DiskSector[14] = 0x00; //Reserved
  DiskSector[15] = 0x2E; //46 for rev 4.6

  //Create the root directory
  unsigned char RootHeader[16];
  RootHeader[0] = 0; //set inode to 0
  //eventually need a way to track and give out available
  RootHeader[1] = 'r'; //7 character name
  RootHeader[2] = 'o';
  RootHeader[3] = 'o';
  RootHeader[4] = 't';
  RootHeader[5] = '\0';
  RootHeader[6] = '\0';  
  RootHeader[7] = '\0';
  RootHeader[8] = 1;  //set the time. Supposedly doesnt matter
  RootHeader[9] = 2;
  RootHeader[10] = 3;
  RootHeader[11] = 0xFD; //File Description
  RootHeader[12] = 0x12; //LSB of File Header Location
  RootHeader[13] = 0x00; //MSB of File Header Location
  RootHeader[14] = 0; //Length of File. Should be 0?
  RootHeader[15] = 0;

  //Now do the Root Header Index
  unsigned char RootIndex[16];
  RootIndex[0] = 0x13;;
  RootIndex[1] = 0; 
  RootIndex[2] = 0x14;
  RootIndex[3] = 0;
  RootIndex[4] = 0x15;
  RootIndex[5] = 0;
  RootIndex[6] = 0x16;  
  RootIndex[7] = 0;
  RootIndex[8] = 0x17;  //set the time. Supposedly doesnt matter
  RootIndex[9] = 0;
  RootIndex[10] = 0x18;
  RootIndex[11] = 0; //File Description
  RootIndex[12] = 0x19; //LSB of File Header Location
  RootIndex[13] = 0; //MSB of File Header Location
  RootIndex[14] = 0x1A; //Length of File. Should be 0?
  RootIndex[15] = 0;

  
  
  
  //set the parts of the bitmap that are in use
  for(INT32 i=0; i<=0x1A; i++){
    SetBitInBitMap(i, BitArray, BitModified);
  }

  for(INT32 i=0x0600; i<= 0x7FF; i++){
    SetBitInBitMap(i, BitArray, BitModified);
  }
  
  for(INT32 i=0; i<16; i++){
    for(INT32 j=0; j<16; j++){
      printf("%x ", BitArray[i][j]);
    }
    printf("\n");
  }

  //Atomic Section
  LockLocation(DISK_LOCK[DiskID]);
  
  
  DQ_ELEMENT *dqe;
  dqe = CreateDiskQueueElement(DiskID, 0, (long)DiskSector,
			       CurrentContext, CurrentPID, CurrentPCB);
  AddToDiskQueue(DiskID, dqe);

  //Write the Root Directory Header
  dqe = CreateDiskQueueElement(DiskID, 0x12, (long)RootHeader,
			       CurrentContext, CurrentPID, CurrentPCB);
  AddToDiskQueue(DiskID, dqe);

  //Write the Root Directory Index
    dqe = CreateDiskQueueElement(DiskID, 0x13, (long)RootIndex,
			       CurrentContext, CurrentPID, CurrentPCB);
  AddToDiskQueue(DiskID, dqe);
  

  for(INT32 i=0; i<16; i++){
    if(BitModified[i] == TRUE){
        dqe = CreateDiskQueueElement(DiskID, i+1,
				    (long)(&BitArray[i][0]),
				     CurrentContext, CurrentPID,
				     CurrentPCB);
	AddToDiskQueue(DiskID, dqe);
	BitModified[i] = FALSE;
    }
  }

  
  StartDiskWrite(DiskID);

  UnlockLocation(DISK_LOCK[DiskID]);
  dispatcher();

  (*ReturnError) = ERR_SUCCESS;
  
}
/*

void AddToDiskQueue(long DiskID, DQ_ELEMENT *dqe);
 typedef struct{

  long disk_action;  
  long disk_id;
  long disk_sector;
  long disk_address;
  
  long context;
  long PID;
  void* PCB;
  }DQ_ELEMENT;
*/
