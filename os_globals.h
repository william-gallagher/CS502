/*
The include file for all the OS wide structures and defines

*/


//Should there be a max number of processes????
#define MAXPROCESSES 15

#ifndef A_H
#define A_H

//maybe #define ACTIVE is better than RUNNING???
//define the states of a process
#define SUSPENDED 0
#define RUNNING 1
#define READY 2
#define TIMER 3
#define DISK 4
#define WAITING_TO_SUSPEND_TIMER 5
#define WAITING_TO_SUSPEND_DISK 6

//name has limit of 100 for now. Seems like a hack
//should priority be an INT32 or long??????
typedef struct{
  INT32 in_use;
  INT32 parent;
  long idnum;
  long context;
  char name[100];
  INT32 priority;
  INT32 state;
  INT32 LOCK;
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

//make sure this lock isnt use. Might be
//#define SUSPEND_LOCK MEMORY_INTERLOCK_BASE + 2;
int suspend_queue_id;




//ready queue

typedef struct{
  long context;
  long PID;
  void* PCB;
}RQ_ELEMENT;

//everything for Ready Queue
int ready_queue_id;
#define READY_LOCK MEMORY_INTERLOCK_BASE+10


//The struct that holds the data for processes in the timer queue
typedef struct{
  long wakeup_time;
  long context;
  long PID;
  void* PCB;
}TQ_ELEMENT;

int timer_queue_id;
#define TIMER_LOCK  READY_LOCK + 1

//struct for holding context while disk works on reading and writing
typedef struct{

  long disk_id;
  long disk_sector;
  long disk_address;
  
  long context;
  long PID;
  void* PCB;
}DQ_ELEMENT;

int disk_queue[MAX_NUMBER_OF_DISKS];

INT32 DISK_LOCK[MAX_NUMBER_OF_DISKS];

//store the contexts of processes while they wait for disks do read/write
DQ_ELEMENT* disk_process[MAX_NUMBER_OF_DISKS];

#define PROC_LOCK_BASE  TIMER_LOCK + 1



#endif //end my guard
