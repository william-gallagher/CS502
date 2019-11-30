/*
diskManagement.c 

This file holds all the functions that deal with Files and Directories
on the disk.
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

/*
This function initializes an array to track the available and in use
Inodes. 0 indicates Inode is not in use. 
*/
void InitializeInodes(){

  for(INT16 i=0; i<MAX_NUMBER_INODES; i++){
    InodeArray[i] = 0;
  }
}

/*
Find the next available Inode in the Inode Array.
Pointer NewInode is set to the available Inode number.
*/
void GetInode(unsigned char *NewInode){

  for(INT16 i=0; i<MAX_NUMBER_INODES; i++){
    if(InodeArray[i] == 0){
      InodeArray[i] = 1;
      (*NewInode) = i;
      return;
    }
  }
  aprintf("\n\nERROR: All Inodes Allocated\n\n");
  (*NewInode) = -1;
}

/*
Create a disk cache. This is essentially a copy of the disk in memory.
2048 Sectors are created. There is also a Modified array which tracks 
the sectors of the disk that have been modified for writing out to the 
disk at a later time.
*/
DISK_CACHE* CreateDiskCache(){

  DISK_CACHE *Cache = malloc(sizeof(DISK_CACHE));
  if(Cache == NULL){
    aprintf("\n\nError in allocating for Disk Cache\n\n");
  }

  for(INT32 i=0; i<NUMBER_LOGICAL_SECTORS; i++){
    for(INT32 j=0; j<PGSIZE; j++){
      Cache->Block[i].Byte[j] = 0;
    }
  }
  for(INT32 i=0; i<NUMBER_LOGICAL_SECTORS; i++){
    Cache->Modified[i] = 0;
  }
  return Cache;
}

/*
Puts the magic number 5A into the first byte of the first sector.
*/
void SetMagicNumber(DISK_BLOCK *Sector0, unsigned char MagicNum){

  Sector0->Byte[0] = MagicNum;
}

/*
Set the second byte of sector0 to the disk ID.
*/
void SetDiskID(DISK_BLOCK *Sector0, unsigned char DiskID){

  Sector0->Byte[1] = DiskID;
}

/*
Set the disk length of disk sector 0. 
*/
void SetDiskLength(DISK_BLOCK *Sector0, INT16 DiskLength){

  //Set LSB. Mask Upper Bits.
  Sector0->Byte[2] = 0x00FF & DiskLength;
  //Set MSB
  Sector0->Byte[3] = (DiskLength>>8);
}

/*
Use header bytes 14 and 15 to hold the file size.
*/
void SetFileSize(DISK_BLOCK *Header, INT16 FileSize){

  //Set LSB
  Header->Byte[14] = (0x00FF & FileSize);
  //Set MSB
  Header->Byte[15] = FileSize >> 8;
}

/*
This function is given a file header and returns the file size.
*/
void GetFileSize(DISK_BLOCK *Header, INT16 *FileSize){

  (*FileSize) = Header->Byte[14] + (Header->Byte[15] <<8);
}

/*
Sets the length of the bitmap in Sector 0.
*/
void SetBitMapField(DISK_BLOCK *Sector0, unsigned char BitMap){

  Sector0->Byte[4] = BitMap;
}

/*
Sets the length of the Swap field in Disk Sector 0.
*/
void SetSwapField(DISK_BLOCK *Sector0, unsigned char SwapLength){

  Sector0->Byte[5] = SwapLength;
}

/*
Sets the location in Sector 0 where the bitmap can be found.
*/
void SetBitMapLocation(DISK_BLOCK *Sector0, INT16 Location){

  //Set LSB. Mask Upper Bits.
  Sector0->Byte[6] = 0x00FF & Location;
  //Set MSB
  Sector0->Byte[7] = (Location>>8);
}

/*
Sets the location of the Root Directory in Sector 0.
*/
void SetRootLocation(DISK_BLOCK *Sector0, INT16 Location){

  //Set LSB. Mask Upper Bits.
  Sector0->Byte[8] = 0x00FF & Location;
  //Set MSB
  Sector0->Byte[9] = (Location>>8);
}

/*
Sets the where the swap location can be found on the disk.
*/
void SetSwapLocation(DISK_BLOCK *Sector0, INT16 Location){

  //Set LSB. Mask Upper Bits.
  Sector0->Byte[10] = 0x00FF & Location;
  //Set MSB
  Sector0->Byte[11] = (Location>>8);
}

