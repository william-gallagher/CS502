#ifndef DQ
#define DQ

#include "os_globals.h"

//Does this have to have int return????
int CreateDiskQueue(int disk_number);

void InitializeDiskLocks();

void AddToDiskQueue(long disk_id, DQ_ELEMENT* dqe);


void CheckDiskStatus(long disk_id, long* status);

DQ_ELEMENT* RemoveFromDiskQueueHead(long DiskID);

DQ_ELEMENT* CheckDiskQueue(long DiskID);

void osDiskReadRequest(long DiskID, long DiskSector, long DiskAddress);

void osDiskWriteRequest(long DiskID, long DiskSector, long DiskAddress);

void osCheckDiskRequest(long DiskID, long DiskSector);

#endif


			
