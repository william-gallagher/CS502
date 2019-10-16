/*
The include file for all the OS wide structures and defines

*/




#ifndef A_H
#define A_H



//Should there be a max number of processes????
#define MAXPROCESSES 15

//No idea what this should be
#define MAX_MESSAGE_LENGTH 500

#define MAX_MESSAGES_IN_BUFFER 10

#define MAX_NAME_LENGTH 100

//maybe #define ACTIVE is better than RUNNING???
//define the states of a process
#define SUSPENDED 0
#define RUNNING 1
#define READY 2
#define TIMER 3
#define DISK 4
#define WAITING_TO_SUSPEND_TIMER 5
#define WAITING_TO_SUSPEND_DISK 6
#define SUSPENDED_WAITING_FOR_MESSAGE 7

//define whether a PCB is in use or not
#define FREE 0
#define IN_USE 1

//should priority be an INT32 or long??????
typedef struct{
  INT32 in_use;
  INT32 parent;
  long idnum;
  long context;
  char name[MAX_NAME_LENGTH];
  INT32 priority;
  INT32 state;
  INT32 waiting_for_message;
  INT32 LOCK;
  void* queue_ptr;
} PROCESS_CONTROL_BLOCK;


PROCESS_CONTROL_BLOCK PCB[MAXPROCESSES];
INT32 nextid;

//make this outside a function for now to allow everyone to use
//PROCESSES_INFORMATION* PRO_INFO;

//suspended queue
typedef struct{
  long context;
}SQ_ELEMENT;


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

#define READ_DISK 0
#define WRITE_DISK 1

//struct for holding context while disk works on reading and writing
typedef struct{

  long disk_action;//write == 1 read == 0
  
  long disk_id;
  long disk_sector;
  long disk_address;
  
  long context;
  long PID;
  void* PCB;
}DQ_ELEMENT;

INT32 disk_queue[MAX_NUMBER_OF_DISKS];

INT32 DISK_LOCK[MAX_NUMBER_OF_DISKS];

//store the contexts of processes while they wait for disks do read/write
DQ_ELEMENT* disk_process[MAX_NUMBER_OF_DISKS];

#define PROC_LOCK_BASE  TIMER_LOCK + 1

//Message Queue
int message_queue_id;
#define MESSAGE_LOCK MEMORY_INTERLOCK_BASE+30

typedef struct{

  long target_pid;
  long sender_pid;
  char message[MAX_MESSAGE_LENGTH];
  long message_length;
}MQ_ELEMENT;


#endif //end my guard
