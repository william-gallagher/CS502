//Include file for everything to do with the timer queue

#include "global.h"

//ready queue

typedef struct{
  long context;
}RQ_ELEMENT;

//everything for Ready Queue
int ready_queue_id;
#define READY_LOCK MEMORY_INTERLOCK_BASE + 1

int CreateReadyQueue();

//can't figure out how call works right now
void dispatcher();

//The struct that holds the data for processes in the timer queue
typedef struct{
	long context;
	long wakeup_time;
}TQ_ELEMENT;

int timer_queue_id;
#define TIMER_LOCK  MEMORY_INTERLOCK_BASE

void AddToReadyQueue(long context);

void StartTimer(long sleep_time);

void HandleTimerInterrupt();

//really doesnt need an int return
//use functions in QueueManager.c to create a timer for storing 
int CreateTimerQueue();

//Just use this file for now for Disk Queues as well

//struct for holding context while disk works on reading and writing
typedef struct{
  long context;
}DQ_ELEMENT;

int disk_queue[MAX_NUMBER_OF_DISKS];

//Does this have to have int return????
int CreateDiskQueue(int disk_number);
