/*



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
//#include "osSchedulePrinter.h"

void InitializeFrameManager(){

  for(INT16 i=0; i<16; i++){
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
at some previous point. This would be indicated by bit 15 being set t0
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
All the Frames are full and we need to put the contents of one onto disk
*/
INT16 FreeUsedFrame(PROCESS_CONTROL_BLOCK *pcb){

  INT16 *ShadowPageTable = pcb->shadow_page_table;
  INT16 *PageTable = pcb->page_table;

  //get a random frame. Eventually need a better way to do this.
  INT16 FrameToRemove = random()%64;

  INT32 FrameContents = FrameManager[FrameToRemove];

  unsigned int FramePID = (FrameContents & 0x00FF0000) >> 16;

  INT16 PageNumber = FrameContents & 0x0000FFFF;

  //Clear the valid bit
  PageTable[PageNumber] = 0;
  
  if(pcb->cache == NULL){
    //pcb->cache = CreateDiskCache();
    pcb->cache = malloc(10000000);
    pcb->next_swap_location = 0x0000;
  }
  INT16 CacheLine;
  //Check to see if the logical page has been in the swap space before.
  //If yes then put it back where it was.
  if(CheckPreviouslyOnDisk(PageNumber, ShadowPageTable) == FALSE){
 
    CacheLine = pcb->next_swap_location;

    //fill the shadow page table
    ShadowPageTable[PageNumber] = 0x4000; //set the prev in use bit 
    // ShadowPageTable[PageNumber] = ShadowPageTable[PageNumber] + ((FramePID & 0x0F)<<11);
    ShadowPageTable[PageNumber] = ShadowPageTable[PageNumber] + CacheLine;

    //  aprintf("here is whats store in shadow pg %x\n", ShadowPageTable[PageNumber]);
    //  aprintf("new swap location is %x\n", pcb->next_swap_location);
    pcb->next_swap_location++;

  }
  else{
    CacheLine = ShadowPageTable[PageNumber] & 0x0FFF;
  }

  ShadowPageTable[PageNumber] |= 0x8000;  //set in use bit
    
  //have to get data from physical memory and put into cache
  char DataBuffer[16];
  Z502ReadPhysicalMemory(FrameToRemove, DataBuffer);
  
  for(int i=0; i<16; i++){
    // aprintf("%x ", DataBuffer[i]);
  }

  //copy to cache
  for(int i=0; i<16; i++){
    pcb->cache->Block[CacheLine].Byte[i] = DataBuffer[i];
  }
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
  FrameManager[FrameIndex] = FrameManager[FrameIndex] + (PID & 0x00ff0000);
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
    //aprintf("Checking %d returning true\n", TableEntry);
    return TRUE;
  }
  //aprintf("Checking %d. Returning false\n", TableEntry);
  return FALSE;
}

/*
We need to find a frame for Page Table entry Index

void NoFrames(PROCESS_CONTROL_BLOCK *CurrentPCB, INT16 Index){

  //First see if the Logical Page is already on the disk somewhere.
  //This involves checking the Shadow Page Table
  INT16 *ShadowPageTable  = (INT16 *)CurrentPCB->shadow_page_table;
  INT16 *PageTable = (INT16 *)CurrentPCB->page_table;
  long CurrentPID = CurrentPCB->idnum;

  //get a random frame
  INT16 FrameToRemove = random()%64;

  INT32 FrameContents = FrameManager[FrameToRemove];

  unsigned int FramePID = (FrameContents & 0x00FF0000) >> 16;
  aprintf("Here is the PID %d\n", FramePID);

  INT16 PageNumber = FrameContents & 0x0000FFFF;
  aprintf("Here is the Page Number associated with the Frame : %d\n", PageNumber);
  
  if(CurrentPCB->cache == NULL){
    CurrentPCB->cache = CreateDiskCache();
    CurrentPCB->next_swap_location = 0x600;
  }

  INT16 CacheLine = CurrentPCB->next_swap_location;

  //fill the shadow page table
  ShadowPageTable[PageNumber] = 0x8000; //set the in use bit
  ShadowPageTable[PageNumber] = ShadowPageTable[PageNumber] + ((FramePID & 0x0F)<<11);
  ShadowPageTable[PageNumber] = ShadowPageTable[PageNumber] + CacheLine;
  CurrentPCB->next_swap_location++;

  //have to get data from physical memory and put into cache
  char DataBuffer[16];
  Z502ReadPhysicalMemory(FrameToRemove, DataBuffer);

  for(int i=0; i<16; i++){
    aprintf("%x ", DataBuffer[i]);
  }

  //copy to cache
  for(int i=0; i<16; i++){
    CurrentPCB->cache->Block[CacheLine].Byte[i] = DataBuffer[i];
  }

  //update page table
  PageTable[Index] =  FrameToRemove;


  //update Frame Manager
  FrameManager[FrameToRemove] = FrameManager[FrameToRemove] + (CurrentPID & 0x00ff0000);
  FrameManager[FrameToRemove] = FrameManager[FrameToRemove] + Index;

}
*/
