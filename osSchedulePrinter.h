/*
osSchedulePrinter.h

This file is the include file for everything to do with Schedule Printer
and the variables that track the prints for the different states.
*/

#ifndef SCHEDULE_PRINTER_H
#define SCHEDULE_PRINTER_H

//These variables track the number of prints.
//Different tests set these numbers to different values
INT32 SVCPrints;
INT32 InterruptHandlerPrints;
INT32 SchedulerPrints;
INT32 MemoryPrints;
INT32 TestRunning;


void osPrintState(char* Action, long TargetPID, long CurrentPID);
void PrintSVC(long Arguments[], INT32 call_type);
void PrintInterrupt(INT32 DeviceID, INT32 Status);
void SetTestNumber(char TestName[]);
long GetTestName(char TestName[]);
void SetPrintOptions(INT32 TestRunning);

#endif //SCHEDULE_PRINTER_H