/*
The last byte in Sector 0 corresponds to the revision of the Z502.
This is set here.
*/
void SetRevision(DISK_BLOCK *Sector0, unsigned char Revision){

  Sector0->Byte[15] = Revision;
}

/*
Set the file or directory inode in the file or directory header.
*/
void SetInode(DISK_BLOCK *Header, unsigned char Inode){

  Header->Byte[0] = Inode;
}

/*
Copy the given file name into the bytes reserved for it in the file
header.
*/
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

/*
Copy the File Name stored in bytes 1 to 8 of the Header into the Buffer
*/
void GetFileName(DISK_BLOCK *Header, char *Buffer){

  for(INT16 i=0; i<7; i++){
    Buffer[i] = Header->Byte[i+1];
  }
  Buffer[7] = '\0';
}
  
/*
Set the creation time using the call to the hardware.
Just the last 3 bytes are used.
*/
void SetCreationTime(DISK_BLOCK *Header){

  //get the creation time
  long TimeOfCreation;
  GetTimeOfDay(&TimeOfCreation);

  long mask = 0xFF;
  
  Header->Byte[8] = mask & TimeOfCreation;
  TimeOfCreation = TimeOfCreation >>8;
  Header->Byte[9] = mask & TimeOfCreation;
  TimeOfCreation = TimeOfCreation >>8;
  Header->Byte[10] = mask & TimeOfCreation;
}

/*
Return the creation time from the file or directory header.
*/
void GetCreationTime(DISK_BLOCK *Header, INT32 *Time){

  (*Time) = Header->Byte[10]<<16;
  (*Time) = (*Time) + (Header->Byte[9]<<8);
  (*Time) = (*Time) + (Header->Byte[8]);
}

/*
As the name indicates this function sets the bit in the header which 
indicates if the header corresponds to a file or directory
*/
void SetFileOrDirectory(DISK_BLOCK *Header, INT32 Type){

  if(Type == FILE){
    Header->Byte[11] &= 0xFE;
  }
  else{ //Type == DIR
    Header->Byte[11] |= 0x01;
  }
}

/*
Returns if header corresponds to a file or directory.
*/
void GetFileOrDirectory(DISK_BLOCK *Header, INT32 *Type){

  if((Header->Byte[11] & 0x01) == 0){
    (*Type) = FILE;
  }
  else{
    (*Type) = DIR;
  }
}

/*
Byte 11 in the Header sector holds a number of fields
Bits 00000XX0 holds the file level.
0,1,2,3 are options for the file level.
*/
void SetFileLevel(DISK_BLOCK *Header, unsigned char Level){

  Level = Level<<1;
  unsigned char Mask = 0xF9;
  Header->Byte[11] = (Mask & Header->Byte[11]) + Level;
}

/*
Get the Inode of the parent.
*/
void GetParentInode(DISK_BLOCK *ParentHeader, unsigned char *ParentInode){

  (*ParentInode) = ParentHeader->Byte[0];
}
  
/*
Set the byte that indicates the parent of the file or directory that 
corresponds to the header.
*/
void SetParentInode(DISK_BLOCK *Header, unsigned char Parent){

  Parent = Parent<<3;
  unsigned char Mask = 0x07;
  Header->Byte[11] = (Mask & Header->Byte[11]) + Parent;
}

/*
Set which disk sector holds the first level index of the header.
*/
void SetIndexLocation(DISK_BLOCK *Header, INT16 Location){

  //Set LSB. Mask Upper Bits.
  Header->Byte[12] = 0x00FF & Location;
  //Set MSB
  Header->Byte[13] = (Location>>8);
}

/*
Set the two bits that that indicate the file length in the file header
*/
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
Get the root location of the disk given by the sector 0
*/
void GetRootLocation(DISK_BLOCK *Sector0, INT16 *RootAddress){

  (*RootAddress) = Sector0->Byte[8];
  (*RootAddress) = (Sector0->Byte[9] << 8) + (*RootAddress);
}

/*
Get the location on disk that corresponds to the first level index of
the given file.
*/
void GetHeaderIndexSector(DISK_BLOCK *Header, INT16 *IndexAddress){

  (*IndexAddress) = (Header->Byte[13]<<8) + Header->Byte[12];
}

