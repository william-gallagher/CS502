#include "global.h"
#include "os_globals.h"


void dispatcher();

int CreateReadyQueue();
long RemoveFromReadyQueue(PROCESS_CONTROL_BLOCK* process);

long ChangePriorityInReadyQueue(PROCESS_CONTROL_BLOCK* process, INT32 NewPriority);

void AddToReadyQueue(long context, long PID, void* PCB);

