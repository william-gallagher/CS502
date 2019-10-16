#ifndef DISK_QUEUE_H
#define DISK_QUEUE_H

#include "osGlobals.h"
#include "global.h"


void CreateDiskQueue(INT32 DiskNumber);
void InitializeDiskLocks();
void AddToDiskQueue(long disk_id, DQ_ELEMENT* dqe);
void CheckDiskStatus(long disk_id, long* status);
DQ_ELEMENT* RemoveFromDiskQueueHead(long DiskID);
DQ_ELEMENT* CheckDiskQueue(long DiskID);
void osDiskReadRequest(long DiskID, long DiskSector, long DiskAddress);
void osDiskWriteRequest(long DiskID, long DiskSector, long DiskAddress);
void osCheckDiskRequest(long DiskID, long DiskSector);
void HandleDiskInterrupt(long DiskID);

#endif //DISK_QUEUE_H


			
