/*
memoryManagement.h 

This file includes the prototypes and globals that deal with memory
management. It also includes functions for handling the Fault Handler.
*/

#ifndef MEM_MANAGEMENT_H
#define MEM_MANAGEMENT_H

//The tests expect the swap disk to be 1
#define SWAP_DISK 1
INT16 NextSwapLocation;

//This can be anywhere
#define SharedAreaStartFrame 0x20

long SharedIDCount;

void InitializeFrameManager();
void GetPhysicalFrame(INT16 *Frame, PROCESS_CONTROL_BLOCK *pcb,
		      INT16 PageIndex);
void SetValidBit(INT16 *PageEntry);
INT32 CheckValidBit(INT16 TableEntry);
void NoFrames(PROCESS_CONTROL_BLOCK *CurrentPCB, INT16 Index);
INT32 CheckOnDisk(INT16 ShadowTableIndex, INT16 *ShadowPageTable);
void InitializeSharedArea(long StartingAddress, long PagesInSharedArea,
			   char *AreaTag, long *OurSharedID, long
			  *ReturnError);
void HandleFaultHandler(INT32 DeviceID, INT32 Status);


#endif //MEM_MANAGEMENT_H
