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

DISK_CACHE* CreateDiskCache(){

  DISK_CACHE *Cache = malloc(sizeof(DISK_CACHE));
  if(Cache == NULL){
    aprintf("\n\nError in allocating for Disk Cache\n\n");
  }

  for(INT32 i=0; i<2048; i++){
    for(INT32 j=0; j<PGSIZE; j++){
      Cache->Block[i].Byte[j] = 0;
    }
  }
  for(INT32 i=0 ; i<2048; i++){
    Cache->Modified[i] = 0;
  }
  return Cache;
}


void SetMagicNumber(DISK_BLOCK *Sector0, unsigned char MagicNum){

  Sector0->Byte[0] = MagicNum;
}

void SetDiskID(DISK_BLOCK *Sector0, unsigned char DiskID){

  Sector0->Byte[1] = DiskID;
}

void SetDiskLength(DISK_BLOCK *Sector0, INT16 DiskLength){

  //Set LSB. Mask Upper Bits.
  Sector0->Byte[2] = 0x00FF & DiskLength;
  //Set MSB
  Sector0->Byte[3] = (DiskLength>>8);
}

void SetBitMapField(DISK_BLOCK *Sector0, unsigned char BitMap){

  Sector0->Byte[4] = BitMap;
}

void SetSwapField(DISK_BLOCK *Sector0, unsigned char SwapLength){

  Sector0->Byte[5] = SwapLength;
}

void SetBitMapLocation(DISK_BLOCK *Sector0, INT16 Location){

  //Set LSB. Mask Upper Bits.
  Sector0->Byte[6] = 0x00FF & Location;
  //Set MSB
  Sector0->Byte[7] = (Location>>8);
}

void SetRootLocation(DISK_BLOCK *Sector0, INT16 Location){

  //Set LSB. Mask Upper Bits.
  Sector0->Byte[8] = 0x00FF & Location;
  //Set MSB
  Sector0->Byte[9] = (Location>>8);
}

void SetSwapLocation(DISK_BLOCK *Sector0, INT16 Location){

  //Set LSB. Mask Upper Bits.
  Sector0->Byte[10] = 0x00FF & Location;
  //Set MSB
  Sector0->Byte[11] = (Location>>8);
}

void SetRevision(DISK_BLOCK *Sector0, unsigned char Revision){

  Sector0->Byte[15] = Revision;
}


void SetInode(DISK_BLOCK *Header, unsigned char Inode){

  Header->Byte[0] = Inode;
}

void SetFileName(DISK_BLOCK *Header, char *FileName){

  INT32 i = 0;
  while(FileName[i] != '\0'){
    Header->Byte[i+1] = FileName[i];
    i++;
  }
  //Fill up the rest of the File Name Field with 0's
  for( ; i<7; i++){
    Header->Byte[i+1] = '\0';
  }
}

void SetCreationTime(DISK_BLOCK *Header, INT32 Time){

  Header->Byte[8] = (0x000000FF) & Time;
  Time = Time>>8;
  Header->Byte[9] = (0x000000FF) & Time;
  Time = Time>>8;
  Header->Byte[10] = (0x000000FF) & Time;
}

#define FILE 0
#define DIR 1

void SetFileOrDirectory(DISK_BLOCK *Header, INT32 Type){

  if(Type == FILE){
    Header->Byte[11] &= 0xFE;
  }
  else{ //Type == DIR
    Header->Byte[11] |= 0x01;
  }
  aprintf("Here is the value after setting the dir %x\n", Header->Byte[11]);
}

/*
00000XX0 holds the file level.
0,1,2,3 are options.
*/
void SetFileLevel(DISK_BLOCK *Header, unsigned char Level){

  Level = Level<<1;
  unsigned char Mask = 0xF9;
  Header->Byte[11] = (Mask & Header->Byte[11]) + Level;
  aprintf("after set LEvel %x\n", Header->Byte[11]);
}

void SetParentInode(DISK_BLOCK *Header, unsigned char Parent){

  Parent = Parent<<3;
  unsigned char Mask = 0x07;
  Header->Byte[11] = (Mask & Header->Byte[11]) + Parent;
    aprintf("after setting parent %x\n", Header->Byte[11]);
}

