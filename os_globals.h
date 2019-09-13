/*
The include file for all the OS wide structures and defines

*/


//Should there be a max number of processes????
#define MAXPROCESSES 15

#ifndef A_H
#define A_H

//name has limit of 100 for now. Seems like a hack
//should priority be an INT32 or long??????
typedef struct{
  INT32 in_use;
  INT32 parent;
  long idnum;
  long context;
  char name[100];
  INT32 priority;
  INT32 suspended;
  void* queue_ptr;
} PROCESS_CONTROL_BLOCK;

typedef struct{
  INT32 nextid;
  INT32 current;
  PROCESS_CONTROL_BLOCK PCB[MAXPROCESSES];
} PROCESSES_INFORMATION;



//make this outside a function for now to allow everyone to use
PROCESSES_INFORMATION* PRO_INFO;

//suspended queue
typedef struct{
  long context;
}SQ_ELEMENT;

#define SUSPEND_LOCK MEMORY_INTERLOCK_BASE + 2;
int suspend_queue_id;




//ready queue

typedef struct{
  long context;
}RQ_ELEMENT;

//everything for Ready Queue
int ready_queue_id;
#define READY_LOCK MEMORY_INTERLOCK_BASE


//The struct that holds the data for processes in the timer queue
typedef struct{
	long context;
	long wakeup_time;
}TQ_ELEMENT;

int timer_queue_id;
#define TIMER_LOCK  MEMORY_INTERLOCK_BASE + 1

//struct for holding context while disk works on reading and writing
typedef struct{
  long context;
}DQ_ELEMENT;

int disk_queue[MAX_NUMBER_OF_DISKS];

INT32 DISK_LOCK[MAX_NUMBER_OF_DISKS];

#endif //end my guard
