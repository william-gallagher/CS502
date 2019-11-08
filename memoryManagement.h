#ifndef MEM_MANAGEMENT_H
#define MEM_MANAGEMENT_H


void InitializeFrameManager();
void GetAvailablePhysicalFrame(INT16 *Frame);
void SetValidBit(INT16 *PageEntry);


#endif //MEM_MANAGEMENT_H
