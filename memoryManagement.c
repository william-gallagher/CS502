/*
memoryManagement.c

This file holds all the functions that deal with virtual memory, page 
replacement, and related things.
*/

#include <string.h>
#include <stdlib.h>
#include "protos.h"
#include "process.h"
#include "readyQueue.h"
#include "timerQueue.h" 
#include "osGlobals.h"
#include "memoryManagement.h"
#include "diskManagement.h"
#include "diskQueue.h"
#include "osSchedulePrinter.h"

/*
The frame manager tracks which frames are in use. All the elements of the 
frame manager are set to 0 to indicate none of the frames are in use when
the OS is started.
*/
void InitializeFrameManager(){

  for(INT16 i=0; i<NUMBER_PHYSICAL_PAGES; i++){
    FrameManager[i] = 0;
  }
}

/*
Check the MSB in the Shadow Page Table. If set this indicates that the logical
page is on disk.
*/
INT32 CheckOnDisk(INT16 ShadowTableIndex, INT16 *ShadowPageTable){

  if((ShadowPageTable[ShadowTableIndex] & 0x8000) == 0){
    return FALSE;
  }
  return TRUE;
}

/*
Test to see is the page in the logical address has been swapped onto
at some previous point. This would be indicated by bit 15 being set to
1 in the shadow page table.
*/
INT32 CheckPreviouslyOnDisk(INT16 ShadowTableIndex,
			    INT16 *ShadowPageTable){


  if((ShadowPageTable[ShadowTableIndex] & 0x4000) == 0){
    return FALSE;
  }
  return TRUE;
}

/*
Simply checks the referenced bit and returns true or false. If the bit is
set then clear it.
*/
void TestReferenceBit(INT16 *PageTableEntry, INT32 *PageReferenced){

  if((*PageTableEntry & PTBL_REFERENCED_BIT) == 0){
    (*PageReferenced) = FALSE;
  }
  else{
    (*PageReferenced) = TRUE;
    //set Referenced Bit to 0
    (*PageTableEntry) = (*PageTableEntry) & (~PTBL_REFERENCED_BIT);
  }
}

/*
Used for getting the next frame to check when using LRU approximation
to find a frame to replace. Resets to 0 when NextFrame is 63. 
Note: NextFrame is a global variable.
*/
void GetNextFrame(){

  if(NextFrame == (NUMBER_PHYSICAL_PAGES-1)){
    NextFrame = 0;
  }
  else{
    NextFrame++;
  }
}

/*
Use the LRU Approximation algorithm to find a page to replace.
Look through Frame Manager to find a page that has not been referenced.

Remember: the frame manager is made up of INT32 elements.
byte 7 is for the in use flag
bytes 4-6 hold the PID that has the frame
bytes 0-3 hold the page table index
*/
void FindFrameToReplace(){

  INT16 *PageTable;
  INT16 PageTableIndex;
  INT16 *PageTableEntry;
  long FramePID;
  PROCESS_CONTROL_BLOCK *pcb;
  INT32 PageReferenced = TRUE;

  //We need to find out which process has the frame and then which
  //entry in the page table it is.
  //Then get the page table for that process and check the reference bit.
  while(PageReferenced == TRUE){

    GetNextFrame();
    FramePID = (FrameManager[NextFrame]>>16) & 0xFF;
    PageTableIndex = FrameManager[NextFrame] & 0xFFFF;
    pcb = GetPCB(FramePID);
    PageTable = pcb->page_table;
    PageTableEntry = &PageTable[PageTableIndex];

    TestReferenceBit(PageTableEntry, &PageReferenced);
  } 
 }
  
/*
When all the frames are in use we need to put the contents of one onto disk
and return the freed frame. The page table and the shadow page table of the
process that had the frame need to be updated as well.
*/
INT16 FreeUsedFrame(PROCESS_CONTROL_BLOCK *pcb1){

  //Use LRU Approximation to get a frame
  FindFrameToReplace();
  INT16 FrameToRemove = NextFrame;
  
  INT32 FrameContents = FrameManager[FrameToRemove];

  unsigned char FramePID = (FrameContents & 0x00FF0000) >> 16;

  PROCESS_CONTROL_BLOCK* pcb = GetPCB((long) FramePID);

  
  INT16 *ShadowPageTable = pcb->shadow_page_table;
  INT16 *PageTable = pcb->page_table;

  INT16 PageNumber = FrameContents & 0x00000FFFF;

  //Clear the valid bit
  PageTable[PageNumber] &= (~PTBL_VALID_BIT);
 
  INT16 DiskLocation;
  //Check to see if the logical page has been in the swap space before.
  //If yes then put it back where it was before.
  if(CheckPreviouslyOnDisk(PageNumber, ShadowPageTable) == FALSE){
    
    DiskLocation = NextSwapLocation;

    //fill the shadow page table
    ShadowPageTable[PageNumber] = 0x4000; //set the prev in use bit 
    ShadowPageTable[PageNumber] = ShadowPageTable[PageNumber] + DiskLocation;
    NextSwapLocation++;

  }
  else{

    DiskLocation = ShadowPageTable[PageNumber] & 0x0FFF;
  }

  ShadowPageTable[PageNumber] |= 0x8000;  //set in use bit

  //We have to get data from physical memory and put on the disk
  char DataBuffer[16];
  Z502ReadPhysicalMemory(FrameToRemove, DataBuffer);
 
  osDiskWriteRequest(SWAP_DISK, DiskLocation, (long)DataBuffer);
  
  return FrameToRemove;
}