/*
Search the given file index to find an available spot.
*/
void GetAvailableIndexSpot(DISK_BLOCK *Index, INT16 *Position){

  for(INT16 i=0; i<PGSIZE; i=i+2){

    //Sector 0 is reserved. Any other unused sector will be set to 0
    if(Index->Byte[i] == 0 && Index->Byte[i+1] == 0){
      (*Position) = i;
      return;
    }
  }
  (*Position) = -1;
  return;
}

/*
Check to see if the spot given by Position in the Directory or 
File Index is occupied. Return TRUE if that is the case.
Otherwise return FALSE.
*/
INT32 CheckIndexSpot(DISK_BLOCK *Index, INT16 Position){

  if(Index->Byte[Position] == 0 && Index->Byte[Position+1] == 0){
    return FALSE;
  }
  else{
    return TRUE;
  }
}  

/*
Given an Index find the Disk Sector pointed to be the Position variable
and return it in SubIndexSector.
*/
void GetSubIndex(DISK_BLOCK *Index, INT16 *SubIndexSector,
		 INT16 Position){

  (*SubIndexSector) = (Index->Byte[Position]<<8) +
    Index->Byte[Position+1];
}  

/*
Return the Disk Block that is at the variable Cache
*/
DISK_BLOCK* GetSubDirectoryHeader(DISK_CACHE *Cache, DISK_BLOCK *Index,
			   INT16 Position){

  DISK_BLOCK *SubDirectoryHeader;
  INT16 SubDirectorySector = (Index->Byte[Position]<<8) +
    Index->Byte[Position+1];

  if(SubDirectorySector != 0){
    SubDirectoryHeader = &Cache->Block[SubDirectorySector];
    return SubDirectoryHeader;
  }
  else{
    return NULL;
  }
}

/*
Set the spot in the index to point to a newly created header.
*/
void SetIndexSpot(DISK_BLOCK *Index, INT16 Position, INT16 NewHeader){

  Index->Byte[Position] = NewHeader>>8;
  Index->Byte[Position + 1] = (NewHeader & 0x00FF);
  
}

void SetBit(unsigned char *CharPtr, INT16 position){
  (*CharPtr) |= (0x80 >> position);
}