void SetIndexLocation(DISK_BLOCK *Header, INT16 Location){

  //Set LSB. Mask Upper Bits.
  Header->Byte[12] = 0x00FF & Location;
  //Set MSB
  Header->Byte[13] = (Location>>8);
}

void SetFileLength(DISK_BLOCK *Header, INT16 Length){

  //Set LSB. Mask Upper Bits.
  Header->Byte[14] = 0x00FF & Length;
  //Set MSB
  Header->Byte[15] = (Length>>8);
}

/*
Create a sub index at disk block given by SubIndexLocation.
Record this creation in the Parent Index at position 0 to 8
*/
void CreateSubIndex(DISK_BLOCK *Index, INT16 SubIndexLocation,
		    unsigned int PositionInIndex){
  
  Index->Byte[PositionInIndex*2] = SubIndexLocation>>8;
  Index->Byte[PositionInIndex*2+1] = SubIndexLocation;
}
  




/*
Copy the contents of a disk sector into the header struct.
*/

void CopyDiskHeader(DISK_BLOCK *Block, FILE_HEADER *Header){

  Header->inode = Block->Byte[0];

  //pretty ugly
  strncpy(Header->name, (char *)&Block->Byte[1], 7);

  Header->creation_time2 = Block->Byte[8];
  Header->creation_time1 = Block->Byte[9];
  Header->creation_time0 = Block->Byte[10];

  Header->file_description = Block->Byte[11];
  Header->index_location = Block->Byte[12] +
    (Block->Byte[13]<<8);
  Header->file_size = Block->Byte[14] +
    (Block->Byte[15]<<8);
   
}

void SetBit(unsigned char *CharPtr, INT16 position){
  (*CharPtr) |= (0x80 >> position);
}


void SetBitInBitMap(DISK_CACHE* Cache, INT16 SectorNumber){
  
  INT16 BitMapStartSector = 1; //eventually set this. Not hard code

  //Find the Row
  INT16 BitRow = SectorNumber/128;

  SectorNumber = SectorNumber - BitRow*128;

  BitRow = BitRow + BitMapStartSector;

  //Find the Column
  INT16 BitColumn = SectorNumber/8;

  SectorNumber = SectorNumber - BitColumn*8;

  SetBit(&Cache->Block[BitRow].Byte[BitColumn], SectorNumber);

  //Set Modified Flag so OS knows to write back to disk
  Cache->Modified[BitRow] = 1;
}

  

  
  


void InitializeBitStruct(BIT_MAP *Map, long DiskID, long StartAddress){

  Map->StartAddress = StartAddress;
  Map->DiskID = DiskID;
  
  for(INT32 i=0; i<BIT_MAP_LENGTH; i++){
    Map->flags[i] = 0;
    Map->block[i] = NULL;
  }
}

/*
The Disk Sector is a part of the Bit Map. Look through it to find the first 
open spot and return the value. Return -1 if none exist.
*/
INT32 CheckBitMapRow(DISK_BLOCK *BitMapRow){

  for(INT32 i=0; i<PGSIZE; i++){
    if(BitMapRow->Byte[i] != 0xFF){


      //return ;
    }
  }
  return -1;
}



/*
Passed the address of the BIT_MAP structure. Step through it to find 
the next available sector.
We need to fill the Bit Map structure with Disk Reads as necessary.
Set the flag to indicate a change has been made in the bitmap sectors.
*/
INT32 GetAvailableBlock(BIT_MAP *Map){

  INT32 i = Map->StartAddress;
  
  
    if(Map->block[i] == NULL){
      Map->block[i] = malloc(sizeof(DISK_BLOCK));
      osDiskReadRequest(Map->DiskID, i, (long)Map->block[i]);
    }

  

    return 0;
}

/*
Return the position of the first bit to be 0. Work from left to right.
Note that 1101 1111 would return 2 as the second bit from the left is not set.
*/
INT32 TestBit(unsigned char Byte){

  INT32 position = 0;
  
  for(INT32 i=7; i>=0; i--){

    if((Byte>>i) == 0){
      return position;
    }
    position++;
  }
  aprintf("\n\nError: All Bits Set\n\n");
  return -1;
}



