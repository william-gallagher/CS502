//Prints
INT32 SVCPrints;
INT32 InterruptHandlerPrints;
INT32 SchedulePrints;
INT32 MemoryPrints;

INT32 TestRunning;


void osPrintState(char* Action);

void PrintSVC(long Arguments[], INT32 call_type);

void PrintInterrupt(INT32 DeviceID, INT32 Status);