/*
Indicate that the Sector Number is in use by setting the correct position
in the bitmap.
*/
void SetBitInBitMap(DISK_CACHE* Cache, INT16 SectorNumber){
  
  INT16 BitMapStartSector = 1;

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

/*
Look through the current directory to find the directory given by 
DirName. Otherwise return NULL.
*/
DISK_BLOCK* FindDirectory(DISK_CACHE *Cache, DISK_BLOCK *Index, char *DirName){

  DISK_BLOCK *SubDirectory = NULL;
  char Buffer[8];
  
  for(INT16 i=0; i<PGSIZE; i=i+2){
    
    if(CheckIndexSpot(Index, i) == TRUE){
      
      SubDirectory = GetSubDirectoryHeader(Cache, Index, i);
      GetFileName(SubDirectory, Buffer);
      
      if(strcmp(DirName, Buffer) == 0){
        return SubDirectory;
      }
    }
  }
  return NULL;
}

/*
Return the position of the first bit to be 0. Work from left to right.
Note that 1101 1111 would return 2 as the second bit from the left is
not set.
*/
INT16 TestBit(unsigned char Byte){

  INT16 position = 0;
  unsigned int Mask = 0x80;
  
  for(INT16 i=7; i>=0; i--){
    if((Byte & Mask) == 0){
      return position;
    }
    position++;
    Mask = Mask>>1;
  }
  aprintf("\n\nError: All Bits Set\n\n");
  return -1;
}


/*
Passed the address of the BIT_MAP structure. Step through it to find 
the next available sector.
We need to fill the Bit Map structure with Disk Reads as necessary.
Set the flag to indicate a change has been made in the bitmap sectors.
*/
void GetAvailableSector(DISK_CACHE *Cache, INT16 *AvailableSector){

  INT16 BitMapStart = 1;
  
  for(INT16 i=BitMapStart; i<BitMapStart+0x10; i++){
    for(INT16 j=0; j<16; j++){
      if(Cache->Block[i].Byte[j] != 0xFF){

	INT16 Position = TestBit(Cache->Block[i].Byte[j]);

	(*AvailableSector) = 128*(i-1) + 8*j + Position;
	SetBitInBitMap(Cache, *AvailableSector);
	return;
      }
    }
  }
  //If Nothing is available on disk set return to 0
  (*AvailableSector) = 0;
  return;
}




void InitializeBitMap(unsigned char BitArray[16][16]){
  for(int i=0; i<16; i++){
    for(int j=0; j<16; j++){
      BitArray[i][j] = 0;
    }
  }
}


/*
This creates a disk queue element for writing to the disk.
It does not start a write. This allows for stacking up a bunch of writes
and starting the write all at once.
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

/*
Create the Sector 0 as described in the documentation.
Also create the Root Directory.
Note: Swap at 0x600
Root at 0x11
*/
void osFormatDisk(long DiskID, long *ReturnError){

  if(DiskID < 0 || DiskID >= MAX_NUMBER_OF_DISKS){
    aprintf("\n\nERROR: Disk is not in the proper range\n\n");
  }

  //get the Current Process information
  long CurrentPID = GetCurrentPID();
  long CurrentContext = osGetCurrentContext();
  PROCESS_CONTROL_BLOCK *CurrentPCB = GetCurrentPCB();

  if(Cache == NULL){
    Cache = CreateDiskCache();
  }

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

  unsigned char NewInode;
  GetInode(&NewInode);
  SetInode(RootHeader, NewInode);  //eventually need a better way to do this
  char* DirName = "root";
  SetFileName(RootHeader, DirName);
  SetCreationTime(RootHeader);
  SetFileOrDirectory(RootHeader, DIR);
  SetFileLevel(RootHeader, 1); //Set level to 1. See we ever need 2 lvls
  SetParentInode(RootHeader, 0x1F);
  SetIndexLocation(RootHeader, 0x0012);
  SetFileLength(RootHeader, 0);

  Cache->Modified[0x11] = 1;
  Cache->Modified[0x12] = 1;
  
  //set the parts of the bitmap that are in use
  for(INT16 i=0; i<=0x12; i++){
    SetBitInBitMap(Cache, i);
  }

  //indicate swap area in use
  for(INT16 i=0x0600; i<= 0x7FF; i++){
    SetBitInBitMap(Cache, i);
  }
  
  //Atomic Section
  LockLocation(DISK_LOCK[DiskID]);
  
  DQ_ELEMENT *dqe;

  //Find all the sectors that have been modified and write them to disk
  for(INT16 i=0; i<0x0600; i++){
    if(Cache->Modified[i] == 1){
      
      dqe = CreateDiskQueueElement(DiskID, i, (long)(&Cache->Block[i]),
				   CurrentContext, CurrentPID,
				   CurrentPCB);
      AddToDiskQueue(DiskID, dqe);
      Cache->Modified[i] = 0; //changes wrote to disk
    }
  }
   
  StartDiskWrite(DiskID);

  UnlockLocation(DISK_LOCK[DiskID]);

  dispatcher();

  (*ReturnError) = ERR_SUCCESS;
  
}

/*
This function opens a directory from the current directory.
There are two case: 
1. This directory is the root directory of the disk. In this case Sector 0 is
examined and the root directory is opened.
2. In all other cases the directory to be opened is included in the currently
opened directory. In this case the Sub indices are traversed looking for a
directory that matched the passed FileName.
*/
void osOpenDirectory(long DiskID, char *FileName, long *ReturnError){


  PROCESS_CONTROL_BLOCK *pcb = GetCurrentPCB();
  
  if(strcmp(FileName, "root") == 0){
   
    DISK_BLOCK *Sector0 = &Cache->Block[0];

    INT16 RootLocation;
    GetRootLocation(Sector0, &RootLocation);

    DISK_BLOCK *RootHeader = &Cache->Block[RootLocation];

    pcb->current_directory = RootHeader;
    pcb->current_disk = DiskID;

    (*ReturnError) = ERR_SUCCESS;
    return;
  } //end root

  //All other case except when root is current directory
  
  DISK_BLOCK *Header = pcb->current_directory;
  if(Header == NULL){
    aprintf("osOpenDir has null ptr!!\n");
  }
  DISK_BLOCK *SubDirectory;
  INT16 IndexAddress;

  //Get the Disk Sector where the Header has its top most index
  GetHeaderIndexSector(Header, &IndexAddress);
  
  DISK_BLOCK *Index = &Cache->Block[IndexAddress];
  SubDirectory = FindDirectory(Cache, Index, FileName);

  if(SubDirectory == NULL){
    aprintf("\n\nError: Trying to up NULL directory\n\n");
    (*ReturnError) = ERR_BAD_PARAM;
    return;
    
  }
  pcb->current_directory = SubDirectory;
  (*ReturnError) = ERR_SUCCESS;
  return;
 
}

/*
Write back any disk blocks that have changed to the disk.
*/
void WriteBackChanges(PROCESS_CONTROL_BLOCK *CurrentPCB){

  DQ_ELEMENT *dqe;
  long CurrentContext = osGetCurrentContext();
  long CurrentPID = GetCurrentPID();
  long DiskID = CurrentPCB->current_disk;

 for(INT16 i=0; i<0x0600; i++){
    if(Cache->Modified[i] == 1){

      dqe = CreateDiskQueueElement(DiskID, i, (long)(&Cache->Block[i]),
				   CurrentContext, CurrentPID, CurrentPCB);
      AddToDiskQueue(DiskID, dqe);

      Cache->Modified[i] = 0;
    }
  }
}
  
/*
Create a file with FileName in the currently opened directory of the 
process.
*/
DISK_BLOCK* osCreateFile(char *FileName, long *ReturnError,
			INT32 FileOrDir){
  /*
  if(FileOrDir == DIR)
    aprintf("Create a Directory with %s name\n", FileName);

  if(FileOrDir == FILE)
    aprintf("Create a File with %s name\n", FileName);
  */
  long CurrentPID = GetCurrentPID();
  long CurrentContext = osGetCurrentContext();
  PROCESS_CONTROL_BLOCK *CurrentPCB = GetCurrentPCB();

  long DiskID = CurrentPCB->current_disk;

  DISK_BLOCK *CurrentDirectory = CurrentPCB->current_directory;

  //look in bitmap address until find two blocks available
  INT16 NewHeaderSector;
  INT16 NewIndexSector;

  GetAvailableSector(Cache, &NewHeaderSector);
  GetAvailableSector(Cache, &NewIndexSector);
  
  INT16 IndexSector;
  GetHeaderIndexSector(CurrentDirectory, &IndexSector);

  DISK_BLOCK *Index = &Cache->Block[IndexSector];

  INT16 Position;
  GetAvailableIndexSpot(Index, &Position);
  
  //Set new directory header in Current Directory header
  SetIndexSpot(Index, Position, NewHeaderSector);
  Cache->Modified[IndexSector] = 1;
  
  //Spot for new header
  DISK_BLOCK *NewHeader = &Cache->Block[NewHeaderSector];

  unsigned char NewInode;
  GetInode(&NewInode);
  SetInode(NewHeader, NewInode); 
  SetFileName(NewHeader, FileName);
  SetCreationTime(NewHeader);
  if(FileOrDir == DIR){
    SetFileOrDirectory(NewHeader, DIR);
    SetFileLevel(NewHeader, 1); //default for directory is 1 level
  }
  else{
    SetFileOrDirectory(NewHeader, FILE);
    SetFileLevel(NewHeader, 3); //Default for file is 3 level
  }

  unsigned char ParentInode;
  GetParentInode(CurrentDirectory, &ParentInode);
  
  SetParentInode(NewHeader, ParentInode);
  SetIndexLocation(NewHeader, NewIndexSector);
  SetFileLength(NewHeader, 0);

  Cache->Modified[NewHeaderSector] = 1;

    //Atomic Section
  LockLocation(DISK_LOCK[DiskID]);
  
  WriteBackChanges(CurrentPCB);
   
  StartDiskWrite(DiskID);

  UnlockLocation(DISK_LOCK[DiskID]);

  dispatcher();

  return NewHeader;
}

/*
Looks for the file with FileName in the current directory of the process.
If it does not exist it is created.
The Inode of the opened or created file is returned.
*/
void osOpenFile(char *FileName, long *Inode, long *ReturnError){

  PROCESS_CONTROL_BLOCK *pcb = GetCurrentPCB();

  DISK_BLOCK *Header = pcb->current_directory;

  if(Header == NULL){
    aprintf("\n\nERROR: Header is Null! Big Problem!\n\n");
  }
  DISK_BLOCK *FileToOpen;
  INT16 IndexAddress;

  //Get the Disk Sector where the Header has its top most index
  GetHeaderIndexSector(Header, &IndexAddress);
  
  DISK_BLOCK *Index = &Cache->Block[IndexAddress];
  FileToOpen = FindDirectory(Cache, Index, FileName);

  //If we can't locate file in the current directory create it
  if(FileToOpen == NULL){
    //aprintf("\n\nERROR: %s is not present\n\n", FileName);
    FileToOpen = osCreateFile(FileName, ReturnError, FILE);
  }

  pcb->open_file = FileToOpen;
  unsigned char OpenFileInode;
  GetParentInode(FileToOpen, &OpenFileInode);
  pcb->open_file_inode = OpenFileInode;

  (*Inode) = OpenFileInode;
  (*ReturnError) = ERR_SUCCESS;
   
}

/*
Write a block of data to the open file given by Inode.
*/
void osWriteFile(long Inode, long Index, char *WriteBuffer,
		 long *ReturnError){

  PROCESS_CONTROL_BLOCK *CurrentPCB = GetCurrentPCB();

  if(CurrentPCB->open_file_inode != Inode){
    aprintf("\n\nERROR: File Not Open\n\n");
    (*ReturnError) = ERR_BAD_PARAM;
    return;
  }

  DISK_BLOCK *Header = CurrentPCB->open_file;
  INT16 ThirdLevelSector;

  //Get the Disk Sector where the Header has its top most index
  GetHeaderIndexSector(Header, &ThirdLevelSector);
  
  //File Size in Bytes
  INT16 FileSize;
  GetFileSize(Header, &FileSize);

  INT16 FileBlocks = FileSize/PGSIZE;

  //Calculate the SubIndices
  INT16 Position1 = FileBlocks%8;
  FileBlocks = FileBlocks/8;
  INT16 Position2 = FileBlocks%8;
  FileBlocks = FileBlocks/8;
  INT16 Position3 = FileBlocks%8;

    
  DISK_BLOCK *ThirdLevelIndex = &Cache->Block[ThirdLevelSector]; 

  INT16 SecondLevelSector;
  DISK_BLOCK *SecondLevelIndex;
  
  GetSubIndex(ThirdLevelIndex, &SecondLevelSector, Position3*2);

  if(SecondLevelSector == 0){

    GetAvailableSector(Cache, &SecondLevelSector);
    SetIndexSpot(ThirdLevelIndex, Position3*2, SecondLevelSector);
    Cache->Modified[ThirdLevelSector] = 1;
  }

  SecondLevelIndex = &Cache->Block[SecondLevelSector];

  INT16 FirstLevelSector;
  DISK_BLOCK *FirstLevelIndex;
  
  GetSubIndex(SecondLevelIndex, &FirstLevelSector, Position2*2);

  if(FirstLevelSector == 0){
    GetAvailableSector(Cache, &FirstLevelSector);
    SetIndexSpot(SecondLevelIndex, Position2*2, FirstLevelSector);
    Cache->Modified[SecondLevelSector] = 1;
  }

  FirstLevelIndex = &Cache->Block[FirstLevelSector];

  
  //Now grab a disk sector and copy the Buffer into it
  INT16 DataSector;
  GetAvailableSector(Cache, &DataSector);
  SetIndexSpot(FirstLevelIndex, Position1*2, DataSector);
  Cache->Modified[FirstLevelSector] = 1;

  for(int i=0; i<PGSIZE; i++){
    Cache->Block[DataSector].Byte[i] = WriteBuffer[i];
  }
  Cache->Modified[DataSector] = 1;
  
  //Update file size in file header
  FileSize = FileSize + PGSIZE;
  SetFileLength(Header, FileSize);
  
  long DiskID = CurrentPCB->current_disk;
    
  //Atomic Section
  LockLocation(DISK_LOCK[DiskID]);

  //Add the sectors that have to be written back to the Disk Queue
  WriteBackChanges(CurrentPCB);

  StartDiskWrite(DiskID);
    
  UnlockLocation(DISK_LOCK[DiskID]);
  dispatcher();
  
}

void osCloseFile(long Inode, long *ReturnError){

  PROCESS_CONTROL_BLOCK *CurrentPCB = GetCurrentPCB();

  if(CurrentPCB->open_file_inode == Inode){
    CurrentPCB->open_file_inode = -1;
    CurrentPCB->open_file = NULL;
    (*ReturnError) = ERR_SUCCESS;
    return;
  }
  aprintf("\n\nERROR: Inode does not correspond to Open File\n\n");
  (*ReturnError) = ERR_BAD_PARAM;
}

void osReadFile(long Inode, long Index, char *ReadBuffer, long *ReturnError){

  PROCESS_CONTROL_BLOCK *CurrentPCB = GetCurrentPCB();

  if(CurrentPCB->open_file_inode != Inode){
    aprintf("\n\nERROR: File Not Open\n\n");
    (*ReturnError) = ERR_BAD_PARAM;
    return;
  }

  DISK_BLOCK *Header = CurrentPCB->open_file;
  long DiskID = CurrentPCB->current_disk;
  INT16 ThirdLevelSector;

  //Get the Disk Sector where the Header has its top most index
  GetHeaderIndexSector(Header, &ThirdLevelSector);

  //Calculate the SubIndices
  INT16 Position1 = Index%8;
  Index = Index/8;
  INT16 Position2 = Index%8;
  Index = Index/8;
  INT16 Position3 = Index%8;

  DISK_BLOCK *ThirdLevelIndex = &Cache->Block[ThirdLevelSector]; 

  INT16 SecondLevelSector;
  DISK_BLOCK *SecondLevelIndex;
  
  GetSubIndex(ThirdLevelIndex, &SecondLevelSector, Position3*2);
  SecondLevelIndex = &Cache->Block[SecondLevelSector];

  INT16 FirstLevelSector;
  DISK_BLOCK *FirstLevelIndex;
  
  GetSubIndex(SecondLevelIndex, &FirstLevelSector, Position2*2);
  FirstLevelIndex = &Cache->Block[FirstLevelSector];

  //Now grab the data sector.
  INT16 DataSector;
  GetSubIndex(FirstLevelIndex, &DataSector, Position1*2);
  
  osDiskReadRequest(DiskID, (long)DataSector, (long)ReadBuffer);

  (*ReturnError) = ERR_SUCCESS;
}

/*
Print out the contents of the current directory in a well formatted
way.
*/
void osPrintCurrentDirContents(long *ReturnError){

  PROCESS_CONTROL_BLOCK *CurrentPCB = GetCurrentPCB();

  DISK_BLOCK *Header = CurrentPCB->current_directory;

  if(Header == NULL){
    aprintf("\n\nERROR: No Current Directory\n\n");
    (*ReturnError) = ERR_BAD_PARAM;
    return;
  }

  INT16 HeaderIndex;
  GetHeaderIndexSector(Header, &HeaderIndex);

  DISK_BLOCK *Index = &Cache->Block[HeaderIndex];

  char Buffer[8];
  GetFileName(Header, Buffer);
  aprintf("Contents of Directory \t%s:\n", Buffer);

  DISK_BLOCK *SubFile = NULL;
  unsigned char SubInode;
  INT32 FileOrDir;
  INT32 CreationTime;
  INT16 FileSize = 0;

  aprintf("Inode,\tFileName,\tD/F,\tCreation Time,\tFile Size\n");
  
  for(INT16 i=0; i<PGSIZE; i=i+2){
    if(CheckIndexSpot(Index, i) == TRUE){
      SubFile = GetSubDirectoryHeader(Cache, Index, i);
      GetParentInode(SubFile, &SubInode);
      aprintf("  %x\t", SubInode);
      GetFileName(SubFile, Buffer);
      aprintf(" %s\t\t", Buffer);
      GetFileOrDirectory(SubFile, &FileOrDir);
      if(FileOrDir == FILE){
	aprintf(" F\t");
      }
      else{
	aprintf(" D\t");
      }
      GetCreationTime(SubFile, &CreationTime);
      aprintf(" %d\t\t", CreationTime);

      if(FileOrDir == FILE){
	GetFileSize(SubFile, &FileSize);
	aprintf(" %d \t", FileSize);
      }
      else{
	aprintf(" - \t");
      }
      aprintf("\n");
    }
  }
  
}
