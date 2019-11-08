/*
osGlobal.h

This include file has all the OS wide structs and defines.

*/

#ifndef OS_GLOBAL_H
#define OS_GLOBAL_H

#define MAX_INT ((UINT32)~0 >> 1)

//Some OS wide limits
#define MAXPROCESSES 15
#define MAX_MESSAGE_LENGTH 500
#define MAX_MESSAGES_IN_BUFFER 10
#define MAX_NAME_LENGTH 100

//Define the possible states that a process can be in
#define SUSPENDED 0
#define RUNNING 1
#define READY 2
#define TIMER 3
#define DISK 4
#define WAITING_TO_SUSPEND_TIMER 5
#define WAITING_TO_SUSPEND_DISK 6
#define SUSPENDED_WAITING_FOR_MESSAGE 7

//Define whether a PCB is in use or not
#define FREE 0
#define IN_USE 1

struct{

  unsigned char inode;
  char name[7];
  unsigned char creation_time2;
  unsigned char creation_time1;
  unsigned char creation_time0;
  unsigned char file_description; //maybe break up into fields?
  unsigned short index_location;
  unsigned short file_size;
} typedef FILE_HEADER;

//define
struct{
  unsigned char Byte[16];
} typedef DISK_BLOCK;

struct{
  DISK_BLOCK Block[2048];
  unsigned char Modified[2048];
} typedef DISK_CACHE;

DISK_CACHE *Cache;

//The struct that holds all the information about a process
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
  long current_disk;
  DISK_BLOCK *current_directory;
  DISK_BLOCK *current_index; ///Not used right now???
  unsigned int open_file_inode;
  DISK_BLOCK *open_file;
  DISK_CACHE *cache;
  void* page_table;
} PROCESS_CONTROL_BLOCK;

//The array that holds all the PCBs
PROCESS_CONTROL_BLOCK PCB[MAXPROCESSES];
INT32 nextid;

/*
The struct that holds all the information necessary when a process is 
on the Ready Queue.
*/
typedef struct{
  long context;
  long PID;
  void* PCB;
}RQ_ELEMENT;

/*
The struct that holds all the information necessary when a process is 
on the Timer Queue.
*/
typedef struct{
  long wakeup_time;
  long context;
  long PID;
  void* PCB;
}TQ_ELEMENT;

/*
The struct that holds all the information necessary when a process is 
on the Message Buffer.
*/
typedef struct{
  long target_pid;
  long sender_pid;
  char message[MAX_MESSAGE_LENGTH];
  long message_length;
}MQ_ELEMENT;

/*
The struct that holds all the information necessary when a process is 
on the Disk Queue.
*/
typedef struct{

  long disk_action;  
  long disk_id;
  long disk_sector;
  long disk_address;
  
  long context;
  long PID;
  void* PCB;
}DQ_ELEMENT;

//These states are used for the disk_action field for elements in the
//Disk Queue. This flag is used to indicate whether a read or write is to
//be performed
#define READ_DISK 0
#define WRITE_DISK 1

//Here are the IDs for the Queues and Buffers that use the Queue Manager.
INT32 ready_queue_id;
INT32 timer_queue_id;
INT32 message_buffer_id;
INT32 disk_queue[MAX_NUMBER_OF_DISKS];

//Here are the locks for the different Queues, Buffers and shared memory.
#define READY_LOCK MEMORY_INTERLOCK_BASE
#define TIMER_LOCK  READY_LOCK + 1
#define MESSAGE_LOCK TIMER_LOCK + 1
#define PROC_LOCK_BASE TIMER_LOCK + 1
#define DISK_LOCK_BASE PROC_LOCK_BASE + MAXPROCESSES
INT32 DISK_LOCK[MAX_NUMBER_OF_DISKS];
//Note: The disk queues require a lock for each queue. So dont start
//using DISK_LOCK_BASE + 1 !!!


//Define multiprocessor or uniprocessor
#define MULTI 1
#define UNI 0

//Multiprocessor Flag
INT32 M;

unsigned int FrameManager[NUMBER_PHYSICAL_PAGES];

#endif //OS_GLOBAL_H
