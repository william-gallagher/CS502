#ifndef MEM_MANAGEMENT_H
#define MEM_MANAGEMENT_H


void InitializeFrameManager();
void GetPhysicalFrame(INT16 *Frame, PROCESS_CONTROL_BLOCK *pcb, INT16 PageIndex);
void SetValidBit(INT16 *PageEntry);
INT32 CheckValidBit(INT16 TableEntry);
void NoFrames(PROCESS_CONTROL_BLOCK *CurrentPCB, INT16 Index);
INT32 CheckOnDisk(INT16 ShadowTableIndex, INT16 *ShadowPageTable);

#endif //MEM_MANAGEMENT_H