void InitializeBitMap(unsigned char BitArray[16][16]){
  for(int i=0; i<16; i++){
    for(int j=0; j<16; j++){
      BitArray[i][j] = 0;
    }
  }
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
  PROCESS_CONTROL_BLOCK *CurrentPCB = GetCurrentPCB();

  DISK_CACHE *Cache = CreateDiskCache();
  CurrentPCB->cache = Cache;

  DISK_BLOCK *Sector0 = &Cache->Block[0];

  SetMagicNumber(Sector0, 0x5A);
  SetDiskID(Sector0, DiskID);
  SetDiskLength(Sector0, 0x0800);
  SetBitMapField(Sector0, 0x04);
  SetSwapField(Sector0, 0x80);
  SetBitMapLocation(Sector0, 0x0001);
  SetRootLocation(Sector0, 0x0011);
  SetSwapLocation(Sector0, 0x0600);
  SetRevision(Sector0, 0x2E);

  Cache->Modified[0] = 1;
  
  DISK_BLOCK *RootHeader = &(Cache->Block[0x11]);
  SetInode(RootHeader, 0);  //eventually need a better way to do this
  char* DirName = "root";
  SetFileName(RootHeader, DirName);
  SetCreationTime(RootHeader, 0x123);
  SetFileOrDirectory(RootHeader, DIR);
  SetFileLevel(RootHeader, 2);
  SetParentInode(RootHeader, 0x1F);
  SetIndexLocation(RootHeader, 0x0012);
  SetFileLength(RootHeader, 0);

  Cache->Modified[0x11] = 1;
  
  DISK_BLOCK *RootIndex = &Cache->Block[0x12];

  INT16 UnusedSector = 0x13;

  for(int i=0; i<8; i++){
    CreateSubIndex(RootIndex, UnusedSector, i);
    UnusedSector++;
  }

  Cache->Modified[0x12] = 1;
  
  //set the parts of the bitmap that are in use
  for(INT16 i=0; i<=0x12; i++){
    SetBitInBitMap(Cache, i);
  }

  for(INT16 i=0x0600; i<= 0x7FF; i++){
    SetBitInBitMap(Cache, i);
  }
  
  //Atomic Section
  LockLocation(DISK_LOCK[DiskID]);
  
  
  DQ_ELEMENT *dqe;

  for(INT16 i=0; i<0x0600; i++){
    if(Cache->Modified[i] == 1){
      
      dqe = CreateDiskQueueElement(DiskID, i, (long)(&Cache->Block[i]),
				   CurrentContext, CurrentPID, CurrentPCB);
      AddToDiskQueue(DiskID, dqe);

      Cache->Modified[i] = 0;
    }
  }
    /*
  
  //Write the Root Directory Header
  dqe = CreateDiskQueueElement(DiskID, 0x11, (long)(RootHeader),
			       CurrentContext, CurrentPID, CurrentPCB);
  AddToDiskQueue(DiskID, dqe);

  //Lets hold off on this for now. Probably easier if just keep 0's in
  //index.  Write the Root Directory Index dqe =
  dqe = CreateDiskQueueElement(DiskID, 0x12, (long)(&RootIndex),
			 CurrentContext, CurrentPID, CurrentPCB);
  AddToDiskQueue(DiskID, dqe);
  

  for(INT32 i=0; i<16; i++){
    dqe = CreateDiskQueueElement(DiskID, i,
				 (long)(&Cache->Block[i]),
				 CurrentContext, CurrentPID,
				 CurrentPCB);
    AddToDiskQueue(DiskID, dqe);
  }
*/
  
  StartDiskWrite(DiskID);

  UnlockLocation(DISK_LOCK[DiskID]);
  dispatcher();

  (*ReturnError) = ERR_SUCCESS;
  
}