/*
Frame Manager is made up of 32 bit chunks.
The upper 8 bits is set to 1 when in use. Otherwise set to 0.
Bits 16 to 23 store the PID that is using the current frame.
Bits 0 to 15 hold the Page Table Index that the frame is used for.
*/
void GetPhysicalFrame(INT16 *Frame, PROCESS_CONTROL_BLOCK *pcb, INT16 PageIndex){

  long PID = pcb->idnum;
  INT16 FrameIndex = -1;
  
  for(int i=0; i<NUMBER_PHYSICAL_PAGES; i++){

    if((FrameManager[i] & 0xff000000) == 0){
      FrameIndex = i;
      break;
     }
  }

  //If FrameIndex still = -1 then there are no more physical frames.
  if(FrameIndex == -1){

    FrameIndex = FreeUsedFrame(pcb);
  }

  FrameManager[FrameIndex] = 0x01000000; //signify in use
  FrameManager[FrameIndex] = FrameManager[FrameIndex] + ((PID<<16) & 0x00ff0000);
  FrameManager[FrameIndex] = FrameManager[FrameIndex] + PageIndex;

  (*Frame) = FrameIndex;
  return;
  
}


void SetValidBit(INT16 *TableEntry){

  (*TableEntry) |= PTBL_VALID_BIT;
}

void ClearValidBit(INT16 *TableEntry){

  (*TableEntry) &= (~PTBL_VALID_BIT);
}

INT32 CheckValidBit(INT16 TableEntry){

  //Check to see if valid bit is set
  //Remember PTLB_VALID_BIT is 0x8000
  if(TableEntry & PTBL_VALID_BIT){
    return TRUE;
  }
  return FALSE;
}

/*
Handle the shared area tests.
*/
void InitializeSharedArea(long StartingAddress, long PagesInSharedArea,
			   char *AreaTag, long *OurSharedID, long
			  *ReturnError){

  PROCESS_CONTROL_BLOCK *pcb = GetCurrentPCB();
  INT16 *PageTable = pcb->page_table;

  //Let's make sure the starting address is on a mod 16 boundary
  if(StartingAddress % PGSIZE != 0){
    aprintf("\n\nERROR: Shared Area starting address is not mod 16\n\n");
    (*ReturnError) = ERR_BAD_PARAM;
    return;
  }

  INT16 StartPage = StartingAddress / PGSIZE;

  //Get frames for all the share pages starting at SharedAreaStartFrame
  for(INT32 i=0; i<PagesInSharedArea; i++){

    PageTable[StartPage + i] = (SharedAreaStartFrame + i) + PTBL_VALID_BIT;
  }

  (*OurSharedID) = SharedIDCount;
  SharedIDCount++;
  (*ReturnError) = ERR_SUCCESS;
}

/*
Handle the Interrupt Handler. Usually this is finding a frame to back the
requested memory page. If the request does not align on a mod 4 boundary
terminate the program.
*/
void HandleFaultHandler(INT32 DeviceID, INT32 Status){
   PrintFault(DeviceID, Status);
 
  //get the current page table
  PROCESS_CONTROL_BLOCK *CurrentPCB = GetCurrentPCB();

  INT16 *PageTable = (INT16 *)CurrentPCB->page_table;
  INT16 *ShadowPageTable = (INT16 *)CurrentPCB->shadow_page_table;
  INT16 Index = (INT16) Status;

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

  //If the data was on the disk we need to get the disk sector from the
  //shadow page table.
  if(OnDisk == TRUE){

    INT16 DiskSector = ShadowPageTable[Index] & 0x0FFF;

    char DataBuffer[16];

    osDiskReadRequest(SWAP_DISK, DiskSector, (long)DataBuffer);

    //Put data from disk into proper memory address
    INT16 Frame = (PageTable[Index] & 0x0FFF);
    Z502WritePhysicalMemory(Frame, DataBuffer);

    //Indicate data can be found in memory rather than on disk
    ShadowPageTable[Index] &= 0x7FFF;
   }
  osPrintMemoryState();
}
