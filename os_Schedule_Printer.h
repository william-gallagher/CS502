//Prints
INT32 SVCPrints;
INT32 InterruptHandlerPrints;
INT32 SchedulerPrints;
INT32 MemoryPrints;

INT32 TestRunning;


void osPrintState(char* Action, long TargetPID, long CurrentPID);

void PrintSVC(long Arguments[], INT32 call_type);

void PrintInterrupt(INT32 DeviceID, INT32 Status);

void SetTestNumber(char TestName[]);

void SetPrintOptions(INT32 TestRunning);