/*
Still have to handle cases other than root dir
Need to work on returning the error message
*/
void osOpenDirectory(long DiskID, char *FileName, long *ReturnError){


  PROCESS_CONTROL_BLOCK *pcb = GetCurrentPCB();
  
  if(strcmp(FileName, "root") == 0){
    aprintf("Opening the root directory of Disk %d\n", DiskID);
    //need to read in sector 0
    DISK_BLOCK Block0;
    osDiskReadRequest(DiskID, 0, (long) &Block0);

    for(int i=0; i<PGSIZE; i++){
      // aprintf("%x ", Block0.Byte[i]);
    }
    //now have to get the root header
    long RootHeaderLocation = (Block0.Byte[9]<<8) + Block0.Byte[8];
   
    DISK_BLOCK RootHeader;
    osDiskReadRequest(DiskID, RootHeaderLocation, (long) &RootHeader);
    for(int i=0; i<PGSIZE; i++){
      // aprintf("%x ", RootHeader.Byte[i]);
    }

    CopyDiskHeader(&RootHeader, &pcb->current_directory);
    pcb->current_disk = DiskID;
    
  } //end root
}

//Disk Sector has already been determined to be available.
INT32 GetSpot1(long DiskID, DISK_INDEX *Index, short DiskSector, short CurrentDiskSector){
 
  aprintf("At level 1 of Dir\n");
    

  for(INT32 i=0; i<PGSIZE/2; i++){
    if(Index->address[i] == 0){
      aprintf("The %x slot is available\nCode to write the header to Disk Sector %x should be inserted here\n", i , DiskSector);

      Index->address[i] = DiskSector;
      //update file hierarchy on disk

      long CurrentPID = GetCurrentPID();
      long CurrentContext = osGetCurrentContext();
      void* CurrentPCB = GetCurrentPCB();
      DQ_ELEMENT *dqe;
      dqe = CreateDiskQueueElement(DiskID, CurrentDiskSector, (long)Index,
				   CurrentContext, CurrentPID, CurrentPCB);
      AddToDiskQueue(DiskID, dqe);
      return TRUE;
      
    }
  }

  return FALSE;

}

//Disk Sector has already been determined to be available.
INT32 GetSpot2(long DiskID, DISK_INDEX *Index, short DiskSector, short CurrentDiskSector){
 
  aprintf("At level 2 of Dir\n");
    

  for(INT32 i=0; i<PGSIZE/2; i++){
    if(Index->address[i] == 0){
      aprintf("The %x slot is available\nWe need to create a level 1 index\n", i , DiskSector);

      Index->address[i] = DiskSector;
      //update file hierarchy on disk

     
      return TRUE;
      
    }
  }

  return FALSE;

}

/*
Need to check for errors etc
Enough Space...
Name already exists....
Name too long or contains illegal characters...
Lots of work...
*/

void osCreateDirectory(char *FileName, long *ReturnError){

  aprintf("Create a Directory with %s name\n", FileName);
  
  //first read in sector 0
  PROCESS_CONTROL_BLOCK *pcb = GetCurrentPCB();
  // long CurrentContext = osGetCurrentContext();
  // long CurrentPID = GetCurrentPID();
  long DiskID = pcb->current_disk;

  DISK_BLOCK Sector0;

  //not sure about the lock situation here
  osDiskReadRequest(DiskID, 0, (long)(&Sector0));

  /*
  DiskSector[4] = 0x04; //Bitmap Size is 0x10. 4*4
  DiskSector[6] = 0x01; //LSB of Bitmap Location
  DiskSector[7] = 0x00; //MSB of Bitmap Location
  */

  //maybe dont have to get length. All should be the same - 16 blocks
  short BitmapLength = (Sector0.Byte[4])*4; //have to mult by 4
  short BitmapLocation = (Sector0.Byte[7]<<8) + Sector0.Byte[6];

  aprintf("Here is the bitmap location and size\n %d %x\n", BitmapLocation, BitmapLength);

  BIT_MAP Map;
  InitializeBitStruct(&Map, DiskID, BitmapLocation);

  //read in bitmap address until find two blocks available
  INT32 NewHeaderSector = GetAvailableBlock(&Map);
  INT32 NewIndexSector = GetAvailableBlock(&Map);

  //create the dir header and index. Mark Bitmap as changed.. Write? Don't Write?

  //Now have to traverse the directory structure and find a place to store a pointer to the new header.

  //Might need to create indices as we go....

  //set error and return.
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
